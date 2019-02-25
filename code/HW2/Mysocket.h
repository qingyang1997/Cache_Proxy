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
// using namespace std;
class SocketInfo {
public:
  int socket_fd;
  struct addrinfo host_info;
  struct addrinfo *host_info_list;
  const char *hostname;
  const char *port;
  void setup();
  void wait();
  void acc(int *client_connection_fd);
  void connect_socket();
  void client_setup();
  SocketInfo()
      : socket_fd(0), host_info_list(nullptr), hostname(nullptr),
        port(nullptr) {}
  ~SocketInfo() {
    if (socket_fd == 0) {
      close(socket_fd);
    }
    if (host_info_list != nullptr) {
      free(host_info_list);
    }
  }
};

void SocketInfo::setup() {
  memset(&host_info, 0, sizeof(host_info));

  host_info.ai_family = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;
  host_info.ai_flags = AI_PASSIVE;

  int status = getaddrinfo(hostname, port, &(host_info), &(host_info_list));
  if (status != 0) {
    throw ErrorException("getaddrinfo failure");
  } // if

  socket_fd = socket(host_info_list->ai_family, host_info_list->ai_socktype,
                     host_info_list->ai_protocol);
  if (socket_fd == -1) {
    throw ErrorException("socket() failure");
  } // if
}

void SocketInfo::wait() {
  int yes = 1;
  int status =
      setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  status =
      ::bind(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if (status == -1) {
    throw ErrorException("bind failure");
  } // if
  status = listen(socket_fd, 100);
  if (status == -1) {
    throw ErrorException("listen failure");
  } // if
}

void SocketInfo::acc(int *client_connection_fd) {
  struct sockaddr_storage socket_addr;
  socklen_t socket_addr_len = sizeof(socket_addr);
  *client_connection_fd =
      accept(socket_fd, (struct sockaddr *)&socket_addr, &socket_addr_len);
  if (*client_connection_fd == -1) {
    throw ErrorException("accept failure");
  } // if
}

void SocketInfo::connect_socket() {
  int status =
      connect(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if (status != 0) {
    throw ErrorException("connect failure");
  }
}

void SocketInfo::client_setup() {
  memset(&host_info, 0, sizeof(host_info));

  host_info.ai_family = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;

  int status = getaddrinfo(hostname, port, &(host_info), &(host_info_list));
  if (status != 0) {
    throw ErrorException("getaddrinfo failure");
  } // if

  socket_fd = socket(host_info_list->ai_family, host_info_list->ai_socktype,
                     host_info_list->ai_protocol);
  if (socket_fd == -1) {
    throw ErrorException("socket() failure");
  } // if
}

#endif // PROXY_MYSOCKET_H
