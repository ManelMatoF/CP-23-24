#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <mpi.h>

#define DEBUG 1

#define N 128

int main(int argc, char *argv[] ) {

  int rank, size, i, j;
  float *matrix;
  float vector[N];
  float *result;
  struct timeval  tv1, tv2;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  int rows_per_process = N / size;
  int extra_rows = N % size;
  

  float *sub_matrix = (float *)malloc(rows_per_process * N * sizeof(float));
  float *sub_result = (float *)malloc(rows_per_process * sizeof(float));

   //Proceso 0 inicializa la matrix y el vector
    if (rank == 0) {
        result = malloc(N*sizeof(float));
        matrix = (float *)malloc(N * N * sizeof(float));
        for (i = 0; i < N; i++) {
            vector[i] = i;
            for (j = 0; j < N; j++) {
                matrix[i*N + j] = i + j;
            }
        }
    }

    //Manda la matrix
    MPI_Scatter(matrix, rows_per_process * N, MPI_FLOAT, sub_matrix,
                rows_per_process * N, MPI_FLOAT, 0, MPI_COMM_WORLD);

    MPI_Bcast(vector, N, MPI_FLOAT, 0, MPI_COMM_WORLD);

    gettimeofday(&tv1, NULL);

    //Calcular el resultado parcial
    for (i = 0; i < rows_per_process; i++) {
        sub_result[i] = 0;
        for (j = 0; j < N; j++) {
            sub_result[i] += sub_matrix[i * N + j] * vector[j];
        }
    }

    //Recoge los resultados
    MPI_Gather(sub_result, rows_per_process, MPI_FLOAT, result,
                rows_per_process, MPI_FLOAT, 0, MPI_COMM_WORLD);

   if(rank == 0){
      int res = N % size;
      for (i = 0; i < res; i++) {
          result[N - res + i] = 0;
          for (j = 0; j < N; j++) {
              result[N - res + i] += matrix[(N - res + i) * N + j] * vector[j];
            }
        }
    }

    gettimeofday(&tv2, NULL);
    
  int microseconds = (tv2.tv_usec - tv1.tv_usec)+ 1000000 * (tv2.tv_sec - tv1.tv_sec);

  if (rank == 0) {
        if (DEBUG) {
            for (i = 0; i < N; i++) {
                printf("%f ", result[i]);
            }
            printf("\n");
        } else {
            printf("Time (seconds) = %lf\n", (double)microseconds / 1E6);
        }
        free(matrix);
}
    free(sub_matrix);
    free(sub_result);

    MPI_Finalize();


  return 0;
  
}

