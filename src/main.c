#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define PORT       8080
#define MAX_EVENTS 10

static const char RESPONSE[] = "HTTP/1.1 200 OK\r\n"
                               "Content-Type: text/plain\r\n"
                               "Content-Length: 13\r\n"
                               "\r\n"
                               "Hello, World!";

int
set_nonblocking(int sockfd)
{
  int flags = fcntl(sockfd, F_GETFL, 0);
  if (flags == -1)
  {
    perror("fcntl");
    return -1;
  }
  if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
  {
    perror("fcntl");
    return -1;
  }
  return 0;
}

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

  if (set_nonblocking(sockfd) < 0)
  {
    perror("set_nonblocking");
    return 1;
  }

  int epoll_fd = epoll_create1(0);
  if (epoll_fd < 0)
  {
    perror("epoll_create1");
    return 1;
  }

  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = sockfd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &event) < 0)
  {
    perror("epoll_ctl");
    return 1;
  }

  struct epoll_event events[MAX_EVENTS];
  printf("Server listening on port %d\n", PORT);

  while (1)
  {
    int n_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    if (n_fds < 0)
    {
      if (errno == EINTR)
      {
        continue; // Interrupted by signal, retry
      }
      perror("epoll_wait");
      return 1;
    }

    for (int i = 0; i < n_fds; i++)
    {
      int current_fd = events[i].data.fd;
      uint32_t current_events = events[i].events;

      // accept new connections
      if (current_fd == sockfd)
      {
        while (1)
        {
          struct sockaddr_in client_addr;
          socklen_t client_len = sizeof(client_addr);
          int client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
          if (client_fd < 0)
          {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
              // No more incoming connections
              break;
            }
            perror("accept");
            return 1;
          }

          if (set_nonblocking(client_fd) < 0)
          {
            perror("set_nonblocking");
            close(client_fd);
            continue;
          }

          struct epoll_event client_event;
          client_event.events = EPOLLIN | EPOLLET; // Edge-triggered
          client_event.data.fd = client_fd;
          if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) < 0)
          {
            perror("epoll_ctl");
            close(client_fd);
            continue;
          }

          printf("Accepted connection on fd %d\n", client_fd);
        }
      }
      // error / hangup events
      else if (current_events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
      {
        fprintf(stderr, "epoll error on fd %d\n", current_fd);
        close(current_fd);
      }
      // read data
      else if (current_events & EPOLLIN)
      {
        char buffer[1024];
        ssize_t bytes_read = recv(current_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read > 0)
        {
          buffer[bytes_read] = '\0';

          if (strstr(buffer, "\r\n\r\n") != NULL)
          {
            // Send HTTP response
            send(current_fd, RESPONSE, sizeof(RESPONSE) - 1, 0);

            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
            close(current_fd);
          }
        }
        else if (bytes_read == 0)
        {
          // Client closed connection
          epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
          close(current_fd);
        }
        else
        {
          if (errno != EAGAIN && errno != EWOULDBLOCK)
          {
            perror("recv");
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
            close(current_fd);
          }
        }
      }
    }
  }

  close(epoll_fd);
  close(sockfd);

  return 0;
}