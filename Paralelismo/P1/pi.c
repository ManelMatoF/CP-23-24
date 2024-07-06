#include <stdio.h>
#include <math.h>
#include <mpi.h>

int main(int argc, char *argv[])
{
    int i, done = 0, n, rank, numprocs, num_iteraciones, intervalos;
    double PI25DT = 3.141592653589793238462643;
    double pi, h, sum, x;

    MPI_Init (&argc , &argv);
    MPI_Comm_size ( MPI_COMM_WORLD , &numprocs); 
    MPI_Comm_rank ( MPI_COMM_WORLD , &rank );

    int ini = rank;
    
    while (!done)
    {
        if (rank == 0)
        {
            printf("Enter the number of intervals: (0 quits) \n");
            scanf("%d",&intervalos);
            for (int i = 1; i < numprocs; i++)
                MPI_Send(&intervalos, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
        }else MPI_Recv(&intervalos, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        n = intervalos;
        
        if (n == 0) break;

        h   = 1.0 / (double) n;
        sum = 0.0;

        for (i = ini + 1; i <= n; i += numprocs) {
            x = h * ((double)i - 0.5);
            sum += 4.0 / (1.0 + x*x);
        }
        pi = h * sum;

        if (rank != 0)
        {
            MPI_Send(&pi, 1, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD);
        }else {
            double final_pi = pi;
            double buf[numprocs - 1];
            for (int i = 1; i < numprocs; i++)
            {
                MPI_Recv(&buf[i - 1], 1, MPI_DOUBLE, i, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                final_pi += buf[i - 1]; 
            }
            printf("pi is approximately %.16f, Error is %.16f\n", final_pi, fabs(final_pi - PI25DT));
        }
    }

    MPI_Finalize ();
}
