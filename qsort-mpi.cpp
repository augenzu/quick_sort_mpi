#include "qsort-mpi.h"
#include <iostream>
#include <cstring>

// integer log2(num); works only if num == 2**deg && deg in [0, 30]
// (yes, this is a crutch but this is enough for our goals)
int
log2(int num)
{
    enum { MAX_DEG = 30 };

    int deg = MAX_DEG;
    int powered = (1 << deg);

    while (powered && (powered & num) != powered) {
        powered >>= 1;
        --deg;    
    }

    return deg;
}

int 
cmp(const void *a, const void *b)
{
    int *ia = (int *) a;
    int *ib = (int *) b;
    if (*ia < *ib) {
        return -1;
    } else if (*ia == *ib) {
        return 0;
    } else {
        return 1;
    }
}

// merge two ascending arrays to one (dst)
void
merge(int *lhs, int lsz, int *rhs, int rsz, int *dst)
{
    int sz = lsz + rsz;
    int i = 0;
    int li = 0, ri = 0;

    for (; li < lsz && ri < rsz && i < sz; ++i) {
        if (lhs[li] < rhs[ri]) {
            dst[i] = lhs[li++];
        } else {
            dst[i] = rhs[ri++];
        }
    }
    if (li < lsz) {
        for (; i < sz; ++i) {
            dst[i] = lhs[li++];
        }
    } else if (ri < rsz) {
        for (; i < sz; ++i) {
            dst[i] = rhs[ri++];
        }
    }
}

void
q_sort(int *orig_data, int orig_sz)
{   
    MPI_Init(NULL, NULL);

    int comm_sz, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); 

    // parts of original array for every process
    int *data = NULL;
    int sz = 0;
    
    if (rank == 0) {
        int part_sz = orig_sz / comm_sz;
        int shift = orig_sz - (part_sz * comm_sz);
        std::cout << "orig_sz: " << orig_sz << "; comm_sz: " << comm_sz << "; part_sz: " 
                << part_sz << "; shift: " << shift << std::endl;
        std::cout << "Original array:" << std::endl;
        for (int i = 0; i < orig_sz; ++i) {
            std::cout << orig_data[i] << " ";
        }
        std::cout << std::endl << "Start initial array parts sending..." << std::endl;

        // send original array parts to other processes [1..comm_sz)
        for (int i = 1; i < comm_sz; ++i) {
            std::cout << "Send part " << i << "; start_index: " << shift + part_sz * i << std::endl;
            int dst = i;
            int tag = 0;
            MPI_Send(orig_data + shift + part_sz * i, part_sz, MPI_INT, dst, tag, MPI_COMM_WORLD);
        }

        // need this to work with 0's array part the same as we do this 
        // with other processes' parts
        sz = part_sz + shift;
        data = (int *) calloc(sz, sizeof(int));
        memcpy(data, orig_data, sz * sizeof(int));     

        std::cout << "Array part #" << rank << ": " << data[0] << " .. " << data[sz - 1] << std::endl;
    } else {
        MPI_Status status;
        // need to know incoming array size (i. e. part_sz from process 0)
        int src = 0;
        int tag = 0;
        MPI_Probe(src, tag, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_INT, &sz);

        // allocate memory for incoming array
        data = (int *) calloc(sz, sizeof(int));

        // recieve array
        MPI_Recv(data, sz, MPI_INT, src, tag, MPI_COMM_WORLD, &status);

        std::cout << "Array part #" << rank << ": " << data[0] << " .. " << data[sz - 1] << std::endl;
    }

    // sort each array part
    qsort(data, sz, sizeof(int), cmp);
    std::cout << "After sorting initial array parts: " << data[0] << " .. " << data[sz - 1] << std::endl;

    int deg = log2(comm_sz);

    // main sorting cycle
    for (int i = deg; i > 0; --i) {
        int step = (1 << i);
        int mask = ~(step - 1);
        int middle = 0;  // i.e. pivot - value to split array elements bys

        if ((rank & mask) == rank) {  // 'main' process in a 'group'
            // middle value to slit 'group' array elements by
            middle = (data[0] + data[sz - 1]) / 2;
            // send middle value to other 'group' members
            std::cout << "LE bit: #" << i << "; before swapping. I'm the main proc in group; rank: " 
                    << rank << "; orig middle: " << middle << std::endl
                    << "Start sending to other in group..." << std::endl;
            for (int dst = rank + 1; dst < rank + step; ++dst) {
                int tag = i;
                MPI_Send(&middle, 1, MPI_INT, dst, tag, MPI_COMM_WORLD);
            }
        } else {  // all the processes in a 'group' except of 'main'
            // recieve middle value to slit array elements by
            int src = (rank & mask);
            int tag = i;
            MPI_Status status;
            MPI_Recv(&middle, 1, MPI_INT, src, tag, MPI_COMM_WORLD, &status);
            std::cout << "LE bit: #" << i << "; before swapping. I'm not a main proc; main's rank: " 
                        << src << "; rcvd middle: " << middle << "; my rank: " << rank << std::endl;
        }

        // how much array elments are less then middle value
        int lt_middle_sz = 0;
        for (int i = 0; i < sz; ++i) {
            if (data[i] < middle) {
                ++lt_middle_sz;
            }
        }
        int *lt_middle = lt_middle_sz ? (int *) calloc(lt_middle_sz, sizeof(int)) : NULL;
        int ge_middle_sz = sz - lt_middle_sz;
        int *ge_middle = ge_middle_sz ? (int *) calloc(ge_middle_sz, sizeof(int)) : NULL;

        // split array by middle value
        int lt_i = 0, ge_i = 0;
        for (int i = 0; i < sz; ++i) {
            if (data[i] < middle) {
                lt_middle[lt_i++] = data[i];
            } else {
                ge_middle[ge_i++] = data[i];
            }
        }

        free(data);
        data = NULL;
        sz = 0;

        // need this to decide this process sends or recieves first
        int one_bit = (rank & step);

        // process with one_bit == 1 sends lt_middle array to its partner
        // (partner - process with the same rank number except of one_bit == 0)
        // and then recieves from it its ge_middle array;
        // the partner, therefore, does the opposite:
        // first, it recieves lt_middle array, and then sends ge_middle array
        if (one_bit) {
            int partner_rank = (rank & ~step);
            int tag = deg + i;
            // sent lt_middle to partner
            MPI_Send(lt_middle, lt_middle_sz, MPI_INT, partner_rank, tag, MPI_COMM_WORLD);
            // need to know incoming partner's ge_middle array size (i. e. partner_ge_middle_sz)
            MPI_Status status;
            MPI_Probe(partner_rank, tag, MPI_COMM_WORLD, &status);
            int partner_ge_middle_sz = 0;
            MPI_Get_count(&status, MPI_INT, &partner_ge_middle_sz);
            int *partner_ge_middle = (int *) calloc(partner_ge_middle_sz, sizeof(int));
            // recieve ge_middle from partner
            MPI_Recv(partner_ge_middle, partner_ge_middle_sz, 
                    MPI_INT, partner_rank, tag, MPI_COMM_WORLD, &status);
            // collect new data from ge_middle & partner_ge_middle
            sz = ge_middle_sz + partner_ge_middle_sz;
            data = (int *) calloc(sz, sizeof(int));
            // merge ge_middle with partner_ge_middle to data
            merge(ge_middle, ge_middle_sz, partner_ge_middle, partner_ge_middle_sz, data);
            // remove auxilary array
            free(partner_ge_middle);

            std::cout << "LE bit: #" << i << "; after swapping. My one_bit == 1; rank: " 
                    << rank << "; middle: " << middle << "; data: " << data[0] << " .. " 
                    << data[sz - 1] << std::endl;
        } else {
            int partner_rank = (rank | step);
            int tag = deg + i;
            // need to know incoming partner's lt_middle array size (i. e. partner_lt_middle_sz)
            MPI_Status status;
            MPI_Probe(partner_rank, tag, MPI_COMM_WORLD, &status);
            int partner_lt_middle_sz = 0;
            MPI_Get_count(&status, MPI_INT, &partner_lt_middle_sz);
            int *partner_lt_middle = (int *) calloc(partner_lt_middle_sz, sizeof(int));
            // recieve lt_middle from partner
            MPI_Recv(partner_lt_middle, partner_lt_middle_sz, 
                    MPI_INT, partner_rank, tag, MPI_COMM_WORLD, &status);
            // sent ge_middle to partner
            MPI_Send(ge_middle, ge_middle_sz, MPI_INT, partner_rank, tag, MPI_COMM_WORLD);

            // collect new data from lt_middle & partner_lt_middle
            sz = lt_middle_sz + partner_lt_middle_sz;
            data = (int *) calloc(sz, sizeof(int));
            // merge lt_middle with partner_lt_middle to data
            merge(lt_middle, lt_middle_sz, partner_lt_middle, partner_lt_middle_sz, data);
            // remove auxilary array
            free(partner_lt_middle);

            std::cout << "LE bit: #" << i << "; after swapping. My one_bit == 0; rank: " 
                    << rank << "; middle: " << middle << "; data: " << data[0] << " .. " 
                    << data[sz - 1] << std::endl;
        }

        // remove auxilary arrays after splitting, swapping and merging
        free(lt_middle);
        free(ge_middle);
    }

    MPI_Finalize();
}
