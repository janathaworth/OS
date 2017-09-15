all: yash 

yash: main.o 
	gcc main.o -o yash

main.o: main.c
	gcc -c main.c



