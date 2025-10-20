#define _POSIX_C_SOURCE 199309L
#include <openssl/md5.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <sys/mman.h>

#include "hash.h"

#define BUFFER_SIZE 32768

struct Result {
    double time;
    uint64_t hash;
};

int openDescriptor(const char *file) {
    int fd = open(file, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "ERROR: Invalid open file");
        exit(EXIT_FAILURE);
    }
    return fd;
}

size_t getNumBlocks(size_t file_size){
    return (file_size + BUFFER_SIZE - 1) / BUFFER_SIZE - 1;
}

void readFileSequentially(int fd, uint64_t* hash, __attribute__((unused)) size_t file_size){
    char buffer[BUFFER_SIZE];
    ssize_t bytes;
    while ((bytes = read(fd, buffer, BUFFER_SIZE)) > 0){
        *hash = ul_crc64_update(buffer, bytes, *hash);
    }
}

void readFileRandom(int fd, uint64_t* hash, size_t file_size){
    char buffer[BUFFER_SIZE];
    bool fromBegging = true;
    size_t current = 0;
    size_t begin = 0;
    size_t end = getNumBlocks(file_size);
    ssize_t bytes;

    while(begin <= end) {
        if (fromBegging) {
            current = begin;
            begin = begin +1;
        } else {
            current = end;
            end--;
        }
        fromBegging = !fromBegging;
        lseek(fd, current * BUFFER_SIZE, SEEK_SET);
        bytes = read(fd, buffer, BUFFER_SIZE);
        *hash = ul_crc64_update(buffer, bytes, *hash);
    }
}

char* createMmap(int fd, size_t file_size){
    char* file = (char*)mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file == MAP_FAILED) {
        fprintf(stderr, "ERROR: Invalid mmap %zu", file_size);\
        close(fd);
        return NULL;
    }
    return file;
} 

void mmapFileSequentially(int fd, uint64_t* hash, size_t file_size){
    char* file = createMmap(fd, file_size);
    if (file == NULL) return;
    size_t num_blocks = getNumBlocks(file_size); 
    size_t offset = 0;
    for(size_t i = 0; i <= num_blocks; i++) {
        size_t block_size = BUFFER_SIZE;
        if (offset + BUFFER_SIZE > file_size) {
            block_size = file_size - offset;
        }
        *hash = ul_crc64_update(file + offset, block_size, *hash);
        offset += block_size;
    }
    munmap(file, file_size);
}

void mmapFileRandom(int fd, uint64_t* hash, size_t file_size){
    char* file = createMmap(fd, file_size);
    if (file == NULL) return;

    bool fromBegging = true;
    size_t current = 0;
    size_t begin = 0;
    size_t end = getNumBlocks(file_size);

    while(begin <= end) {
        if (fromBegging) {
            current = begin;
            begin++;
        } else {
            current = end;
            end--;
        }
        fromBegging = !fromBegging;
        size_t offset = current * (size_t)BUFFER_SIZE;
        size_t block_size = BUFFER_SIZE;
        if (offset + BUFFER_SIZE > file_size) {
            block_size = file_size - offset;
        }
        *hash = ul_crc64_update(file + offset, block_size, *hash);
    }
    munmap(file, file_size);
}

struct Result measureTimeRun( const char* file, void (*f)(int, size_t*, size_t)){

    int fd = openDescriptor(file);
    struct timespec start, end;
    struct stat info;
    fstat(fd, &info);

    uint64_t crc = 0;

    clock_gettime(CLOCK_MONOTONIC, &start);
    (*f)(fd, &crc, info.st_size);
    clock_gettime(CLOCK_MONOTONIC, &end);


    struct Result result;
    result.hash = crc; 
    result.time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 10e9;

    close(fd);
    return result;
} 

int main(int argc, char* argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file_path>\n", argv[0]);
        return 1;
    }
    const char *file_path = argv[1];

    struct Result result;
    result = measureTimeRun(file_path, readFileSequentially);
    printf("Sequential read : Time %.9f seconds, Hash: %" PRId64 "\n", result.time, result.hash);

    result = measureTimeRun(file_path, readFileRandom);
    printf("Random     read : Time %.9f seconds, Hash: %" PRId64 "\n", result.time, result.hash);

   result = measureTimeRun(file_path, mmapFileSequentially);
    printf("Sequential mmap : Time %.9f seconds, Hash: %" PRId64 "\n", result.time, result.hash);

    result = measureTimeRun(file_path, mmapFileRandom);
    printf("Random     mmap : Time %.9f seconds, Hash: %" PRId64 "\n", result.time, result.hash);
}
