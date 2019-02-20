//
// Created by Duck2 on 2019-02-19.
//

#ifndef PROXY_MYSOCKET_H
#define PROXY_MYSOCKET_H

#include "common.h"
#include <cstring>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
// using namespace std;
struct Socket_Info {
  int socket_fd;
  struct addrinfo host_info;
  struct addrinfo *host_info_list;
  const char *hostname;
  const char *port;
};

typedef struct Socket_Info socket_info_t;

int setup(socket_info_t *socket_info) {
  memset(&socket_info->host_info, 0, sizeof(socket_info->host_info));

  socket_info->host_info.ai_family = AF_UNSPEC;
  socket_info->host_info.ai_socktype = SOCK_STREAM;
  socket_info->host_info.ai_flags = AI_PASSIVE;

  int status =
      getaddrinfo(socket_info->hostname, socket_info->port,
                  &(socket_info->host_info), &(socket_info->host_info_list));
  if (status != 0) {
    //   cout << "Error: cannot get address info for host" << endl;
    return -1;
  } // if

  socket_info->socket_fd = socket(socket_info->host_info_list->ai_family,
                                  socket_info->host_info_list->ai_socktype,
                                  socket_info->host_info_list->ai_protocol);
  if (socket_info->socket_fd == -1) {
    // cout << "Error: cannot create socket" << endl;
    return -1;
  } // if
  return 0;
}

int wait(socket_info_t *socket_info) {
  int yes = 1;
  int status = setsockopt(socket_info->socket_fd, SOL_SOCKET, SO_REUSEADDR,
                          &yes, sizeof(int));
  status = ::bind(socket_info->socket_fd, socket_info->host_info_list->ai_addr,
                  socket_info->host_info_list->ai_addrlen);
  if (status == -1) {
    // cout << "Error: cannot bind socket" << endl;
    return -1;
  } // if
  status = listen(socket_info->socket_fd, 100);
  return status;
}

int acc(socket_info_t *socket_info, int *client_connection_fd) {
  struct sockaddr_storage socket_addr;
  socklen_t socket_addr_len = sizeof(socket_addr);
  *client_connection_fd =
      accept(socket_info->socket_fd, (struct sockaddr *)&socket_addr,
             &socket_addr_len);
  if (*client_connection_fd == -1) {
    // cout << "Error: cannot accept connection on socket" << endl;
    return EXIT_FAILURE;
  } // if
  return EXIT_SUCCESS;
}

int connect_socket(socket_info_t *socket_info) {
  return connect(socket_info->socket_fd, socket_info->host_info_list->ai_addr,
                 socket_info->host_info_list->ai_addrlen);
}
int client_setup(socket_info_t *socket_info) {
  memset(&socket_info->host_info, 0, sizeof(socket_info->host_info));

  socket_info->host_info.ai_family = AF_UNSPEC;
  socket_info->host_info.ai_socktype = SOCK_STREAM;

  int status =
      getaddrinfo(socket_info->hostname, socket_info->port,
                  &(socket_info->host_info), &(socket_info->host_info_list));
  if (status != 0) {
    //    cout << "Error: cannot get address info for host" << endl;
    return -1;
  } // if

  socket_info->socket_fd = socket(socket_info->host_info_list->ai_family,
                                  socket_info->host_info_list->ai_socktype,
                                  socket_info->host_info_list->ai_protocol);
  if (socket_info->socket_fd == -1) {
    //    cout << "Error: cannot create socket" << endl;
    return -1;
  } // if
  return 0;
}

#endif // PROXY_MYSOCKET_H
