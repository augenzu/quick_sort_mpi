#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>

#define DIM 4
#define NDIMS 2

int
max(int num1, int num2) {
    return num1 > num2 ? num1 : num2;
}

int
main()
{
    MPI_Init(NULL, NULL);

    int comm_sz, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // communicator for transputer matrix topology
    int dims[NDIMS] = { DIM, DIM };
    int periods[NDIMS] = { 0, 0 };
    int reorder = 0;
    MPI_Comm transputer_comm;
    MPI_Cart_create(MPI_COMM_WORLD, NDIMS, dims, periods, reorder, &transputer_comm);

    // obtain coordinates in new topology
    int coordinates[NDIMS];
    MPI_Cart_coords(transputer_comm, rank, NDIMS, coordinates); 

    // generate 2 random numbers for process & find the max of them
    srand(42 +  rank);
    int num1 = rand() % 100;
    int num2 = rand() % 100;
    int maxnum = max(num1, num2);
    printf("[before allreduce | rank %2d] maxnum: %2d\n", rank, maxnum);

    // will need them for further sends & recvs
    int other_rank = 0;
    int other_coordinates[NDIMS];
    MPI_Status status;
    int tag = 0;
    int recieved = 0;

    // --- STEP 1 of the REDUCE phase ---
    tag = 1;
    other_coordinates[1] = coordinates[1];
    if (coordinates[0] == 0) {  // (0,0) -> (1,0); (0,1) -> (1,1); (0,2) -> (1,2); (0,3)->(1,3)
        other_coordinates[0] = 1;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the reciever's rank
        MPI_Send(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm);
    } else if (coordinates[0] == 3) {  // (3,0) -> (2,0); (3,1) -> (2,1); (3,2) -> (2,2); (3,3) -> (2,3)
        other_coordinates[0] = 2;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the reciever's rank
        MPI_Send(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm);
    } else if (coordinates[0] == 1) {  // (0,0) -> (1,0); (0,1) -> (1,1); (0,2) -> (1,2); (0,3)->(1,3)
        other_coordinates[0] = 0;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the sender's rank
        MPI_Recv(&recieved, 1, MPI_INT, other_rank, tag, transputer_comm, &status);
        maxnum = max(maxnum, recieved);
    } else if (coordinates[0] == 2) {  // (3,0) -> (2,0); (3,1) -> (2,1); (3,2) -> (2,2); (3,3) -> (2,3)
        other_coordinates[0] = 3;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the sender's rank
        MPI_Recv(&recieved, 1, MPI_INT, other_rank, tag, transputer_comm, &status);
        maxnum = max(maxnum, recieved);
    }
    MPI_Barrier(transputer_comm);

    // --- STEP 2 of the REDUCE phase ---
    tag = 2;
    if (coordinates[0] == 1 || coordinates[0] == 2) {
        other_coordinates[0] = coordinates[0];
        if (coordinates[1] == 0) {  // (1,0) -> (1,1); (2,0) -> (2,1)
            other_coordinates[1] = 1;
            MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the reciever's rank
            MPI_Send(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm);
        } else if (coordinates[1] == 3) {  // (1,3) -> (1,2); (2,3) -> (2,2)
            other_coordinates[1] = 2;
            MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the reciever's rank
            MPI_Send(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm);
        } else if (coordinates[1] == 1) {  // (1,0) -> (1,1); (2,0) -> (2,1)
            other_coordinates[1] = 0;
            MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the sender's rank
            MPI_Recv(&recieved, 1, MPI_INT, other_rank, tag, transputer_comm, &status);
            maxnum = max(maxnum, recieved);
        } else if (coordinates[1] == 2) {  // (1,3) -> (1,2); (2,3) -> (2,2)
            other_coordinates[1] = 3;
            MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the sender's rank
            MPI_Recv(&recieved, 1, MPI_INT, other_rank, tag, transputer_comm, &status);
            maxnum = max(maxnum, recieved);
        }
    }
    MPI_Barrier(transputer_comm);

    // --- STEP 3 of the REDUCE phase ---
    tag = 3;
    if (coordinates[0] == 1 && coordinates[1] == 1) {  // (1,1) -> (2,1)
        other_coordinates[0] = 2;
        other_coordinates[1] = 1;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the reciever's rank
        MPI_Send(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm);
    } else if (coordinates[0] == 1 && coordinates[1] == 2) {  // (1,2) -> (2,2)
        other_coordinates[0] = 2;
        other_coordinates[1] = 2;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the reciever's rank
        MPI_Send(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm);
    } else if (coordinates[0] == 2 && coordinates[1] == 1) {  // (1,1) -> (2,1)
        other_coordinates[0] = 1;
        other_coordinates[1] = 1;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the sender's rank
        MPI_Recv(&recieved, 1, MPI_INT, other_rank, tag, transputer_comm, &status);
        maxnum = max(maxnum, recieved);
    } else if (coordinates[0] == 2 && coordinates[1] == 2) {  // (1,2) -> (2,2)
        other_coordinates[0] = 1;
        other_coordinates[1] = 2;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the sender's rank
        MPI_Recv(&recieved, 1, MPI_INT, other_rank, tag, transputer_comm, &status);
        maxnum = max(maxnum, recieved);
    }
    MPI_Barrier(transputer_comm);

    // --- STEP 4 of the REDUCE phase ---
    tag = 4;
    if (coordinates[0] == 2 && coordinates[1] == 1) {  // (2,1) -> (1,1)
        other_coordinates[0] = 1;
        other_coordinates[1] = 1;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the reciever's rank
        MPI_Send(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm);
    } else if (coordinates[0] == 2 && coordinates[1] == 2) {  // (2,2) -> (1,2)
        other_coordinates[0] = 1;
        other_coordinates[1] = 2;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the reciever's rank
        MPI_Send(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm);
    } else if (coordinates[0] == 1 && coordinates[1] == 1) {  // (2,1) -> (1,1)
        other_coordinates[0] = 2;
        other_coordinates[1] = 1;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the sender's rank
        MPI_Recv(&recieved, 1, MPI_INT, other_rank, tag, transputer_comm, &status);
        maxnum = max(maxnum, recieved);
    } else if (coordinates[0] == 1 && coordinates[1] == 2) {  // (2,2) -> (1,2)
        other_coordinates[0] = 2;
        other_coordinates[1] = 2;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the sender's rank
        MPI_Recv(&recieved, 1, MPI_INT, other_rank, tag, transputer_comm, &status);
        maxnum = max(maxnum, recieved);
    }
    MPI_Barrier(transputer_comm);

    // --- STEP 5 of the REDUCE phase ---
    tag = 5;
    if (coordinates[0] == 1 && coordinates[1] == 1) {  // (1,1) -> (1,2)
        other_coordinates[0] = 1;
        other_coordinates[1] = 2;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the reciever's rank
        MPI_Send(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm);
    } else if (coordinates[0] == 2 && coordinates[1] == 1) {  // (2,1) -> (2,2)
        other_coordinates[0] = 2;
        other_coordinates[1] = 2;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the reciever's rank
        MPI_Send(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm);
    } else if (coordinates[0] == 1 && coordinates[1] == 2) {  // (1,1) -> (1,2)
        other_coordinates[0] = 1;
        other_coordinates[1] = 1;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the sender's rank
        MPI_Recv(&recieved, 1, MPI_INT, other_rank, tag, transputer_comm, &status);
        maxnum = max(maxnum, recieved);
    } else if (coordinates[0] == 2 && coordinates[1] == 2) {  // (2,1) -> (2,2)
        other_coordinates[0] = 2;
        other_coordinates[1] = 1;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the sender's rank
        MPI_Recv(&recieved, 1, MPI_INT, other_rank, tag, transputer_comm, &status);
        maxnum = max(maxnum, recieved);
    }
    MPI_Barrier(transputer_comm);

    // --- STEP 6 of the REDUCE phase ---
    tag = 6;
    if (coordinates[0] == 1 && coordinates[1] == 2) {  // (1,2) -> (1,1)
        other_coordinates[0] = 1;
        other_coordinates[1] = 1;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the reciever's rank
        MPI_Send(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm);
    } else if (coordinates[0] == 2 && coordinates[1] == 2) {  // (2,2) -> (2,1)
        other_coordinates[0] = 2;
        other_coordinates[1] = 1;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the reciever's rank
        MPI_Send(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm);
    } else if (coordinates[0] == 1 && coordinates[1] == 1) {  // (1,2) -> (1,1)
        other_coordinates[0] = 1;
        other_coordinates[1] = 2;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the sender's rank
        MPI_Recv(&recieved, 1, MPI_INT, other_rank, tag, transputer_comm, &status);
        maxnum = max(maxnum, recieved);
    } else if (coordinates[0] == 2 && coordinates[1] == 1) {  // (2,2) -> (2,1)
        other_coordinates[0] = 2;
        other_coordinates[1] = 2;
        MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the sender's rank
        MPI_Recv(&recieved, 1, MPI_INT, other_rank, tag, transputer_comm, &status);
        maxnum = max(maxnum, recieved);
    }
    MPI_Barrier(transputer_comm);

    // --- STEP 1 of the SCATTER phase ---
    tag = 7;
    if (coordinates[1] == 1 || coordinates[1] == 2) {
        other_coordinates[1] = coordinates[1];
        if (coordinates[0] == 1) {  // (1,1) -> (0,1); (1,2) -> (0,2)
            other_coordinates[0] = 0;
            MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the reciever's rank
            MPI_Send(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm);
        } else if (coordinates[0] == 2) {  // (2,1) -> (3,1); (2,2) -> (3,2)
            other_coordinates[0] = 3;
            MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the reciever's rank
            MPI_Send(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm);
        } else if (coordinates[0] == 0) {  // (1,1) -> (0,1); (1,2) -> (0,2)
            other_coordinates[0] = 1;
            MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the sender's rank
            MPI_Recv(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm, &status);
        } else if (coordinates[0] == 3) {  // (2,1) -> (3,1); (2,2) -> (3,2)
            other_coordinates[0] = 2;
            MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the sender's rank
            MPI_Recv(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm, &status);
        }
    }
    MPI_Barrier(transputer_comm);

    // --- STEP 2 of the SCATTER phase ---
    tag = 8;
    if (coordinates[0] == 1 || coordinates[0] == 2) {
        other_coordinates[0] = coordinates[0];
        if (coordinates[1] == 1) {  // (1,1) -> (1,0); (2,1) -> (2,0)
            other_coordinates[1] = 0;
            MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the reciever's rank
            MPI_Send(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm);
        } else if (coordinates[1] == 2) {  // (1,2) -> (1,3); (2,2) -> (2,3)
            other_coordinates[1] = 3;
            MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the reciever's rank
            MPI_Send(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm);
        } else if (coordinates[1] == 0) {  // (1,1) -> (1,0); (2,1) -> (2,0)
            other_coordinates[1] = 1;
            MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the sender's rank
            MPI_Recv(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm, &status);
        } else if (coordinates[1] == 3) {  // (1,2) -> (1,3); (2,2) -> (2,3)
            other_coordinates[1] = 2;
            MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the sender's rank
            MPI_Recv(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm, &status);
        }
    }
    MPI_Barrier(transputer_comm);

    // --- STEP 3 of the SCATTER phase ---
    tag = 9;
    if (coordinates[0] == 0 || coordinates[0] == 3) {
        other_coordinates[0] = coordinates[0];
        if (coordinates[1] == 1) {  // (0,1) -> (0,0); (3,1) -> (3,0)
            other_coordinates[1] = 0;
            MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the reciever's rank
            MPI_Send(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm);
        } else if (coordinates[1] == 2) {  // (0,2) -> (0,3); (3,2) -> (3,3)
            other_coordinates[1] = 3;
            MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the reciever's rank
            MPI_Send(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm);
        } else if (coordinates[1] == 0) {  // (0,1) -> (0,0); (3,1) -> (3,0)
            other_coordinates[1] = 1;
            MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the sender's rank
            MPI_Recv(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm, &status);
        } else if (coordinates[1] == 3) {  // (0,2) -> (0,3); (3,2) -> (3,3)
            other_coordinates[1] = 2;
            MPI_Cart_rank(transputer_comm, other_coordinates, &other_rank);  // obtain the sender's rank
            MPI_Recv(&maxnum, 1, MPI_INT, other_rank, tag, transputer_comm, &status);
        }
    }
    MPI_Barrier(transputer_comm);

    printf("[after  allreduce | rank %2d] maxnum: %2d\n", rank, maxnum);

    MPI_Finalize();
}
