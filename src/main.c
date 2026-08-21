#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 8080

int
main()
{
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    perror("socket");
    return 1;
  }

  int optval = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
  {
    perror("setsockopt");
    return 1;
  }

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
  {
    perror("bind");
    return 1;
  }

  if (listen(sockfd, 10) < 0)
  {
    perror("listen");
    return 1;
  }

  printf("Server listening on port %d\n", PORT);

  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  // block until a client connects
  int connfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
  if (connfd < 0)
  {
    perror("accept");
    return 1;
  }

  printf("Client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

  char buffer[1024];
  ssize_t bytes_read;
  while ((bytes_read = recv(connfd, buffer, sizeof(buffer) - 1, 0)) > 0)
  {
    buffer[bytes_read] = '\0'; // null-terminate the string
    printf("Received: %s", buffer);

    if (strstr(buffer, "\r\n\r\n") != NULL)
    {
      break; // end of HTTP headers
    }
  }

  const char *response = "HTTP/1.1 200 OK\r\n"
                         "Content-Type: text/plain\r\n"
                         "Content-Length: 13\r\n"
                         "\r\n"
                         "Hello, World!";
  send(connfd, response, strlen(response), 0);

  close(connfd);
  close(sockfd);

  return 0;
}