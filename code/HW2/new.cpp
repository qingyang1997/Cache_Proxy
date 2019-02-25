//#include "Cache.h"
#include "Mysocket.h"
#include "Request.h"
#include "Response.h"
#include <mutex>
#include <typeinfo>
#define RECV_LENGTH 2048
#define SEND_LENGTH 2048
#define HEADER_LENGTH 8192

std::mutex mtx;
int uid = 0;
// std::ifstream log;

void readHeader(int read_fd, Http &http) {
  char message[HEADER_LENGTH];
  memset(message, 0, sizeof(message));
  ssize_t recv_bytes = recv(read_fd, &message, sizeof(message), 0);
  if (recv_bytes == -1) {
    throw ErrorException("recv error");
  }
  if (recv_bytes == 0) {
    throw ErrorException("client close socket");
  }

  // only for debugging, so there is no try and catch
  std::string temp(message);
  const std::type_info &type_info = typeid(http);
  if (type_info == typeid(Request)) {
    std::cout << "[DEBUG] REQUEST " << http.getUid() << std::endl;
  } else {
    std::cout << "[DEBUG] RESPONSE " << http.getUid() << std::endl;
  }
  std::cout << temp << std::endl;

  // only for debugging, so there is no try and catch
  http.parseHeader(temp);
  std::cout << "[DEBUG] parse successfully" << std::endl;
  //

  size_t end_of_header = temp.find("\r\n\r\n") + 4;
  try {
    http.updateBody(&message[end_of_header], recv_bytes - end_of_header);
  } catch (...) {
    throw ErrorException("update failed");
  }
}

void readMulti(int read_fd, std::string &body, int content_length) {
  int total_bytes = body.size();
  while (1) {
    body.resize(total_bytes + RECV_LENGTH);
    ssize_t recv_bytes = recv(read_fd, &body[total_bytes], RECV_LENGTH, 0);
    if (recv_bytes == -1) {
      throw ErrorException("recv error");
    }
    total_bytes += recv_bytes;
    body.resize(total_bytes);
    std::cout << "[DEBUG] read size " << total_bytes << std::endl;
    if (recv_bytes == 0) {
      if (errno == EINTR) {
        throw ErrorException("client exit");
      }
    }
    if ((content_length == 0 && recv_bytes < RECV_LENGTH) ||
        (content_length > 0 && total_bytes >= content_length) ||
        (recv_bytes == 0)) {
      // may need to throw an exception not finish reading
      break;
    }
  }
}

// void send_multi(int send_fd, std::string &body) {
//   size_t total_bytes = 0;
//   while (1) {
//     size_t send_size = (total_bytes + SEND_LENGTH > body.size())
//                            ? body.size() - total_bytes
//                            : SEND_LENGTH;
//     send(send_fd, &body[total_bytes], send_size, 0);
//     total_bytes += send_size;
//     if (total_bytes >= body.size()) {
//       break;
//     }
//   }
//   std::cout << "[INFO] successfully send " << total_bytes << std::endl;
// }

// void exchangeData(int client_fd, int destination_fd) {
//   std::string temp;
//   readMulti(client_fd, temp, 0);
//   std::cout << "[INFO] client " << client_fd << "sent " << temp.size()
//             << std::endl;
//   if (temp.size() == 0) {
//     throw ErrorException("read nothing");
//   }
//   int status = send(destination_fd, &temp[0], temp.size(), 0);
//   if (status == -1) {
//     throw ErrorException("send return -1");
//   }
//   std::cout << "[INFO] proxy sent " << temp.size() << " to " <<
//   destination_fd
//             << std::endl;
// }

int exchange_data(int client_fd, int destination_fd) {
  std::string temp;
  temp.resize(8192);
  ssize_t recv_bytes;
  recv_bytes = recv(client_fd, &temp[0], 8192, 0);
  temp.resize(recv_bytes);
  std::cout << "[INFO] client " << client_fd << "sent " << temp.size()
            << std::endl;
  if (temp.size() == 0) {
    return -1;
  }
  send(destination_fd, &temp.data()[0], temp.size(), 0);
  std::cout << "[INFO] proxy sent " << temp.size() << " to " << destination_fd
            << std::endl;
  return 0;
}

void handler(int client_fd, Cache *cache) {
  Request request;
  Response response;
  std::string request_header = "";
  std::string response_header = "";
  int status = 0;

  try {
    std::lock_guard<std::mutex> lck(mtx);
    request.setUid(uid);
    ++uid;
  } catch (...) {
    std::cout << "[DEBUG] LOCK ERROR" << std::endl;
    return;
  }

  try {
    readHeader(client_fd, request);
  } catch (ErrorException &e) {
    std::cout << e.what() << std::endl;
    return;
  }

  request.reconstructHeader(request_header); // no exception

  // only for dubugging
  std::cout << "[DEBUG] parsed header " << request_header << std::endl;
  //

  response.setUid(request.getUid());

  SocketInfo server_socket_info;
  std::string hostname = request.getHost();
  server_socket_info.hostname = hostname.c_str();
  std::string port = request.getPort();
  server_socket_info.port = port.c_str();

  std::cout << "[INFO] CONNECTING SERVER" << std::endl;
  try {
    server_socket_info.clientSetup();
  } catch (ErrorException &e) {
    std::cout << e.what() << std::endl;
    //    close(client_fd);
    return;
  }

  std::cout << "[DEBUG] setup successfully" << std::endl;
  try {
    server_socket_info.connectSocket();
  } catch (ErrorException &e) {
    std::cout << e.what() << std::endl;
    //    close(client_fd);
    return;
  }
  std::cout << "[DEBUG] connect successfully" << std::endl;

  if (request.getMethod() == "GET") {
    std::cout << "[INFO] GET" << std::endl;
    // send_multi(server_socket_info.socket_fd, header);
    std::string log_message = "";
    bool result_cache = cache->validate(request, response, log_message);
    std::cout << "[LOG] request " << log_message << std::endl;
    if (result_cache == true) { // response to client directly
      // only for dubbging, no try and catch
      std::cout << "[DEBUG] body received successfully" << std::endl;
      response.reconstructHeader(response_header); // no exception
      std::cout << "[DEBUG] reconstruct header " << response_header
                << std::endl;

      // send_multi(client_fd, header);
      status = send(client_fd, &response_header[0], response_header.size(), 0);
      if (status == -1) {
        std::cout << "[ERROR] send to client failed" << std::endl;
        return;
      }
      std::cout << "[DEBUG] send header successfully" << std::endl;
      //      send_multi(client_fd, response.get_body());

      std::string body = response.getBody();
      status = send(client_fd, &body[0], body.size(), 0);
      if (status == -1) {
        std::cout << "[ERROR] send to client failed" << std::endl;
        return;
      }
      std::cout << "[DEBUG] send body successfully" << std::endl;
    } else { // send to server
      send(server_socket_info.socket_fd, &request_header[0],
           request_header.size(), 0);
      std::cout << "[DEBUG] send to server successfully" << std::endl;
      try {
        readHeader(server_socket_info.socket_fd, response);
      } catch (ErrorException &e) {
        std::cout << "[ERROR] invalid resposne" << std::endl;
        std::cout << e.what() << std::endl;
        return;
      }

      // only for dubugging
      std::string key = "Content-Length";
      std::cout << "[DEBUG] Content-Lenght " << response.getValue(key)
                << std::endl;

      if (response.getBody().size() != 0) { // check other readMulti !!!!!!!!!
        try {
          readMulti(server_socket_info.socket_fd, response.getBody(),
                    atoi(response.getValue(key).c_str()));

        } catch (ErrorException &e) {
          std::cout << "[ERROR] reading response body failed" << std::endl;
          std::cout << e.what() << std::endl;
          return;
        }
      }
      std::cout << "[DEBUG] body received successfully" << std::endl;
      cache->update(request, response,
                    log_message); // only for dubbging, no try and catch
      std::cout << "[LOG] response " << log_message << std::endl;
      response.reconstructHeader(response_header); // no exception
      std::cout << "[DEBUG] reconstruct header " << response_header
                << std::endl;

      // send_multi(client_fd, header);
      status = send(client_fd, &response_header[0], response_header.size(), 0);
      if (status == -1) {
        std::cout << "[ERROR] send to client failed" << std::endl;
        return;
      }
      std::cout << "[DEBUG] send header successfully" << std::endl;
      //      send_multi(client_fd, response.get_body());

      std::string body = response.getBody();
      status = send(client_fd, &body[0], body.size(), 0);
      if (status == -1) {
        std::cout << "[ERROR] send to client failed" << std::endl;
        return;
      }
      std::cout << "[DEBUG] send body successfully" << std::endl;
    }
  } // if method == GET
  else if (request.getMethod() == "POST") {
    std::cout << "[INFO] POST" << std::endl;

    // only for debugging
    std::string key = "Content-Length";
    std::cout << "[DEBUG] Request Content-Lenght " << request.getValue(key)
              << std::endl;

    if (atoi(request.getValue(key).c_str()) > (int)request.getBody().size()) {
      try {
        readMulti(client_fd, request.getBody(),
                  atoi(request.getValue(key).c_str()));
      } catch (ErrorException &e) {
        std::cout << "[ERROR] reading request body failed" << std::endl;
        std::cout << e.what() << std::endl;
        return;
      }
    }

    std::cout << "[DEBUG] send to server successfully" << std::endl;
    //      send_multi(server_socket_info.socket_fd, header);
    status = send(server_socket_info.socket_fd, &request_header[0],
                  request_header.size(), 0);
    if (status == -1) {
      std::cout << "[ERROR] send to server failed" << std::endl;
      return;
    }

    // send_multi(server_socket_info.socket_fd, request.get_body());

    std::string body = request.getBody();
    status = send(server_socket_info.socket_fd, &body[0], body.size(), 0);
    if (status == -1) {
      std::cout << "[ERROR] send to server failed" << std::endl;
      return;
    }

    std::cout << "[DEBUG] waiting for response" << std::endl;
    try {
      readHeader(server_socket_info.socket_fd, response);
    } catch (ErrorException &e) {
      std::cout << "[ERROR] recv response header failed" << std::endl;
      std::cout << e.what() << std::endl;
      return;
    }

    std::cout << "[DEBUG] Response Content-Lenght " << response.getValue(key)
              << std::endl;

    if (response.getBody().size() != 0) {
      try {
        readMulti(server_socket_info.socket_fd, response.getBody(),
                  atoi(response.getValue(key).c_str()));
      } catch (ErrorException &e) {
        std::cout << "[ERROR] reading response body failed" << std::endl;
        std::cout << e.what() << std::endl;
        return;
      }
    }

    std::cout << "[DEBUG] body received successfully" << std::endl;

    std::string response_header = "";
    response.reconstructHeader(response_header);
    std::cout << "[DEBUG] reconstruct header " << response_header << std::endl;
    // send_multi(client_fd, header);

    status = send(client_fd, &response_header[0], response_header.size(), 0);
    if (status == -1) {
      std::cout << "[ERROR] send to client failed" << std::endl;
      return;
    }
    std::cout << "[DEBUG] send header successfully" << std::endl;
    //      send_multi(client_fd, response.get_body());
    body = response.getBody();
    status = send(client_fd, &body[0], body.size(), 0);
    if (status == -1) {
      std::cout << "[ERROR] send to client failed" << std::endl;
      return;
    }

    std::cout << "[DEBUG] send body successfully" << std::endl;
  } else if (request.getMethod() == "CONNECT") {
    std::string message = "HTTP/1.1 200 OK\r\n\r\n";
    send(client_fd, message.c_str(), message.size(), 0);
    std::cout << "[INFO] CONNECT RESPONSE TO CLIENT" << std::endl;
    while (1) {
      std::cout << "[INFO] CONNECT" << std::endl;
      fd_set sockset;
      FD_ZERO(&sockset);
      int maxfd = client_fd > server_socket_info.socket_fd
                      ? client_fd
                      : server_socket_info.socket_fd;
      FD_SET(client_fd, &sockset);
      FD_SET(server_socket_info.socket_fd, &sockset);
      struct timeval time;
      time.tv_sec = 0;
      time.tv_usec = 10000000;

      int ret = select(maxfd + 1, &sockset, nullptr, nullptr, &time);

      if (ret == -1) {
        std::cout << "select error" << std::endl;
        break;
      }
      if (ret == 0) {
        break;
      }
      if (FD_ISSET(client_fd, &sockset)) {
        std::cout << "[DEBUG] client to server" << std::endl;
        if (exchange_data(client_fd, server_socket_info.socket_fd) == -1) {
          std::cout << "[DEBUG] transfer failure" << std::endl;
          break;
        }
      } else {
        std::cout << "[DEBUG] server to client" << std::endl;
        if (exchange_data(server_socket_info.socket_fd, client_fd) == -1) {
          std::cout << "[DEBUG] transfer failure" << std::endl;
          break;
        }
      }

      // if (FD_ISSET(client_fd, &sockset)) {
      //   std::cout << "[DEBUG] client to server" << std::endl;

      //   try {
      //     exchangeData(client_fd, server_socket_info.socket_fd);
      //   } catch (ErrorException &e) {
      //     std::cout << "[ERROR] exchange data failed" << std::endl;
      //     std::cout << e.what() << std::endl;
      //     break;
      //   }
      // } else {
      //   std::cout << "[DEBUG] server to client" << std::endl;

      //   try {
      //     exchangeData(server_socket_info.socket_fd, client_fd);
      //   } catch (ErrorException &e) {
      //     std::cout << "[ERROR] exchange data failed" << std::endl;
      //     std::cout << e.what() << std::endl;
      //     break;
      //   }
    }
  } // while
  close(client_fd);
  close(server_socket_info.socket_fd);
} // if CONNECT
}

int main(int argc, char **argv) {
  try {
    log.open("/var/log/erss/proxy.log", std::ifstream::in);
  } catch (std::exception &e) {
    std::cout << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  Cache cache;
  SocketInfo socket_info;
  socket_info.hostname = nullptr;
  socket_info.port = "12345";

  try {
    socket_info.setup(); // Create Socket
  } catch (ErrorException &e) {
    std::cout << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  try {
    socket_info.wait();
  } // Bind & Listen
  catch (ErrorException &e) {
    std::cout << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  while (1) {
    int client_fd;

    try {
      socket_info.acc(&client_fd);
    } // Bind & Listen
    catch (ErrorException &e) {
      std::cout << "exit main" << std::endl;
      std::cout << e.what() << std::endl;
      return EXIT_FAILURE;
    }

    try { // delete after finish
      std::thread th(handler, client_fd, &cache);
      th.detach();
    } catch (...) {
      std::cout << "[NOOOOOOOOOO]" << std::endl;
    }
  }

  return EXIT_SUCCESS;
}
