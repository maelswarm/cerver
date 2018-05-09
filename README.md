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
