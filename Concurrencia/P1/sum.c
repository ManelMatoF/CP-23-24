#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "options.h"

struct nums {
	long *increase;
	long *decrease;
    int size; //ARRAY SIZE
	long total;
	long diff;
};

struct args {
	int thread_num;		// application defined thread #
	long iterations;	// number of operations
	struct nums *nums;	// pointer to the counters (shared with other threads)
    pthread_mutex_t *mutex;//Mutex unico
    pthread_mutex_t *mutexs; // Array de mutexes asociados a las posiciones
};

struct args2 {
	int thread_num;		// application defined thread #
	long iterations;	// number of operations
	struct nums *nums;	// pointer to the counters (shared with other threads)
    pthread_mutex_t *mutex;//Mutex unico
};

struct thread_info {
    pthread_t    id;    // id returned by pthread_create()
    struct args *args;  // pointer to the arguments
};

struct thread_info2 {
    pthread_t    id;    // id returned by pthread_create()
    struct args2 *args;  // pointer to the arguments
};

struct thread_info3 {
    pthread_t    id;    // id returned by pthread_create()
    struct args2 args;  // structure on the stack
};

long sum_arrays(long *a, long *b, int size)   
{
    long aux = 0;

    for (int i = 0; i < size; i++)
    {
        aux += a[i] + b[i];
    }
    
    return aux;

}

long sum_array(long *a, int size)   
{
    long aux = 0;

    for (int i = 0; i < size; i++)
    {
        aux += a[i];
    }
    
    return aux;

}

// Threads run on this function
void *decrease_increase(void *ptr)
{
	struct args *args = ptr;
	struct nums *n = args->nums;

    //contador compartido
    static int shared_counter = 0;


	while(shared_counter < args->iterations) {

        pthread_mutex_lock(args->mutex);
            
        shared_counter++;

        pthread_mutex_unlock(args->mutex);

        int index_increase = rand() % args->nums->size;
        int index_decrease = rand() % args->nums->size;


        pthread_mutex_lock(&args->mutexs[index_decrease]); // Bloquea el mutex asociado a la posición
		n->decrease[index_decrease]--;
        pthread_mutex_unlock(&args->mutexs[index_decrease]);

        pthread_mutex_lock(&args->mutexs[index_increase]);
		n->increase[index_increase]++;
        pthread_mutex_unlock(&args->mutexs[index_increase]);

		long diff = (n->total * n->size) - sum_arrays(n->increase, n->decrease, n->size);
		if (diff != n->diff) {
			n->diff = diff;
			printf("Thread %d increasing %ld decreasing %ld diff %ld\n",
			       args->thread_num, sum_array(n->increase, n->size),  sum_array(n->decrease, n->size), diff);
		}

    }
   
    return NULL;
}

void *increase (void *ptr){
    struct args2 *args = ptr;
	struct nums *n = args->nums;

    //contador compartido
    static int shared_counter = 0;

     while(shared_counter < args->iterations) {
        pthread_mutex_lock(args->mutex);
        shared_counter++;

        int index_increase = rand() % args->nums->size;
        int index_decrease = rand() % args->nums->size;

        n->increase[index_increase]++;
        n->increase[index_decrease]--;

        long diff = (n->total * n->size) - sum_arrays(n->increase, n->decrease, n->size);
		if (diff != n->diff) {
			n->diff = diff;
			printf("Thread %d increasing %ld decreasing %ld diff %ld\n",
			       args->thread_num, sum_array(n->increase, n->size),  sum_array(n->decrease, n->size), diff);
		}
        pthread_mutex_unlock(args->mutex);
     }

     return NULL;
}


// start opt.num_threads threads running on decrease_incresase
struct thread_info2 *start_threads(struct options opt, struct nums *nums, pthread_mutex_t *mutex)
{
    int i;
    struct thread_info2 *threads;

    printf("creating %d threads\n", opt.num_threads);
    threads = malloc(sizeof(struct thread_info2) * opt.num_threads);


    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }


    // Create num_thread threads running decrease_increase
    for (i = 0; i < opt.num_threads; i++) {
        threads[i].args = malloc(sizeof(struct args2));

        threads[i].args->thread_num = i;
        threads[i].args->nums       = nums;
        threads[i].args->iterations = opt.iterations;
        threads[i].args->mutex      = mutex;
        
        if (0 != pthread_create(&threads[i].id, NULL, increase, threads[i].args)) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
    }

    return threads;
}

void print_totals(struct nums *nums)
{
	printf("Final: increasing %ld decreasing %ld diff %ld\n",
	       sum_array(nums->increase, nums->size), sum_array(nums->decrease, nums->size), nums->total * nums->size - sum_arrays(nums->increase, nums->decrease, nums->size));
}

// wait for all threads to finish, print totals, and free memory
void wait(struct options opt, struct nums *nums, struct thread_info3 *threads) {
    // Wait for the threads to finish
    for (int i = 0; i < opt.num_threads; i++)
        pthread_join(threads[i].id, NULL);

    print_totals(nums);
}

int main (int argc, char **argv){
    struct options opt;
    struct nums nums;


    srand(time(NULL));

    // Default values for the options
    opt.num_threads  = 4;
    opt.iterations   = 100000;
    opt.size         = 10;

    struct thread_info3 thrs3[opt.num_threads];

    read_options(argc, argv, &opt);

    pthread_mutex_t mutex;
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        printf("Error al inicializar el mutex.\n");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_t *mutexs = malloc(opt.size * sizeof(pthread_mutex_t));
    if (mutexs == NULL) {
        printf("Not enough memory\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < opt.size; i++)
    {
        if (pthread_mutex_init(&mutexs[i], NULL) != 0)
        {
            printf("Error al inicializar el mutex.\n");
            exit(EXIT_FAILURE);
        }
    }
    
    nums.total = opt.iterations * opt.num_threads;
    nums.size = opt.size;
    nums.increase = malloc(opt.size * sizeof(long));
    nums.decrease = malloc(opt.size * sizeof(long));

    for (int i = 0; i < opt.size; i++)
    {
        nums.increase[i] = 0;
        nums.decrease[i] = nums.total;
    }
    
    nums.diff = 0;
    
    for (int i = 0; i < opt.num_threads; i++) {
        thrs3[i].args.thread_num = i;
        thrs3[i].args.nums = &nums;
        thrs3[i].args.iterations = opt.iterations;
        thrs3[i].args.mutex = &mutex;
        
        if (0 != pthread_create(&thrs3[i].id, NULL, increase, &thrs3[i].args)) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
    }

    wait(opt, &nums, thrs3);


    pthread_mutex_destroy(&mutex);

    for (int i = 0; i < opt.size; i++)
        pthread_mutex_destroy(&mutexs[i]);

    return 0;
}
