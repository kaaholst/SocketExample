#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define PORT 50124

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

void *reading_thread(void *arg) {
  int socket = *(int*)arg;
  FILE *in = fdopen(socket, "r");
  char line[256];

  printf("reading thread started\n");

  while (fgets(line, sizeof(line), in)) {
    printf("%s", line);
  }

  close(socket);
}

int main(int argc, char *argv[]) {
  char *hostname = "127.0.0.1";
  char msg[81] = "-";
  int port = PORT;

  if (argc > 1) {
    char * p = argv[1];
    hostname = strsep(&p, ":");
    if (p) port = atoi(p);
  }
  if (argc > 2) {
    strncpy(msg, argv[2], 80);
    msg[80] = '\0';
  }

  printf("client\n");
  int socket = connect_to(hostname, port);
  if (socket < 0) {
    fprintf(stderr, "client can't connect: %m\n");
    return 1;
  }

  printf("client connected\n");
  pthread_t thread_id;
  if (pthread_create(&thread_id, NULL, reading_thread, &socket)) {
      fprintf(stderr, "Unable to create reading thread %m\n");
      exit(2);
  }

  if (strcmp("-", msg)) {
    dprintf(socket, "%s\n", msg);
  } else {
    while (fgets(msg, 80, stdin)) {
      msg[strcspn(msg, "\n")] = 0;
      dprintf(socket, "%s\n", msg);
    }
  }

  close(socket);
}
