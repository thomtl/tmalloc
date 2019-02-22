/* 
   Written by thom_tl in 2019
   Please set the settings according to your project
   And please don't use this code in anything serious, just use dlmalloc or something
*/
#ifndef TMALLOC
#define TMALLOC

// Use the mmap function instead of sbrk
#define USE_MMAP

// Use spinlocks to keep the allocator thread-safe
#define MULTITHREADING_PROTECTION

// Set errno if something goes wrong
#define USE_ERRNO

/* Use allocation type
   0 = first fit
   1 = best fit
*/
#define ALLOCATION_TYPE 1

#if (ALLOCATION_TYPE > 1)
#error "Undefined allocation type"
#endif

#include <stdio.h>
#include <stddef.h> // size_t
#include <string.h> // memset / memcpy
#include <stdint.h> // uint32_t / uint64_t
#include <stdbool.h> // bool

#ifdef USE_MMAP
#include <sys/mman.h> // mmap
#else
#include <unistd.h> // sbrk
#endif

#ifdef MULTITHREADING_PROTECTION
#include <pthread.h>

#define tmalloc_mutex_t pthread_mutex_t
#define tmalloc_multithreading_lock(a) (pthread_mutex_lock(a))
#define tmalloc_multithreading_unlock(a) (pthread_mutex_unlock(a))
#else
#define tmalloc_mutex_t int
#define tmalloc_multithreading_lock(a) ((void)(a))
#define tmalloc_multithreading_unlock(a) ((void)(a))
#endif

#ifdef USE_ERRNO
#include <errno.h>
#define TMALLOC_ENOMEM ENOMEM
#else
#define TMALLOC_ENOMEM 0
#endif

#define TMALLOC_MAGIC 0xBEEFCAFE

struct tmalloc_header {
    size_t size;
    bool is_free;
    uint32_t magic;
    struct tmalloc_header* next;
    uint64_t off_by_one_error_protection;
};

void* tmalloc(size_t size);
void tfree(void* ptr);
void* tcalloc(size_t nmemb, size_t size);
void* trealloc(void* ptr, size_t size);
void* treallocarray(void* ptr, size_t nmemb, size_t size);

#endif