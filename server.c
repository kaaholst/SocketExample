#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define PORT 50123

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

int main(int argc, char *argv[]) {
  printf("server\n");

  struct pollfd pfds[1];
  int timeout = -1;

  pfds[0].fd = setup_listener(PORT);
  pfds[0].events = POLLIN;

  printf("server listening\n");
  while (1) {
    if (poll(pfds, 1, timeout) < 0) {
      printf("poll: %m\n");
    }

    for (struct pollfd *cli = pfds; cli <= &pfds[0]; cli++) {
      if (cli->revents) {
        int socket = accept(cli->fd, NULL, NULL);

        if (socket != -1) {
          printf("server accepted\n");
          FILE *file = fdopen(socket, "r");
          uint8_t client;
          char line[256];

          while (1) {
            uint8_t client = getc(file);
            if (client == EOF) {
              break;
            }
            if (!fgets(line, sizeof(line), file)) {
              break;
            }
            printf("%d - %s", client, line);
            write(socket, &client, 1);
            dprintf(socket, "echo %s", line);
          }

          close(socket);
        }
        else
          printf("Can't accept: %m\n");
      }
    }
  }
}
