# cerver

This project is in development... It's not ready for a production environment.

## Install

````
git clone git@github.com:roecrew/cerver.git
openssl req -newkey rsa:2048 -nodes -keyout key.pem -x509 -days 365 -out cert.pem
make
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

## Load Test Findings

Machine type
* custom (4 vCPU, 8.0 GB memory)

CPU platform
* Intel Sandy Bridge

Zone
* us-central1-a

Requests per Page Load
* 6

Page Size
* 60.1 kb

<img src="https://i.imgur.com/K39YjFx.png" />

<img src="https://i.imgur.com/KCdBLGr.png" />
