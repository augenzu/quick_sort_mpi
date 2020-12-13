#include "qsort-mpi.h"
#include <iomanip>
#include <iostream>
#include <cstring>

// calculates integer log2(num)
// works only if num == 2**deg && deg in [0, 30]
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

// fills array with equal values
void
fill_array(int *arr, int sz, int value)
{
    for (int i = 0; i < sz; ++i) {
        arr[i] = value;
    }
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
    std::cout << "Original array:" << std::endl;
    for (int i = 0; i < orig_sz; ++i) {
        std::cout << orig_data[i] << " ";
    }
    std::cout << std::endl;
    
    MPI_Init(NULL, NULL);

    // for time measuring
    double start, end;

    int comm_sz, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0) {
        start = MPI_Wtime();
    }

    int deg = log2(comm_sz);
    int dims[deg];
    fill_array(dims, deg, 2);
    int periods[deg];
    fill_array(periods, deg, 0);
    int reorder = 1;

    // communicator for hypercube topology
    MPI_Comm hypercube_comm;
    MPI_Cart_create(MPI_COMM_WORLD, deg, dims, periods, reorder, &hypercube_comm);

    int root = 0;

    // parts of original array for every process
    // int *data = NULL;
    // int sz = 0;
    
    // if (rank == 0) {
    //     int part_sz = orig_sz / comm_sz;
    //     int shift = orig_sz - (part_sz * comm_sz);

    //     // send original array parts to other processes [1..comm_sz)
    //     for (int i = 1; i < comm_sz; ++i) {
    //         int dst = i;
    //         int tag = 0;
    //         MPI_Send(orig_data + shift + part_sz * i, part_sz, MPI_INT, dst, tag, hypercube_comm);
    //     }

    //     // need this to work with 0's array part the same as we do this 
    //     // with other processes' parts
    //     sz = part_sz + shift;
    //     data = (int *) calloc(sz, sizeof(int));
    //     memcpy(data, orig_data, sz * sizeof(int));
    // } else {
    //     MPI_Status status;
    //     int src = 0;
    //     int tag = 0;

    //     // need to know incoming array size (i. e. part_sz from process 0)
    //     MPI_Probe(src, tag, hypercube_comm, &status);
    //     MPI_Get_count(&status, MPI_INT, &sz);

    //     data = (int *) calloc(sz, sizeof(int));

    //     // recieve array
    //     MPI_Recv(data, sz, MPI_INT, src, tag, hypercube_comm, &status);
    // }

    int part_sz = orig_sz / comm_sz;
    int shift = orig_sz - (part_sz * comm_sz);

    int sz = 0;
    if (rank == 0) {
        sz = part_sz + shift;
    } else {
        sz = part_sz;
    }
    int *data = (int *) calloc(sz, sizeof(int));

    int *sendcounts = (int *) calloc(comm_sz, sizeof(int));
    sendcounts[0] = sz;
    for (int i = 1; i < comm_sz; ++i) {
        sendcounts[i] = part_sz;
    }
    int *sendoffsets = (int *) calloc(comm_sz, sizeof(int));
    sendoffsets[0] = 0;
    for (int i = 1; i < comm_sz; ++i) {
        sendoffsets[i] = sendoffsets[i - 1] + sendcounts[i - 1];
    }

    MPI_Scatterv(orig_data, sendcounts, sendoffsets, MPI_INT, data, sz, MPI_INT, root, hypercube_comm);

    // std::cout << "Array part of rank: " << rank << ": " << data[0] << " .. " << data[sz - 1] << std::endl;

    free(sendcounts);
    free(sendoffsets);

    // sort each array part
    qsort(data, sz, sizeof(int), cmp);

    // main sorting cycle
    for (int i = deg; i > 0; --i) {
        int step = (1 << i);
        int mask = ~(step - 1);
        int split = 0;  // i.e. pivot - value to split array elements by

        // calculate and send & recieve split values within a groups
        if ((rank & mask) == rank) {  // 'main' process in a 'group'
            // value to slit 'group' array elements by
            split = (data[0] + data[sz - 1]) / 2;
            // send split value to other 'group' members
            for (int dst = rank + 1; dst < rank + step; ++dst) {
                int tag = i;
                MPI_Send(&split, 1, MPI_INT, dst, tag, hypercube_comm);
            }
        } else {  // all the processes in a 'group' except of 'main'
            // recieve value to slit array elements by
            int src = (rank & mask);
            int tag = i;
            MPI_Status status;

            MPI_Recv(&split, 1, MPI_INT, src, tag, hypercube_comm, &status);
        }        

        // split array by split value
        int *lt_split = NULL;
        int *ge_split = NULL;
        int lt_split_sz = 0;
        int ge_split_sz = 0;
        split_by_value(split, data, sz, &lt_split, &lt_split_sz, &ge_split, &ge_split_sz);

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
            int tag = comm_sz * i + rank;

            // sent lt_split to partner
            MPI_Send(lt_split, lt_split_sz, MPI_INT, partner_rank, tag, hypercube_comm);
      
            MPI_Status status;

            // need to know incoming partner's ge_split array size (i. e. partner_ge_split_sz)
            MPI_Probe(partner_rank, tag, hypercube_comm, &status);
            int partner_ge_split_sz = 0;
            MPI_Get_count(&status, MPI_INT, &partner_ge_split_sz);

            int *partner_ge_split = (int *) calloc(partner_ge_split_sz, sizeof(int));

            // recieve ge_split from partner
            MPI_Recv(partner_ge_split, partner_ge_split_sz, 
                    MPI_INT, partner_rank, tag, hypercube_comm, &status);
            // collect new data from ge_split & partner_ge_split

            sz = ge_split_sz + partner_ge_split_sz;
            data = (int *) calloc(sz, sizeof(int));
            // merge ge_split with partner_ge_split to data
            merge(ge_split, ge_split_sz, partner_ge_split, partner_ge_split_sz, data);

            // remove auxilary array
            if (partner_ge_split) {
                free(partner_ge_split);
            }
        } else {
            int partner_rank = (rank | i_bit_mask);
            int tag = comm_sz * i + partner_rank;
            MPI_Status status;

            // need to know incoming partner's lt_split array size (i. e. partner_lt_split_sz)
            MPI_Probe(partner_rank, tag, hypercube_comm, &status);
            int partner_lt_split_sz = 0;
            MPI_Get_count(&status, MPI_INT, &partner_lt_split_sz);

            int *partner_lt_split = (int *) calloc(partner_lt_split_sz, sizeof(int));

            // recieve lt_split from partner
            MPI_Recv(partner_lt_split, partner_lt_split_sz, 
                    MPI_INT, partner_rank, tag, hypercube_comm, &status);
            // sent ge_split to partner
            MPI_Send(ge_split, ge_split_sz, MPI_INT, partner_rank, tag, hypercube_comm);

            // collect new data from lt_split & partner_lt_split
            sz = lt_split_sz + partner_lt_split_sz;
            data = (int *) calloc(sz, sizeof(int));
            // merge lt_split with partner_lt_split to data
            merge(lt_split, lt_split_sz, partner_lt_split, partner_lt_split_sz, data);

            // remove auxilary array
            if (partner_lt_split) {
                free(partner_lt_split);
            }
        }

        // remove auxilary arrays after splitting, swapping and merging
        if (lt_split) {
            free(lt_split);
        }
        if (ge_split) {
            free(ge_split);
        }
    }

    // after sorting array parts we need 
    // to collect them together (to send to process 0)
    // and write all of them to original data array
    // in order of sending processes ranks
    // if (rank != 0) {  // ranks 1, 2, ... send their array parts to rank 0
    //     int dst = 0;
    //     int tag = rank;

    //     MPI_Send(data, sz, MPI_INT, dst, tag, hypercube_comm);

    //     free(data);

    //     MPI_Finalize();
    // } else {  // rank 0 recieves array parts from other ranks & collects the entire array
    //     int i = 0;  // orig_data index

    //     // write array part 0 to original array
    //     for (size_t j = 0; j < sz; ++j) {
    //         orig_data[i++] = data[j];
    //     }

    //     free(data);

    //     int **parts = (int **) calloc(comm_sz, sizeof(int *));
    //     int *part_szs = (int *) calloc(comm_sz, sizeof(int));

    //     // recieve & save array parts
    //     for (int src = 1; src < comm_sz; ++src) {
    //         int tag = src;
    //         MPI_Status status;

    //         MPI_Probe(src, tag, hypercube_comm, &status);
    //         int part_sz = 0;
    //         MPI_Get_count(&status, MPI_INT, &part_sz);

    //         parts[src] = (int *) calloc(part_sz, sizeof(int));

    //         MPI_Recv(parts[src], part_sz, MPI_INT, src, tag, hypercube_comm, &status);
    //         part_szs[src] = part_sz;
    //     }

    //     // write array parts to original array in order of their numbers
    //     for (int src = 1; src < comm_sz; ++src) {
    //         int part_sz = part_szs[src];
    //         for (size_t j = 0; j < part_sz; ++j) {
    //             orig_data[i++] = parts[src][j];
    //         }
    //         free(parts[src]);
    //     }

    //     free(parts);
    //     free(part_szs);

    //     end = MPI_Wtime();

    //     MPI_Finalize();

    //     double elapsed = end - start;

    //     // save to file: number of processes, array size, elapsed time
    //     std::cout << std::setw(4) << comm_sz
    //             << std::setw(9) << orig_sz
    //             << std::setw(12) << std::setprecision(8) << std::fixed << elapsed << std::endl;
    // }



    // every process sends its array elements number to process 0
    // to let it know what values should contain 
    // recvcounts & recvcoffsets in next MPI_Gatherv
    // (i. e. gathering sorted array parts from all the processes
    // to orig_data in process 0)

    int *recvcounts = (int *) calloc(comm_sz, sizeof(int));
    int sendcount = 1;

    MPI_Gather(&sz, sendcount, MPI_INT, recvcounts, comm_sz, MPI_INT, root, hypercube_comm);

    int *recvoffsets = (int *) calloc(comm_sz, sizeof(int));
    
    if (rank == 0) {
        recvoffsets[0] = 0;
        for (int i = 1; i < comm_sz; ++i) {
            recvoffsets[i] = recvoffsets[i - 1] + recvcounts[i - 1];
        }
    }

    //gathering all the sorted array parts together
    MPI_Gatherv(data, sz, MPI_INT, orig_data, recvcounts, recvoffsets, MPI_INT, root, hypercube_comm);

    free(recvcounts);
    free(recvoffsets);

    free(data);

    if (rank == 0) {
        end = MPI_Wtime();

        double elapsed = end - start;

        // save to file: number of processes, array size, elapsed time
        std::cout << std::setw(4) << comm_sz
                << std::setw(9) << orig_sz
                << std::setw(12) << std::setprecision(8) << std::fixed << elapsed << std::endl;

        std::cout << std::endl << "Sorted array:" << std::endl;
        for (int i = 0; i < orig_sz; ++i) {
            std::cout << orig_data[i] << " ";
        }
        std::cout << std::endl;
    }

    MPI_Finalize();
}
