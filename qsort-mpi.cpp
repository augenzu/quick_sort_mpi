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

// splits src array values to which are less then split value
// and which are greater or equal to it (to lt_split and ge_split arrays)
void
split_by_value(int split, const int *src, int sz, 
        int **lt_split, int *lt_split_sz, 
        int **ge_split, int *ge_split_sz)
{
    // how much array elments are less then split value
    *lt_split_sz = 0;
    for (int i = 0; i < sz; ++i) {
        if (src[i] < split) {
            ++*lt_split_sz;
        }
    }
    *ge_split_sz = sz - *lt_split_sz;
        
    *lt_split = *lt_split_sz ? (int *) calloc(*lt_split_sz, sizeof(int)) : NULL;
    *ge_split = *ge_split_sz ? (int *) calloc(*ge_split_sz, sizeof(int)) : NULL;

    // split src by split value
    int lt_i = 0, ge_i = 0;
    for (int i = 0; i < sz; ++i) {
        if (src[i] < split) {
            (*lt_split)[lt_i++] = src[i];
        } else {
            (*ge_split)[ge_i++] = src[i];
        }
    }
}

// merges two ascending arrays to one (dst)
void
merge(const int *lhs, int lsz, const int *rhs, int rsz, int *dst)
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

        std::cout << "Array part " << rank << ": " << data[0] << " .. " << data[sz - 1] << std::endl;
    }

    // sort each array part
    qsort(data, sz, sizeof(int), cmp);
    std::cout << "After sorting initial array parts; array part " << rank 
            << ": " << data[0] << " .. " << data[sz - 1] << std::endl;

    int deg = log2(comm_sz);

    // main sorting cycle
    for (int i = deg; i > 0; --i) {
        int step = (1 << i);
        int mask = ~(step - 1);
        int split = 0;  // i.e. pivot - value to split array elements bys

        // calculate and send & recieve split values within a groups
        if ((rank & mask) == rank) {  // 'main' process in a 'group'
            // value to slit 'group' array elements by
            split = (data[0] + data[sz - 1]) / 2;
            // send split value to other 'group' members
            std::cout << "LE bit: #" << i << "; before swapping. Main's rank: " << rank 
                    << "; my rank: " << rank << "; orig split: " << split << std::endl;
                    // << "Start sending to other in group..." << std::endl;
            for (int dst = rank + 1; dst < rank + step; ++dst) {
                int tag = i;
                MPI_Send(&split, 1, MPI_INT, dst, tag, MPI_COMM_WORLD);
            }
        } else {  // all the processes in a 'group' except of 'main'
            // recieve value to slit array elements by
            int src = (rank & mask);
            int tag = i;
            MPI_Status status;
            MPI_Recv(&split, 1, MPI_INT, src, tag, MPI_COMM_WORLD, &status);
            std::cout << "LE bit: #" << i << "; before swapping. Main's rank: " << src 
                    << "; my rank: " << rank << "; rcvd split: " << split << std::endl;
        }        

        // split array by split value
        int *lt_split = NULL;
        int *ge_split = NULL;
        int lt_split_sz = 0;
        int ge_split_sz = 0;
        split_by_value(split, data, sz, &lt_split, &lt_split_sz, &ge_split, &ge_split_sz);
        std::cout << "LE bit: #" << i << "; after splitting. My rank: " << rank 
                << "; split: " << split << "; lt_split_sz: " << lt_split_sz
                << ", ge_split_sz: " << ge_split_sz << std::endl;
        // std::cout << "lt_split: " << lt_split[0] << " .. " << lt_split[lt_split_sz - 1]
        //         << "; ge_split: " << ge_split[0] << " .. " << ge_split[ge_split_sz - 1] << std::endl;

        free(data);
        data = NULL;
        sz = 0;

        // need this to decide this process sends or recieves first
        int i_bit_mask = (step >> 1);
        int i_bit = (rank & i_bit_mask);

        // process with i_bit == 1 sends lt_split array to its partner
        // (partner - process with the same rank number except of i_bit == 0)
        // and then recieves from it its ge_split array;
        // the partner, therefore, does the opposite:
        // first, it recieves lt_split array, and then sends ge_split array
        if (i_bit) {
            int partner_rank = (rank & ~i_bit_mask);
            std::cout << "LE bit: #" << i << "; in swapping. My i_bit == 1; rank: " 
                    << rank << "; partner_rank: " << partner_rank << std::endl;
            int tag = comm_sz * i + rank;
            // sent lt_split to partner
            MPI_Send(lt_split, lt_split_sz, MPI_INT, partner_rank, tag, MPI_COMM_WORLD);
            // need to know incoming partner's ge_split array size (i. e. partner_ge_split_sz)
            MPI_Status status;
            MPI_Probe(partner_rank, tag, MPI_COMM_WORLD, &status);
            int partner_ge_split_sz = 0;
            MPI_Get_count(&status, MPI_INT, &partner_ge_split_sz);
            int *partner_ge_split = (int *) calloc(partner_ge_split_sz, sizeof(int));
            // recieve ge_split from partner
            MPI_Recv(partner_ge_split, partner_ge_split_sz, 
                    MPI_INT, partner_rank, tag, MPI_COMM_WORLD, &status);
            // collect new data from ge_split & partner_ge_split
            sz = ge_split_sz + partner_ge_split_sz;
            data = (int *) calloc(sz, sizeof(int));
            // merge ge_split with partner_ge_split to data
            merge(ge_split, ge_split_sz, partner_ge_split, partner_ge_split_sz, data);
            // remove auxilary array
            if (partner_ge_split) {
                free(partner_ge_split);
            }

            std::cout << "LE bit: #" << i << "; after swapping. My i_bit == 1; rank: " 
                    << rank << "; split: " << split << "; data: " << data[0] << " .. " 
                    << data[sz - 1] << std::endl;
        } else {
            int partner_rank = (rank | i_bit_mask);
            std::cout << "LE bit: #" << i << "; in swapping. My i_bit == 0; rank: " 
                    << rank << "; partner_rank: " << partner_rank << std::endl;
            int tag = comm_sz * i + partner_rank;
            // need to know incoming partner's lt_split array size (i. e. partner_lt_split_sz)
            MPI_Status status;
            MPI_Probe(partner_rank, tag, MPI_COMM_WORLD, &status);
            int partner_lt_split_sz = 0;
            MPI_Get_count(&status, MPI_INT, &partner_lt_split_sz);
            int *partner_lt_split = (int *) calloc(partner_lt_split_sz, sizeof(int));
            // recieve lt_split from partner
            MPI_Recv(partner_lt_split, partner_lt_split_sz, 
                    MPI_INT, partner_rank, tag, MPI_COMM_WORLD, &status);
            // sent ge_split to partner
            MPI_Send(ge_split, ge_split_sz, MPI_INT, partner_rank, tag, MPI_COMM_WORLD);

            // collect new data from lt_split & partner_lt_split
            sz = lt_split_sz + partner_lt_split_sz;
            data = (int *) calloc(sz, sizeof(int));
            // merge lt_split with partner_lt_split to data
            merge(lt_split, lt_split_sz, partner_lt_split, partner_lt_split_sz, data);
            // remove auxilary array
            if (partner_lt_split) {
                free(partner_lt_split);
            }
            
            std::cout << "LE bit: #" << i << "; after swapping. My i_bit == 0; rank: " 
                    << rank << "; split: " << split << "; data: " << data[0] << " .. " 
                    << data[sz - 1] << std::endl;
        }

        // remove auxilary arrays after splitting, swapping and merging
        if (lt_split) {
            free(lt_split);
        }
        if (ge_split) {
            free(ge_split);
        }
    }

    MPI_Finalize();
}
