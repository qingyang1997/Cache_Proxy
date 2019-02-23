//
// Created by Duck2 on 2019-02-19.
//

#ifndef PROXY_MYSOCKET_H
#define PROXY_MYSOCKET_H
#include "common.h"
#include "myexception.h"
#include <cstring>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

struct Socket_Info {
  int socket_fd;
  struct addrinfo host_info;
  struct addrinfo *host_info_list;
  const char *hostname;
  const char *port;
};

typedef struct Socket_Info socket_info_t;

void setup(socket_info_t *socket_info) {
  memset(&socket_info->host_info, 0, sizeof(socket_info->host_info));

  socket_info->host_info.ai_family = AF_UNSPEC;
  socket_info->host_info.ai_socktype = SOCK_STREAM;
  socket_info->host_info.ai_flags = AI_PASSIVE;

  int status =
      getaddrinfo(socket_info->hostname, socket_info->port,
                  &(socket_info->host_info), &(socket_info->host_info_list));
  if (status != 0) {
    throw ErrorException("getaddrinfo failure");
  } // if

  socket_info->socket_fd = socket(socket_info->host_info_list->ai_family,
                                  socket_info->host_info_list->ai_socktype,
                                  socket_info->host_info_list->ai_protocol);
  if (socket_info->socket_fd == -1) {
    throw ErrorException("socket() failure");
  } // if
}

void wait(socket_info_t *socket_info) {
  int yes = 1;
  int status = setsockopt(socket_info->socket_fd, SOL_SOCKET, SO_REUSEADDR,
                          &yes, sizeof(int));
  status = ::bind(socket_info->socket_fd, socket_info->host_info_list->ai_addr,
                  socket_info->host_info_list->ai_addrlen);
  if (status == -1) {
    throw ErrorException("bind failure");
  } // if
  status = listen(socket_info->socket_fd, 100);
  if (status == -1) {
    throw ErrorException("listen failure");
  } // if
}

void acc(socket_info_t *socket_info, int *client_connection_fd) {
  struct sockaddr_storage socket_addr;
  socklen_t socket_addr_len = sizeof(socket_addr);
  *client_connection_fd =
      accept(socket_info->socket_fd, (struct sockaddr *)&socket_addr,
             &socket_addr_len);
  if (*client_connection_fd == -1) {
    throw ErrorException("accept failure");
  } // if
}

void connect_socket(socket_info_t *socket_info) {
  int status =
      connect(socket_info->socket_fd, socket_info->host_info_list->ai_addr,
              socket_info->host_info_list->ai_addrlen);
  if (status != 0) {
    throw ErrorException("connect failure");
  }
}

void client_setup(socket_info_t *socket_info) {
  memset(&socket_info->host_info, 0, sizeof(socket_info->host_info));

  socket_info->host_info.ai_family = AF_UNSPEC;
  socket_info->host_info.ai_socktype = SOCK_STREAM;

  int status =
      getaddrinfo(socket_info->hostname, socket_info->port,
                  &(socket_info->host_info), &(socket_info->host_info_list));
  if (status != 0) {
    throw ErrorException("getaddrinfo failure");
  } // if

  socket_info->socket_fd = socket(socket_info->host_info_list->ai_family,
                                  socket_info->host_info_list->ai_socktype,
                                  socket_info->host_info_list->ai_protocol);
  if (socket_info->socket_fd == -1) {
    throw ErrorException("socket() failure");
  } // if
}

#endif // PROXY_MYSOCKET_H
