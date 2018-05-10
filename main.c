#define _DEFAULT_SOURCE
#include <arpa/inet.h>
#include <bsd/string.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "hashmap.h"


off_t fsize(const char *);

void init_openssl(void);
void intHandler(int);

void configure_context(SSL_CTX *);
void construct_routes();
void send_not_found(SSL *);
void send_ok(SSL *, int, char *);
void *connection_handler(void *);

SSL_CTX *ctx;
HMAP_PTR routeMap;

off_t fsize(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

void init_openssl()
{
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

SSL_CTX *create_context()
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = SSLv23_server_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
	perror("Unable to create SSL context");
	ERR_print_errors_fp(stderr);
	exit(EXIT_FAILURE);
    }

    return ctx;
}

void configure_context(SSL_CTX *ctx)
{
    SSL_CTX_set_ecdh_auto(ctx, 1);

    if (SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
	exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
	exit(EXIT_FAILURE);
    }
}

void construct_routes() {
  routeMap = hmap_create(0,1.0);
  hmap_set(routeMap, "/", "./build/index.html");
  hmap_set(routeMap, "/main.js", "./build/main-xelem-min.js");
  hmap_set(routeMap, "/main.css", "./build/main.css");
  hmap_set(routeMap, "/SourceSansPro.css", "./build/SourceSansPro.css");
  hmap_set(routeMap, "/logo2.png", "./build/logo2.png");
  //add routes...
}

void intHandler(int x) {
    hmap_free(routeMap, 0);
    SSL_CTX_free(ctx);
    EVP_cleanup();
    exit(0);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, intHandler);
    int socket_desc, client_sock, c, *new_sock;
    struct sockaddr_in server, client;

    init_openssl();
    ctx = create_context();
    configure_context(ctx);
    if (!SSL_CTX_check_private_key(ctx))
    {
        abort();
    }

    construct_routes();

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("0.0.0.0");
    server.sin_port = htons(443);

    if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("bind failed. Error");
        return 1;
    }
    listen(socket_desc, 3);

    c = sizeof(struct sockaddr_in);
    while ((client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t *)&c)))
    {

        pthread_t thread;
        new_sock = malloc(sizeof(client_sock));
        *new_sock = client_sock;

        if (pthread_create(&thread, NULL, connection_handler, (void *)new_sock) < 0)
        {
            perror("could not create thread");
            return 1;
        }
    }

    hmap_free(routeMap, 0);
    SSL_CTX_free(ctx);
    EVP_cleanup();

    if (client_sock < 0)
    {
        perror("accept failed");
        return 1;
    }
    return 0;
}

void send_entity_too_large(SSL *ssl) {
    char sbuff[500];
    size_t sbuffSize = sizeof(sbuff);
    memset(sbuff, '\0', sbuffSize);

    strlcat(sbuff, "HTTP/1.1 413 Entity Too Large\r\n", sbuffSize);
    strlcat(sbuff, "Connection: Closed\r\n", sbuffSize);
    strlcat(sbuff, "Content-Length: 42\r\n\r\n", sbuffSize);
    strlcat(sbuff, "<html><body>Entity Too Large</body></html>", sbuffSize);
    int ret;
    if(ret = SSL_write(ssl, sbuff, strlen(sbuff)) <= 0) {
      printf("Send Error: %i", SSL_get_error(ssl, ret));
    }
}

void send_bad_request(SSL *ssl) {
    char sbuff[500];
    size_t sbuffSize = sizeof(sbuff);
    memset(sbuff, '\0', sbuffSize);

    strlcat(sbuff, "HTTP/1.1 400 Bad Request\r\n", sbuffSize);
    strlcat(sbuff, "Connection: Closed\r\n", sbuffSize);
    strlcat(sbuff, "Content-Length: 37\r\n\r\n", sbuffSize);
    strlcat(sbuff, "<html><body>Bad Request</body></html>", sbuffSize);
    int ret;
    if(ret = SSL_write(ssl, sbuff, strlen(sbuff)) <= 0) {
      printf("Send Error: %i", SSL_get_error(ssl, ret));
    }
}

void send_not_found(SSL *ssl) {
    char sbuff[500];
    size_t sbuffSize = sizeof(sbuff);
    memset(sbuff, '\0', sbuffSize);

    strlcat(sbuff, "HTTP/1.1 404 Not Found\r\n", sbuffSize);
    strlcat(sbuff, "Connection: Closed\r\n", sbuffSize);
    strlcat(sbuff, "Content-Length: 35\r\n\r\n", sbuffSize);
    strlcat(sbuff, "<html><body>Not Found</body></html>", sbuffSize);
    int ret;
    if(ret = SSL_write(ssl, sbuff, strlen(sbuff)) <= 0) {
      printf("Send Error: %i", SSL_get_error(ssl, ret));
    }
}

void send_ok(SSL *ssl, int fileSize, char *fileContent) {
    char sbuff[fileSize+1000];
    size_t sbuffSize = sizeof(sbuff);
    memset(sbuff, '\0', sbuffSize);

    strlcat(sbuff, "HTTP/1.1 200 OK\r\n", sbuffSize);
    strlcat(sbuff, "Content-Length: ", sbuffSize);
    snprintf(sbuff, strlen(sbuff) + 1 + fileSize, "%s%i", sbuff, fileSize);
    strlcat(sbuff, "\r\nConnection: keep-alive\r\n", sbuffSize);
    strlcat(sbuff, "Keep-Alive: timeout=5, max=20\r\n\r\n", sbuffSize);
    strlcat(sbuff, fileContent, sbuffSize);
    int ret;
    if(ret = SSL_write(ssl, sbuff, strlen(sbuff)) <= 0) {
      printf("Send Error: %i", SSL_get_error(ssl, ret));
    }
}

void *connection_handler(void *socket_desc)
{
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, *(int *)socket_desc);
    int sock = *(int *)socket_desc;
    int n = 0, offset = 0;
    char rbuff[8000]; // limit to 8000 bytes
    memset(rbuff, '\0', sizeof(rbuff));

    if (SSL_accept(ssl) <= 0)
    {
        ERR_print_errors_fp(stderr);
    }

    while ((n = SSL_read(ssl, &rbuff[offset], sizeof(rbuff) - offset)) > 0) {
      offset += n;
      if(offset >= sizeof(rbuff) - 1) {
        send_entity_too_large(ssl);
        break;
      }
      rbuff[offset] = '\0';
      if(SSL_get_error(ssl, n) == SSL_ERROR_WANT_READ) {
        continue;
      }
      void *start = strstr(rbuff, "HTTP");
      void *end = strstr(rbuff, "/");
      if(start == NULL || end == NULL || start <= end) {
        send_bad_request(ssl);
        continue;
      }
      int len = start - end - 1;
      char reqRoute[len + 1];
      memset(reqRoute, '\0', sizeof(reqRoute));
      strlcpy(reqRoute, &rbuff[strcspn(rbuff, " ") + 1], len);
      char fileName[1000];
      memset(fileName, '\0', sizeof(fileName));
      char *tmp = hmap_get(routeMap, reqRoute);
      if(tmp == NULL) {
        send_not_found(ssl);
        continue;
      }
      strlcpy(fileName, tmp, sizeof(fileName));
      int fileSize = fsize(fileName);
      char fileContent[fileSize];
      memset(fileContent, '\0', sizeof(fileContent));
      FILE *fp;
      if (!(fp = fopen(fileName, "r"))) {
        perror(fileName);
        break;
      }
      fread(fileContent, 1, fileSize, fp);
      fclose(fp);
      send_ok(ssl, fileSize, fileContent);
    }

    connection_end:

    SSL_free(ssl);
    close(sock);
    free(socket_desc);

    if(n < 0)
    {
        perror("recv failed");
    }
    return 0;
}
