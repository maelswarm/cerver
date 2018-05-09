default: main.c hashmap.c hashmap.h
	gcc -std=c11 -c hashmap.c hashmap.h
	gcc -std=c11 main.c hashmap.o -o cerver -lssl -lcrypto -lpthread
