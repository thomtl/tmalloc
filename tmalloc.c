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

static void* tmalloc_morecore(void* addr, intptr_t increment, bool* mmapped){
    #ifdef USE_MMAP
    if(increment == 0) return sbrk(0);
    if(increment < 0){
        if(-increment >= TMALLOC_MMAP_THRESHOLD){
            int mapping = munmap(addr, -increment);
            return (mapping == -1) ? ((void*)0) : ((void*)1);
        } else {
            return sbrk(increment);
        }
    }

    if(increment >= TMALLOC_MMAP_THRESHOLD){
        void* mapping = mmap(NULL, increment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        *mmapped = true;
        return (mapping == MAP_FAILED) ? (NULL) : (mapping);
    } else {
        void* mapping = sbrk(increment);
        *mmapped = false;
        return (mapping == (void*)-1) ? (NULL) : (mapping);
    }
    #else
    (void)(mapped);
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
    bool mmapped = false;
    void* block = tmalloc_morecore(NULL, total_size, &mmapped);
    if(block == NULL){
        tmalloc_multithreading_unlock(&tmalloc_global_mutex);
        tmalloc_set_errno(TMALLOC_ENOMEM);
        return NULL;
    }

    header = block;
    header->size = size;
    header->is_free = false;
    header->next = NULL;
    header->magic = TMALLOC_MAGIC;

    uint32_t flags = 0;
    if(mmapped) flags |= TMALLOC_HEADER_FLAGS_MMAP;
    header->flags = flags;

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
    if(header->magic != TMALLOC_MAGIC){
        perror("tmalloc.c: header->magic is not equal to TMALLOC_MAGIC\n");
        return;
    }

    bool trash;

    #ifdef USE_MMAP

    if(header->flags & TMALLOC_HEADER_FLAGS_MMAP){
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
        
        tmalloc_morecore(header, 0 - sizeof(struct tmalloc_header) - header->size, &trash);
    } else {
        void* program_break = tmalloc_morecore(NULL, 0, &trash);
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
            tmalloc_morecore(NULL, 0 - sizeof(struct tmalloc_header) - header->size, &trash);
            tmalloc_multithreading_unlock(&tmalloc_global_mutex);
            return;
        }

        header->is_free = true;
    }

    return;

    #else
    void* program_break = tmalloc_morecore(NULL, 0, &trash);
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
        tmalloc_morecore(NULL, 0 - sizeof(struct tmalloc_header) - header->size, &trash);
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