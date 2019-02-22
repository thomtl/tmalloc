#include "tmalloc.h"

#include <stdio.h>

int main(void){

    void* p[50];

    for(int i = 0; i < 50; i++){
        p[i] = tmalloc(0x8);
    }
    for(int i = 0; i < 50; i++) p[i] = trealloc(p[i], 0x1);

    for(int i = 0; i < 50; i++) tfree(p[i]);

    for(int i = 0; i < 50; i++) printf("%p\n", p[i]);

    for(int i = 0; i < 50; i++){
        for(int j = 0; j < 50; j++){
            if(p[i] == p[j] && i != j) printf("ERRROR");
        }
    }


    return 0;
}