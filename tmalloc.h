#ifndef TMALLOC
#define TMALLOC

#define USE_MMAP

#define MULTITHREADING_PROTECTION

#define USE_ERRNO

#ifdef RHINO_KERNEL

#include <rhino/common.h>

#else

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#ifdef USE_MMAP
#include <sys/mman.h>
#else
#include <unistd.h>
#endif

#ifdef MULTITHREADING_PROTECTION
#include <pthread.h>
#define tmalloc_multithreading_lock(a) (pthread_mutex_lock(a))
#define tmalloc_multithreading_unlock(a) (pthread_mutex_unluck(a))
#else
#define tmalloc_multithreading_lock(a) ((void)(a))
#define tmalloc_multithreading_unlock(a) ((void)(a))
#endif

#endif

void* tmalloc(size_t size);
void  tfree(void* ptr);
void* tcalloc(size_t nmemb, size_t size);
void* trealloc(void* ptr, size_t size);
void* treallocarray(void* ptr, size_t nmemb, size_t size);

#endif
