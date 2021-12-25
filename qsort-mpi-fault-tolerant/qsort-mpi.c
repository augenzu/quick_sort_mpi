#include "qsort-mpi.h"
#include "mpi.h"
#include "mpi-ext.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAUXILARY 3
#define MAX_NUMBER_LEN 10
#define RADIX 10
#define ROOT 0
#define WANNA_DIE_RANK 1

MPI_Comm comm, working_comm;
MPI_Group comm_group, working_group;
int nworking;
int *working_ranks;
int rank;
int error_occured = 0;

void
save_check_point(int *data, int data_sz)
{
    char fname[MAX_NUMBER_LEN];
    sprintf(fname, "%d", rank);
    FILE *fout = fopen(fname, "wb");
    fwrite(&data_sz, sizeof(int),  1, fout);
    fwrite(data, sizeof(int),  data_sz, fout);
    fclose(fout);
}

void
load_check_point(int **data, int *data_sz)
{
    char fname[MAX_NUMBER_LEN];
    sprintf(fname, "%d", rank);
    FILE *fin = fopen(fname, "rb");
    fread(data_sz, sizeof(int),  1, fin);
    if (*data_sz == 0) {
        *data = NULL;
        return;
    }
    *data = (int *) calloc(*data_sz, sizeof(int));
    fread(*data, sizeof(int),  *data_sz, fin);
    fclose(fin);
}

void
verbose_errhandler(MPI_Comm *pcomm, int *perr, ...)
{
    error_occured = 1;

    int err = *perr;

    MPI_Group failed_group;
    int nfailed;
    MPIX_Comm_failure_ack(comm);
    MPIX_Comm_failure_get_acked(comm, &failed_group);
    MPI_Group_size(failed_group, &nfailed);

    int comm_real_sz;
    MPI_Comm_size(comm, &comm_real_sz);
    char errstr[MPI_MAX_ERROR_STRING];
    int len;
    MPI_Error_string(err, errstr, &len);
    printf("Rank %d / %d: Notified of error %s. %d found dead.\n",
           rank, comm_real_sz, errstr, nfailed);

    MPIX_Comm_shrink(comm, &comm);
    MPI_Comm_rank(comm, &rank);

    MPI_Comm_group(comm, &comm_group);
    MPI_Group_incl(comm_group, nworking, working_ranks, &working_group);
    MPI_Comm_create(comm, working_group, &working_comm);
}

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
    MPI_Init(NULL, NULL);

    comm = MPI_COMM_WORLD;

    MPI_Errhandler errh;
    MPI_Comm_create_errhandler(verbose_errhandler, &errh);
    MPI_Comm_set_errhandler(comm, errh);
    MPI_Barrier(comm);

    int comm_sz;
    MPI_Comm_size(comm, &comm_sz);
    MPI_Comm_rank(comm, &rank);

    nworking = comm_sz - NAUXILARY;

    working_ranks = (int *) calloc(nworking, sizeof(int));
    for (int i = 0; i < nworking; ++i) {
        working_ranks[i] = i;
    }
    MPI_Comm_group(comm, &comm_group);
    MPI_Group_incl(comm_group, nworking, working_ranks, &working_group);
    MPI_Comm_create(comm, working_group, &working_comm);

    if (rank == ROOT) {
        printf("before sort: ");
        for (int i = 0; i < orig_sz; ++i) {
            printf("%d ", orig_data[i]);
        }
        printf("\n");
    }

    int *data = NULL;
    int sz = 0;

    int *sendcounts = NULL;
    int *sendoffsets = NULL;

    if (rank < nworking) {
        int part_sz = orig_sz / nworking;
        int shift = orig_sz - (part_sz * nworking);

        if (rank == ROOT) {
            sz = part_sz + shift;
        } else {
            sz = part_sz;
        }
        data = (int *) calloc(sz, sizeof(int));

        if (rank == ROOT) {
            sendcounts = (int *) calloc(nworking, sizeof(int));
            sendcounts[0] = sz;
            for (int i = 1; i < nworking; ++i) {
                sendcounts[i] = part_sz;
            }
            sendoffsets = (int *) calloc(nworking, sizeof(int));
            sendoffsets[0] = 0;
            for (int i = 1; i < nworking; ++i) {
                sendoffsets[i] = sendoffsets[i - 1] + sendcounts[i - 1];
            }
        }

        MPI_Scatterv(orig_data, sendcounts, sendoffsets, MPI_INT, data, sz, MPI_INT, ROOT, working_comm);

        // sort & save each array part
        qsort(data, sz, sizeof(int), cmp);
        save_check_point(data, sz);
        free(data);
    }

    if (rank == ROOT) {
        free(sendcounts);
        free(sendoffsets);
    }

    if (rank == WANNA_DIE_RANK) {
        raise(SIGKILL);
    }

    // main sorting cycle
    int deg = log2(nworking);
    for (int i = deg; i > 0; --i) {
        check_point_label:
        MPI_Barrier(comm);
        if (rank < nworking) {
            load_check_point(&data, &sz);
        }

        if (rank < nworking) {
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
                    MPI_Send(&split, 1, MPI_INT, dst, tag, working_comm);
                }
                if (error_occured) {
                    error_occured = 0;
                    goto check_point_label;
                }
            } else {  // all the processes in a 'group' except of 'main'
                // recieve value to slit array elements by
                int src = (rank & mask);
                int tag = i;
                MPI_Status status;

                MPI_Recv(&split, 1, MPI_INT, src, tag, working_comm, &status);
                if (error_occured) {
                    error_occured = 0;
                    goto check_point_label;
                }
            }        

            // split array by split value
            int *lt_split = NULL;
            int *ge_split = NULL;
            int lt_split_sz = 0;
            int ge_split_sz = 0;
            split_by_value(split, data, sz, &lt_split, &lt_split_sz, &ge_split, &ge_split_sz);

            free(data);

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
                int tag = nworking * i + rank;

                // sent lt_split to partner
                MPI_Send(lt_split, lt_split_sz, MPI_INT, partner_rank, tag, working_comm);
                if (error_occured) {
                    error_occured = 0;
                    goto check_point_label;
                }
        
                MPI_Status status;

                // need to know incoming partner's ge_split array size (i. e. partner_ge_split_sz)
                MPI_Probe(partner_rank, tag, working_comm, &status);
                if (error_occured) {
                    error_occured = 0;
                    goto check_point_label;
                }
                int partner_ge_split_sz = 0;
                MPI_Get_count(&status, MPI_INT, &partner_ge_split_sz);

                int *partner_ge_split = (int *) calloc(partner_ge_split_sz, sizeof(int));

                // recieve ge_split from partner
                MPI_Recv(partner_ge_split, partner_ge_split_sz, 
                        MPI_INT, partner_rank, tag, working_comm, &status);
                if (error_occured) {
                    error_occured = 0;
                    goto check_point_label;
                }
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
                int tag = nworking * i + partner_rank;
                MPI_Status status;

                // need to know incoming partner's lt_split array size (i. e. partner_lt_split_sz)
                MPI_Probe(partner_rank, tag, working_comm, &status);
                if (error_occured) {
                    error_occured = 0;
                    goto check_point_label;
                }
                int partner_lt_split_sz = 0;
                MPI_Get_count(&status, MPI_INT, &partner_lt_split_sz);

                int *partner_lt_split = (int *) calloc(partner_lt_split_sz, sizeof(int));

                // recieve lt_split from partner
                MPI_Recv(partner_lt_split, partner_lt_split_sz, 
                        MPI_INT, partner_rank, tag, working_comm, &status);
                if (error_occured) {
                    error_occured = 0;
                    goto check_point_label;
                }
                // sent ge_split to partner
                MPI_Send(ge_split, ge_split_sz, MPI_INT, partner_rank, tag, working_comm);
                if (error_occured) {
                    error_occured = 0;
                    goto check_point_label;
                }

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

        MPI_Barrier(comm);
        if (rank < nworking) {
            save_check_point(data, sz);
            free(data);
        }
    }

    if (rank < nworking) {
        load_check_point(&data, &sz);
    }

    // every process sends its array elements number to process 0
    // to let it know what values should contain 
    // recvcounts & recvcoffsets in next MPI_Gatherv
    // (i. e. gathering sorted array parts from all the processes
    // to orig_data in process 0)

    int *recvcounts = NULL;
    if (rank == ROOT) {
        recvcounts = (int *) calloc(nworking, sizeof(int));
    }

    if (rank < nworking) {
        MPI_Gather(&sz, 1, MPI_INT, recvcounts, 1, MPI_INT, ROOT, working_comm);
    }

    int *recvoffsets = NULL;
    
    if (rank == ROOT) {
        recvoffsets = (int *) calloc(nworking, sizeof(int));
        recvoffsets[0] = 0;
        for (int i = 1; i < nworking; ++i) {
            recvoffsets[i] = recvoffsets[i - 1] + recvcounts[i - 1];
        }
    }

    //gathering all the sorted array parts together
    if (rank < nworking) {
        MPI_Gatherv(data, sz, MPI_INT, orig_data, recvcounts, recvoffsets, MPI_INT, ROOT, working_comm);
        free(data);
    }

    if (rank == ROOT) {
        free(recvcounts);
        free(recvoffsets);
        printf("after sort: ");
        for (int i = 0; i < orig_sz; ++i) {
            printf("%d ", orig_data[i]);
        }
        printf("\n");
    }

    free(working_ranks);

    MPI_Barrier(comm);
    MPI_Finalize();
}
