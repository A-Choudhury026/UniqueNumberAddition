#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define NUM_THREADS 4  //CPU cores
#define MAX_NUMBERS 1000000  // Expected unique numbers

// **Interface for Unique Number Storage**
typedef struct UniqueNumberStore {
    int *numbers;
    size_t size;
    pthread_mutex_t lock;
    
    void (*add_number)(struct UniqueNumberStore*, int);
    int (*is_unique)(struct UniqueNumberStore*, int);
} UniqueNumberStore;

//**Implementation of Unique Number Storage**
void add_number(UniqueNumberStore *store, int num) {
    pthread_mutex_lock(&store->lock);
    if (store->is_unique(store, num)) {
        store->numbers[store->size++] = num;
    }
    pthread_mutex_unlock(&store->lock);
}

int is_unique(UniqueNumberStore *store, int num) {
    for (size_t i = 0; i < store->size; i++) {
        if (store->numbers[i] == num)
            return 0;  // Not unique
    }
    return 1;  // Unique
}

//**File Reader**
typedef struct {
    long start_offset;
    long end_offset;
    char *filename;
} FileReader;

FILE* open_file(FileReader *reader) {
    FILE *file = fopen(reader->filename, "r");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }
    fseek(file, reader->start_offset, SEEK_SET);
    return file;
}

//**Number Processor**
typedef struct {
    FileReader file_reader;
    UniqueNumberStore *store;
} NumberProcessor;

void* process_numbers(void* args) {
    NumberProcessor *processor = (NumberProcessor*)args;
    FILE *file = open_file(&processor->file_reader);
    if (!file) pthread_exit(NULL);

    int num;
    while (ftell(file) < processor->file_reader.end_offset && fscanf(file, "%d", &num) == 1) {
        processor->store->add_number(processor->store, num);
    }

    fclose(file);
    pthread_exit(NULL);
}

// **Thread Manager (Manages Multi-Threading)**
typedef struct {
    pthread_t threads[NUM_THREADS];
    NumberProcessor processors[NUM_THREADS];
    UniqueNumberStore store;
} ThreadManager;

void init_thread_manager(ThreadManager *manager, char *filename, long file_size) {
    long chunk_size = file_size / NUM_THREADS;
    manager->store.numbers = (int*)malloc(MAX_NUMBERS * sizeof(int));
    manager->store.size = 0;
    manager->store.add_number = add_number;
    manager->store.is_unique = is_unique;
    pthread_mutex_init(&manager->store.lock, NULL);

    for (int i = 0; i < NUM_THREADS; i++) {
        manager->processors[i].store = &manager->store;
        manager->processors[i].file_reader.filename = filename;
        manager->processors[i].file_reader.start_offset = i * chunk_size;
        manager->processors[i].file_reader.end_offset = (i == NUM_THREADS - 1) ? file_size : (i + 1) * chunk_size;

        if (pthread_create(&manager->threads[i], NULL, process_numbers, &manager->processors[i]) != 0) {
            perror("Thread creation failed");
            exit(1);
        }
    }
}

void join_threads(ThreadManager *manager) {
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(manager->threads[i], NULL);
    }
}

//**Main Function**
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    struct stat st;
    if (stat(argv[1], &st) != 0) {
        perror("File error");
        return 1;
    }

    ThreadManager manager;
    init_thread_manager(&manager, argv[1], st.st_size);
    join_threads(&manager);

    // Print Unique Numbers
    printf("Unique Numbers:\n");
    for (size_t i = 0; i < manager.store.size; i++) {
        printf("%d ", manager.store.numbers[i]);
    }
    printf("\n");

    free(manager.store.numbers);
    pthread_mutex_destroy(&manager.store.lock);
    return 0;
}
