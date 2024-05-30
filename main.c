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
#include <sys/sysinfo.h>
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
void send_ok(SSL *, int, char *, _Bool);
void *connection_handler(void *);

SSL_CTX *ctx;
HMAP_PTR routeMap;

int alive;
volatile int socks[100000];
volatile int socksCount;
pthread_mutex_t lock;
pthread_cond_t cond;

typedef struct Node Node;

struct Node {
  Node *next;
  int fd;
};

Node * volatile firstNode;
Node * volatile lastNode;

Node *createNode(int fd) {
  Node *newNode = (Node *)malloc(sizeof(Node));
  newNode->fd = fd;
  newNode->next = NULL;
  return newNode;
}

typedef struct shared {
  pthread_mutex_t lock;
  pthread_cond_t cond;
} Shared;

off_t fsize(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

void init_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

SSL_CTX *create_context() {
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

void configure_context(SSL_CTX *ctx) {
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
    alive = 0;
    usleep(1000);
    hmap_free(routeMap, 0);
    SSL_CTX_free(ctx);
    EVP_cleanup();
    exit(0);
}

int main(int argc, char *argv[])
{
    printf("Number of CPUs %i\n", get_nprocs_conf());
    signal(SIGINT, intHandler);
    alive = 1;
    socksCount = 0;
    pthread_mutex_init(&lock, 0);
    pthread_cond_init(&cond, NULL);
    //pthread_mutex_init(&lock, 0);
    //pthread_cond_init(&cond, NULL);
    firstNode = NULL;
    lastNode = NULL;
    int socket_desc, client_sock, c, *new_sock;
    struct sockaddr_in server, client;

    init_openssl();
    ctx = create_context();
    configure_context(ctx);
    if (!SSL_CTX_check_private_key(ctx)) {
        abort();
    }

    construct_routes();

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        printf("Could not create socket");
        abort();
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("0.0.0.0");
    server.sin_port = htons(443);

    if(bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("bind failed. Error");
        return 1;
    }

    if(listen(socket_desc, 3) < 0) {
      perror("sockdesc listen failed:");
    }

    int numOfProc = get_nprocs_conf();
    pthread_t threads[numOfProc * 2];
    for(int x = 0; x < numOfProc * 2; x++) {
      pthread_create(&threads[x], NULL, connection_handler, NULL);
    }

    c = sizeof(struct sockaddr_in);
    while ((client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t *)&c)) > -1) {
      fcntl(client_sock, F_SETFL, O_NONBLOCK);
      pthread_mutex_lock(&lock);
      if(firstNode != NULL) {
        lastNode->next = createNode(client_sock);
        lastNode = lastNode->next;
      } else {
        lastNode = createNode(client_sock);
        firstNode = lastNode;
      }
      pthread_cond_signal(&cond);
      pthread_mutex_unlock(&lock);
    }

    alive = 0;
    usleep(1000);

    hmap_free(routeMap, 0);
    SSL_CTX_free(ctx);
    EVP_cleanup();

    if (client_sock < 0) {
        perror("accept failed");
        return 1;
    }
    return 0;
}

void send_entity_too_large(SSL *ssl) {
    char sbuff[500];
    size_t sbuffSize = sizeof(sbuff);
    memset(sbuff, '\0', sbuffSize);

    strncat(sbuff, "HTTP/1.1 413 Entity Too Large\r\n", sbuffSize);
    strncat(sbuff, "Connection: Closed\r\n", sbuffSize);
    strncat(sbuff, "Content-Length: 42\r\n\r\n", sbuffSize);
    strncat(sbuff, "<html><body>Entity Too Large</body></html>", sbuffSize);
    int ret = 0, offset = 0;
    while(1) {
      ret = SSL_write(ssl, &sbuff[offset], strlen(sbuff) - offset);
      if(ret > -1) {
        offset += ret;
      } else {
	      break;
      }
      if(offset >= 42) {
        //printf("Sent %i:bytes\n");
        break;
      }
    }
}

void send_bad_request(SSL *ssl) {
    char sbuff[500];
    size_t sbuffSize = sizeof(sbuff);
    memset(sbuff, '\0', sbuffSize);

    strncat(sbuff, "HTTP/1.1 400 Bad Request\r\n", sbuffSize);
    strncat(sbuff, "Connection: Closed\r\n", sbuffSize);
    strncat(sbuff, "Content-Length: 37\r\n\r\n", sbuffSize);
    strncat(sbuff, "<html><body>Bad Request</body></html>", sbuffSize);
    int ret = 0, offset = 0;
    while(1) {
      ret = SSL_write(ssl, &sbuff[offset], strlen(sbuff) - offset);
      if(ret > -1) {
        offset += ret;
      } else {
	      break;
      }
      if(offset >= 37) {
        //printf("Sent %i:bytes\n");
        break;
      }
    }
}

void send_not_found(SSL *ssl) {
    char sbuff[500];
    size_t sbuffSize = sizeof(sbuff);
    memset(sbuff, '\0', sbuffSize);

    strncat(sbuff, "HTTP/1.1 404 Not Found\r\n", sbuffSize);
    strncat(sbuff, "Connection: Closed\r\n", sbuffSize);
    strncat(sbuff, "Content-Length: 35\r\n\r\n", sbuffSize);
    strncat(sbuff, "<html><body>Not Found</body></html>", sbuffSize);
    int ret = 0, offset = 0;
    while(1) {
      ret = SSL_write(ssl, &sbuff[offset], strlen(sbuff) - offset);
      if(ret > -1) {
        offset += ret;
      } else {
	      break;
      }
      if(offset >= 35) {
        //printf("Sent %i:bytes\n");
        break;
      }
    }
}

void send_ok(SSL *ssl, int fileSize, char *fileContent, _Bool persistent) {
    char sbuff[fileSize+1000];
    size_t sbuffSize = sizeof(sbuff);
    memset(sbuff, '\0', sbuffSize);

    snprintf(sbuff, sbuffSize, "%s%i", "HTTP/1.1 200 OK\r\nContent-Length: ", fileSize); //33
    strncat(sbuff, "\r\nConnection: closed\r\n\r\n", sbuffSize); //24
    strncat(sbuff, fileContent, sbuffSize);
    int ret = 0, offset = 0;
    while(1) {
      ret = SSL_write(ssl, &sbuff[offset], strlen(sbuff) - offset);
      if(ret > -1) {
        offset += ret;
      } else {
	      break;
      }
      if(offset >= fileSize) {
        //printf("Sent %i:bytes\n");
        break;
      }
    }
}

void *connection_handler(void *thread_share) {
  while(alive) {
    SSL *ssl = SSL_new(ctx);
    pthread_mutex_lock(&lock);
    while(firstNode == NULL) {
      pthread_cond_wait(&cond, &lock);
    }
    if(firstNode == NULL) {continue;}
    int sock = firstNode->fd;
    Node *tmp = firstNode->next;
    free(firstNode);
    firstNode = tmp;
    pthread_mutex_unlock(&lock);
    SSL_set_fd(ssl, sock);
    int n = 0, offset = 0;
    char rbuff[8000];
    memset(rbuff, '\0', sizeof(rbuff));
    BIO *accept_bio = BIO_new_socket(sock, BIO_CLOSE);
    SSL_set_bio(ssl, accept_bio, accept_bio);
    int connect = SSL_accept(ssl);
    int error = SSL_get_error(ssl, connect);
    while(1) {
      n = SSL_read(ssl, &rbuff[offset], sizeof(rbuff) - offset);
      if(n > -1) {
        offset += n;
      }
      error = SSL_get_error(ssl, n);
      if(error == SSL_ERROR_WANT_READ) {
        continue;
      } else if(error == 5) {
        break;
      }
      rbuff[offset] = '\0';
      void *start = strstr(rbuff, "HTTP");
      void *end = strstr(rbuff, "/");
      if((start == NULL || end == NULL || start <= end)) {
        if(error == SSL_ERROR_NONE) {
          send_bad_request(ssl);
          break;
        }
        continue;
      }
      int len = start - end - 1;
      char reqRoute[len + 1];
      memset(reqRoute, '\0', sizeof(reqRoute));
      strlcpy(reqRoute, &rbuff[strcspn(rbuff, " ") + 1], len + 1);
      char fileName[1000];
      memset(fileName, '\0', sizeof(fileName));
      char *tmp = hmap_get(routeMap, reqRoute);
      if(tmp == NULL) {
        if(error == SSL_ERROR_NONE) {
          send_not_found(ssl);
          break;
        }
        continue;
      }
      strlcpy(fileName, tmp, sizeof(fileName));
      int fileSize = fsize(fileName);
      if(fileSize != -1) {
        char fileContent[fileSize];
        memset(fileContent, '\0', sizeof(fileContent));
        FILE *fp;
        if (!(fp = fopen(fileName, "r"))) {
          perror(fileName);
          break;
        }
        if(fread(fileContent, 1, fileSize, fp) < 0) {
          perror("fread failed to read:");
          break;
        }
        fclose(fp);
        if(error == SSL_ERROR_NONE) {
          send_ok(ssl, fileSize, fileContent, 0);
          break;
        }
      }
      break;
    }
    SSL_free(ssl);
    close(sock);

    if(n < 0) {
      perror("recv failed");
    }
  }
  return 0;
}
