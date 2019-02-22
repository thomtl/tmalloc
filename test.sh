gcc -c tmalloc.c -o tmalloc.o -Wall -Wextra -Werror
gcc -c test.c -o test.o -Wall -Wextra -Werror
gcc -o tmalloc tmalloc.o test.o
./tmalloc

rm tmalloc.o test.o tmalloc
