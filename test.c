#include "tmalloc.h"

#include <stdio.h>

int main(void){
    void* ptr = tmalloc(0x8);
    printf("PTR: %p  \n", ptr);
    void* t = tmalloc(0x4);
    trealloc(ptr, 0x16);
    printf("PTR: %p  \n", ptr);
    tfree(ptr);
    tfree(t);

    return 0;
}