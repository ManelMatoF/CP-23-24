#include <stdio.h>
#include <math.h>
#include <mpi.h>

int MPI_FlattreeColectiva( void * buff , void * recvbuff , int count ,
MPI_Datatype datatype , MPI_Op op , int root , MPI_Comm comm)
{
    int rank, numprocs;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &numprocs);

    if (rank != root) {
        MPI_Send(buff, count, datatype, 0, 1, comm);
    }else{
        double final_pi = *((double *)buff);
        for (int i = 0; i < numprocs - 1; i++)
        {
            double rec_pi;
            MPI_Recv(&rec_pi, count, datatype, MPI_ANY_SOURCE, 1, comm,  MPI_STATUS_IGNORE);
            final_pi += rec_pi;
        }
        *((double *)recvbuff) = final_pi;
    }
}

void MPI_BinomialBcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm) {
   int rank, size;
   MPI_Comm_rank(comm, &rank);
   MPI_Comm_size(comm, &size);

   int step = 1;
   while (step < size) {
       int sender = rank - step;
       int receiver = rank + step;
       if (receiver < size)
           MPI_Send(buffer, count, datatype, receiver, 0, comm);
       if (sender >= 0)
           MPI_Recv(buffer, count, datatype, sender, 0, comm, MPI_STATUS_IGNORE);
       step *= 2;
   }
}

int main(int argc, char *argv[])
{
    int i, done = 0, n, rank, numprocs, num_iteraciones, intervalos;
    double PI25DT = 3.141592653589793238462643;
    double pi, h, sum, x;

    MPI_Init (&argc , &argv);
    MPI_Comm_size ( MPI_COMM_WORLD , &numprocs); 
    MPI_Comm_rank ( MPI_COMM_WORLD , &rank );

    int ini = rank;
    //PARA MPI BINOMIAL USAR VARIABLE ACUMULATIVA EN VEZ DE POTENCIAS
    while (!done)
    {
        if (rank == 0)
        {
            printf("Enter the number of intervals: (0 quits) \n");
            scanf("%d",&intervalos);
        }
        MPI_BinomialBcast(&intervalos, 1, MPI_INT, 0, MPI_COMM_WORLD);
        n = intervalos;
        
        if (n == 0) break;

        h   = 1.0 / (double) n;
        sum = 0.0;

        for (i = ini + 1; i <= n; i += numprocs) {
            x = h * ((double)i - 0.5);
            sum += 4.0 / (1.0 + x*x);
        }
        pi = h * sum;

        double final_pi;
        double buf;
        MPI_Barrier(MPI_COMM_WORLD);
        //MPI_Reduce(&pi, &final_pi, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_FlattreeColectiva(&pi, &final_pi, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        if (rank == 0 )
            printf("pi is approximately %.16f, Error is %.16f\n", final_pi, fabs(final_pi - PI25DT));
        
        }

    MPI_Finalize ();
}
