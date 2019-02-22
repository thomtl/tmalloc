#include "tmalloc.h"

tmalloc_mutex_t tmalloc_global_mutex;
struct tmalloc_header* head;
struct tmalloc_header* tail;

static void tmalloc_set_errno(int err_code){
    #ifdef USE_ERRNO
    errno = err_code;
    #else
    (void)(err_code);
    #endif
}

static void* tmalloc_morecore(void* addr, intptr_t increment){
    #ifdef USE_MMAP
    if(increment == 0) return NULL;
    if(increment < 0){
        int ret = munmap(addr, -increment);
        return (ret == -1) ? (NULL) : ((void*)1);
    }
    void* mapping = mmap(NULL, increment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (mapping == MAP_FAILED) ? (NULL) : (mapping);
    #else
    (void)(addr);
    void* mapping = sbrk(increment);
    return (mapping == (void*)-1) ? (NULL) : (mapping);
    #endif
}

static struct tmalloc_header* tmalloc_get_free_block(size_t size){
    #if (ALLOCATION_TYPE == 0) // first fit
    struct tmalloc_header* curr = head;
    while(curr){
        if(curr->is_free && curr->size >= size) return curr;
        curr = curr->next;
    }
    return NULL;
    #elif (ALLOCATION_TYPE == 1) // best fit
    struct tmalloc_header* curr = head;
    struct tmalloc_header* tmp = NULL;
    while(curr){
        if(curr->is_free && curr->size == size) return curr;
        if(curr->is_free && curr->size > size) tmp = curr;
        curr = curr->next;
    }
    return tmp;
    #else
    (void)(size);
    perror("tmalloc.c: undefined allocation type\n");
    return NULL;
    #endif
}

void* tmalloc(size_t size){
    if(!size) return NULL;
    
    tmalloc_multithreading_lock(&tmalloc_global_mutex);

    struct tmalloc_header* header = tmalloc_get_free_block(size);
    if(header){
        header->is_free = false;
        tmalloc_multithreading_unlock(&tmalloc_global_mutex);
        return (void*)(header + 1);
    }

    size_t total_size = size + sizeof(struct tmalloc_header);
    void* block = tmalloc_morecore(NULL, total_size);
    if(block == NULL){
        tmalloc_multithreading_unlock(&tmalloc_global_mutex);
        tmalloc_set_errno(TMALLOC_ENOMEM);
        return NULL;
    }

    header = block;
    header->size = size;
    header->is_free = false;
    header->next = NULL;
    if(!head) head = header;
    if(tail) tail->next = header;
    tail = header;
    tmalloc_multithreading_unlock(&tmalloc_global_mutex);
    return (void*)(header + 1);
}

void tfree(void* ptr){
    if(!ptr) return;

    tmalloc_multithreading_lock(&tmalloc_global_mutex);
    struct tmalloc_header* header = (struct tmalloc_header*)ptr - 1;

    #ifdef USE_MMAP

    if(header == head) head = header->next;
    else {
        if(header == tail){
            struct tmalloc_header* tmp = head;
            while(tmp){
                if(tmp->next == header){
                    tail = tmp;
                    tmp->next = NULL;
                    break;
                }
                tmp = tmp->next;
            }
        } else {
            struct tmalloc_header* tmp = head;
            while(tmp){
                if(tmp->next == header){
                    tmp->next = header->next;
                    break;
                }
                tmp = tmp->next;
            }
        }
    }

    tmalloc_morecore(header, -(sizeof(struct tmalloc_header) + header->size));

    return;

    #else
    void* program_break = tmalloc_morecore(NULL, 0);
    if((char*)ptr + header->size == program_break){
        if(head == tail) head = tail = NULL;
        else {
            struct tmalloc_header* tmp = head;
            while(tmp){
                if(tmp->next == tail){
                    tmp->next = NULL;
                    tail = tmp;
                }
                tmp = tmp->next;
            }
        }
        tmalloc_morecore(NULL, 0 - sizeof(struct tmalloc_header) - header->size);
        tmalloc_multithreading_unlock(&tmalloc_global_mutex);
        return;
    }

    header->is_free = true;
    #endif

    tmalloc_multithreading_unlock(&tmalloc_global_mutex);
}

void* tcalloc(size_t nmemb, size_t size){
    if(!nmemb || !size) return NULL;

    size_t allocation_size = nmemb * size;
    if(size != (allocation_size / nmemb)) return NULL;

    void* block = tmalloc(allocation_size);
    if(!block) return NULL;

    memset(block, 0, allocation_size);
    return block;
}

void* trealloc(void* ptr, size_t size){
    if(!ptr && !size) return NULL;

    if(size == 0 && ptr != NULL){
        tfree(ptr);
        return NULL;
    }
    if(!ptr) return tmalloc(size);

    struct tmalloc_header* header = (struct tmalloc_header*)ptr - 1;
    if(header->size >= size) return ptr;

    void* ret = tmalloc(size);
    if(ret){
        memcpy(ret, ptr, header->size);
        tfree(ptr);
    }
    return ret;
}


void* treallocarray(void* ptr, size_t nmemb, size_t size){
    size_t alloc_size;
    if(!nmemb || !size) return NULL;

    alloc_size = (nmemb * size);

    if(size != (alloc_size / nmemb)){
        tmalloc_set_errno(TMALLOC_ENOMEM);
        return NULL;
    }

    return trealloc(ptr, alloc_size);
}
