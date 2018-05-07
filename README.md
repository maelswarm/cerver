# cerver
HTTPS Server written in C

This project is in development... It's not ready for a production environment.

## Install

````
gcc -std=c11 -c hashmap.c hashmap.h
gcc -std=c11 main.c hashmap.o -o cerver -lssl -lcrypto -lpthread
````

## Usage

Name the SSL key "key.pem" and certificate "cert.pem".

Edit routes in main.c

````c
void construct_routes() {
  routeMap = hmap_create(0,1.0);
  hmap_set(routeMap, "/", "./build/home.html");
  hmap_set(routeMap, "/main.js", "./build/main.js");
  //add routes...
}
````
