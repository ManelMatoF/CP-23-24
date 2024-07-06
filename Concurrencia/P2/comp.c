#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "compress.h"
#include "chunk_archive.h"
#include "queue.h"
#include "options.h"
#include <pthread.h>

#define CHUNK_SIZE (1024*1024)
#define QUEUE_SIZE 50

#define COMPRESS 1
#define DECOMPRESS 0

struct thread_args {
    queue in;               // Cola de entrada de chunks
    queue out;             // Cola de salida de chunks
    int chunks;
    int fd;
    pthread_mutex_t *mutex;
    struct options opt;
};

void *reader(void *ptr) {
    struct thread_args *args = ptr;
    int i, offset;
    chunk ch;

    for (i = 0; i < args->chunks; i++) {
        ch = alloc_chunk(args->opt.size);

        offset = lseek(args->fd, 0, SEEK_CUR);

        ch->size   = read(args->fd, ch->data, args->opt.size);
        ch->num    = i;
        ch->offset = offset;

        q_insert(args->in, ch);
    }

    return NULL;
}

// take chunks from queue in, run them through process (compress or decompress), send them to queue out
void *worker(void *ptr) {
    struct thread_args *args = ptr;
    chunk ch, res;
    static int counter = 0;

    while(1) {
    
        pthread_mutex_lock(args->mutex);
        if (counter >= args->chunks)
        {
            pthread_mutex_unlock(args->mutex);
            break;
        }
        
        counter++;
        pthread_mutex_unlock(args->mutex);

        ch = q_remove(args->in);

        res = zcompress(ch);
        free_chunk(ch);

        q_insert(args->out, res);

        
        }
    return NULL;
}

void *writer(void *ptr){
    struct thread_args *args = ptr;
    archive ar;
    chunk ch;
    char comp_file[256];

    if(args->opt.out_file) {
        strncpy(comp_file,args->opt.out_file,255);
    } else {
        strncpy(comp_file, args->opt.file, 255);
        strncat(comp_file, ".ch", 255);
    }

    ar = create_archive_file(comp_file);

    for(int i=0; i<args->chunks; i++) {
        ch = q_remove(args->out);

        add_chunk(ar, ch);
        free_chunk(ch);
    }

    close_archive_file(ar);

    return NULL;
}

// Compress file taking chunks of opt.size from the input file,
// inserting them into the in queue, running them using a worker,
// and sending the output from the out queue into the archive file
void comp(struct options opt) {
    int fd, chunks, i;
    struct stat st;
    queue in, out;
    pthread_t reader_thread, writer_thread, worker_threads[opt.num_threads];

    if((fd=open(opt.file, O_RDONLY))==-1) {
        printf("Cannot open %s\n", opt.file);
        exit(0);
    }

    fstat(fd, &st);
    chunks = st.st_size/opt.size+(st.st_size % opt.size ? 1:0);

    in  = q_create(opt.queue_size);
    out = q_create(opt.queue_size);

    struct thread_args args_reader, args_writer, args_workers[opt.num_threads];
    

    args_reader.fd = fd;
    args_reader.in = in;
    args_reader.chunks = chunks;
    args_reader.opt = opt;
        
    pthread_create(&reader_thread, NULL, reader, (void *)&args_reader);
    
   

    // Crea e inicializa los threads
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    for (i = 0; i < opt.num_threads; i++) {
        args_workers[i].in = in;
        args_workers[i].out = out;
        args_workers[i].chunks = chunks;
        args_workers[i].mutex = &mutex;
        
        pthread_create(&worker_threads[i], NULL, worker, (void *)&args_workers[i]);
    }

    args_writer.out = out;
    args_writer.opt = opt;
    args_writer.chunks = chunks;
        
    pthread_create(&writer_thread, NULL, writer, (void *)&args_writer);

    pthread_join(reader_thread, NULL);

    for (i = 0; i < opt.num_threads; i++) {
        pthread_join(worker_threads[i], NULL);
    }

    pthread_join(writer_thread, NULL);

    
    close(fd);
    q_destroy(in);
    q_destroy(out);
    pthread_mutex_destroy(&mutex);
  
}


// Decompress file taking chunks of size opt.size from the input file

void decomp(struct options opt) {
    int fd, i;
    char uncomp_file[256];
    archive ar;
    chunk ch, res;

    if((ar=open_archive_file(opt.file))==NULL) {
        printf("Cannot open archive file\n");
        exit(0);
    };

    if(opt.out_file) {
        strncpy(uncomp_file, opt.out_file, 255);
    } else {
        strncpy(uncomp_file, opt.file, strlen(opt.file) -3);
        uncomp_file[strlen(opt.file)-3] = '\0';
    }

    if((fd=open(uncomp_file, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))== -1) {
        printf("Cannot create %s: %s\n", uncomp_file, strerror(errno));
        exit(0);
    }

    for(i=0; i<chunks(ar); i++) {
        ch = get_chunk( ar, i);

        res = zdecompress(ch);
        free_chunk(ch);

        lseek(fd, res->offset, SEEK_SET);
        write(fd, res->data, res->size);
        free_chunk(res);
    }

    close_archive_file(ar);
    close(fd);
}

int main(int argc, char *argv[]) {
    struct options opt;

    opt.compress    = COMPRESS;
    opt.num_threads = 6;
    opt.size        = CHUNK_SIZE;
    opt.queue_size  = QUEUE_SIZE;
    opt.out_file    = NULL;

    read_options(argc, argv, &opt);

    if(opt.compress == COMPRESS) comp(opt);
    else decomp(opt);
}
