default: main.c hashmap.c
	gcc -std=c11 -c hashmap.c
	gcc -std=c11 main.c hashmap.o -o cerver -lssl -lcrypto -lpthread
