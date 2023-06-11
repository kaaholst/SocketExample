#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

void on_disconnect(int socket);
void on_message(int socket, char *msg);
void on_response(int client, char *msg);

/* Connect to specified port, which is supposed to be listening */
int connect_to(char *hostname, int port) {
  struct sockaddr_in sin;
  struct hostent *host;
  int sock, arg = 1;

  if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    return -1;
  }

  sin.sin_family = AF_INET;
  if (!inet_aton(hostname, &sin.sin_addr)) {
    if ((host = gethostbyname(hostname)) == 0) {
      close(sock);
      return -1;
    }
    sin.sin_addr = *(struct in_addr *) host->h_addr;
  }
  sin.sin_port = htons(port);

  if (connect(sock, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
    close(sock);
    return -1;
  }

  return sock;
}

/* Setup a filedescriptor to listen for connectionrequests on specified port */
int setup_listener(int port) {
  struct sockaddr_in sin;
  int sock, arg = 1;

  if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    return -1;

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) < 0)
    return -1;
  if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &arg, sizeof(arg)) < 0)
    return -1;

  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(sock, (struct sockaddr *) &sin, sizeof(sin)) < 0)
    return -1;

  if (listen(sock, 64) < 0)
    return -1;

  return sock;
}

void *client_thread(void *arg) {
  int socket = *(int*)arg;
  FILE *in = fdopen(socket, "r");
  char line[256];

  printf("%d - client thread started\n", socket);
  dprintf(socket, "Accepted\n");

  while (fgets(line, sizeof(line), in)) {
    printf("%d - %s", socket, line);
    on_message(socket, line);
  }

  close(socket);
  on_disconnect(socket);
}

int accept_connection(struct pollfd *pfd) {
  if ((pfd->revents & POLLIN) !=0) {
    int *socket = malloc(sizeof(int));
    *socket = accept(pfd->fd, NULL, NULL);
    if (*socket != -1) {
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, client_thread, socket)) {
            fprintf(stderr, "Unable to create client thread %m\n");
            return -1;
        }
    } else {
      fprintf(stderr, "Accept error - %m\n");
    }
    return *socket;
  }
  return -1;
}

void *reading_thread(void *arg) {
  int socket = *(int*)arg;
  FILE *in = fdopen(socket, "r");

  printf("reading thread started\n");

  uint8_t client;
  char line[256];
  while (1) {
    uint8_t client = getc(in);
    if (client == EOF) {
      break;
    }
    if (!fgets(line, sizeof(line), in)) {
      break;
    }
    on_response(client, line);
  }

  close(socket);
}


static int server;
static uint8_t nclient = 0;
static int *clients;

void on_disconnect(int socket) {
  for (uint8_t client = 0; client < nclient; client++) {
    if (clients[client] == socket) {
      nclient--;
      int *new_clients = malloc(nclient * sizeof(int));
      if (client > 0) {
        memcpy(new_clients, clients, (client) * sizeof(int));
      }
      if (client < nclient) {
        memcpy(&new_clients[client], &clients[client+1], (nclient - client) * sizeof(int));
      }
      clients = new_clients;
      return;
    }
  }
}

void on_message(int socket, char *msg) {
  for (uint8_t client = 0; client < nclient; client++) {
    if (clients[client] == socket) {
      write(server, &client, 1);
      dprintf(server, "%s", msg);
    }
  }
}

void on_response(int client, char *msg) {
  dprintf(clients[client], "%s", msg);
}

#define SERVER_PORT 50123
#define LISTEN_PORT 50124

int main(int argc, char *argv[]) {
  printf("router\n");

  struct pollfd pfds[1];
  int timeout = -1;

  pfds[0].fd = setup_listener(LISTEN_PORT);
  pfds[0].events = POLLIN;

  server = connect_to("127.0.0.1", SERVER_PORT);
  if (server < 0) {
    fprintf(stderr, "router can't connect: %m\n");
    return 1;
  }

  printf("router connected\n");
  pthread_t thread_id;
  if (pthread_create(&thread_id, NULL, reading_thread, &server)) {
      fprintf(stderr, "Unable to create reading thread %m\n");
      exit(2);
  }


  printf("router listening\n");
  while (1) {
    if (poll(pfds, 1, timeout) < 0) {
      fprintf(stderr, "poll: %m\n");
    }

    for (struct pollfd *cli = pfds; cli <= &pfds[0]; cli++) {
      int client = accept_connection(cli);
      if (client != -1) {
        int *new_clients = malloc((nclient + 1) * sizeof(int));
        if (nclient > 0) {
          memcpy(new_clients, clients, nclient * sizeof(int));
        }
        clients = new_clients;
        clients[nclient++] = client;
      }
    }
  }
}
