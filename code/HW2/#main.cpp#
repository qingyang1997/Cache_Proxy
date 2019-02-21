#include "Cache.h"
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

void read_header(int read_fd, Http &http) {
  char message[HEADER_LENGTH];
  memset(message, 0, sizeof(message));
  ssize_t recv_bytes = recv(read_fd, &message, sizeof(message), 0);
  if (recv_bytes == 0) {
    std::string temp("client close socket");
    throw ErrorException(temp);
  }
  std::string temp(message);
  const std::type_info &type_info = typeid(http);
  if (type_info == typeid(Request)) {
    std::cout << "[DEBUG] REQUEST " << http.get_uid() << std::endl;
  } else {
    std::cout << "[DEBUG] RESPONSE " << http.get_uid() << std::endl;
  }
  std::cout << temp << std::endl;
  http.parse_head(temp);
  std::cout << "[DEBUG] parse successfully" << std::endl;
  size_t end_of_header = temp.find("\r\n\r\n") + 4;
  http.update_body(&message[end_of_header], recv_bytes - end_of_header);
}

void read_multi(int read_fd, std::string &body, int content_length) {
  int total_bytes = body.size();
  while (1) {
    body.resize(total_bytes + RECV_LENGTH);
    ssize_t recv_bytes = recv(read_fd, &body[total_bytes], RECV_LENGTH, 0);
    total_bytes += recv_bytes;
    if (recv_bytes == 0) {
      std::cout << "0000000000000" << std::endl;
      perror("read_multi");
      if (errno == EINTR) {
        // client exits
        throw ErrorException("client exit");
      } else if (content_length > 0) {
        throw ErrorException("not finish reading");
      } else {
        break;
      }

    } else if (recv_bytes < RECV_LENGTH) {
      // didn't get enough data
      body.resize(total_bytes);
    }
    if (content_length > 0 && total_bytes >= content_length) {
      // finished receiving
      break;
    }
  }
}

void send_multi(int send_fd, std::string &body) {
  size_t total_bytes = 0;
  while (1) {
    size_t send_size = (total_bytes + SEND_LENGTH > body.size())
                           ? body.size() - total_bytes
                           : SEND_LENGTH;
    send(send_fd, &body[total_bytes], send_size, 0);
    total_bytes += send_size;
    if (total_bytes >= body.size()) {
      break;
    }
  }
  std::cout << "[INFO] successfully send " << total_bytes << std::endl;
}

// void exchange_data(int source_fd, int destination_fd) {
//   // read
//   std::string data = "";
//   read_multi(source_fd, data, 0);
//   std::cout << "[DEBUG] successfully received " << data.size() << std::endl;
//   // send
//   send_multi(destination_fd, data);
//   std::cout << "[DEBUG] successfully sent " << data.size() << std::endl;
// }

void exchange_data(int source_fd, int destination_fd) {
  // read
  std::string data = "";
  data.resize(8192);
  size_t cap = recv(source_fd, &data[0], 8192, 0);
  // int ret = getpeername(client_fd, )
  std::cout << errno << std::endl;
  std::cout << "0000000000000" << std::endl;
  perror("read_multi");

  data.resize(cap);
  //  std::cout << "[DEBUG] successfully received " << data.size() << std::endl;
  // send
  send(destination_fd, &data[0], cap, 0);
  // std::cout << "[DEBUG] successfully sent " << data.size() << std::endl;
}
void handler(int client_fd) {
  Request request;
  try {
    std::lock_guard<std::mutex> lck(mtx);
    request.set_uid(uid);
    ++uid;
  } catch (...) {
    cout << "[DEBUG] LOCK ERROR" << endl;
    return;
  }

  try {
    read_header(client_fd, request);
  } catch (ErrorException &e) {

    std::cout << e.what() << std::endl;
  }
  std::string header = "";
  request.reconstruct_header(header);
  std::cout << "[DEBUG] parsed header " << header << std::endl;
  Response response;
  response.set_uid(request.get_uid());

  socket_info_t server_socket_info;
  std::string hostname = request.get_host();
  std::cout << "[DEBUG] HOST " << hostname << std::endl;
  server_socket_info.hostname = hostname.c_str();
  std::string port = request.get_port();
  std::cout << "[DEBUG] PORT " << port << std::endl;
  server_socket_info.port = port.c_str();

  // socket
  std::cout << "[INFO] CONNECTING SERVER" << std::endl;
  try {
    client_setup(&server_socket_info);
  } catch (ErrorException &e) {
    std::cout << e.what() << std::endl;
    close(client_fd);
    return;
  }

  std::cout << "[DEBUG] setup successfully" << std::endl;
  try {
    connect_socket(&server_socket_info);
    std::cout << "[DEBUG] connect successfully" << std::endl;
  } catch (ErrorException &e) {
    std::cout << e.what() << std::endl;
    close(client_fd);
    return;
  }

  try {
    // std::string header = "";
    // request.reconstruct_header(header);
    // std::cout << "[DEBUG] reconstruct header " << header << std::endl;

    if (request.get_method() == "GET") {
      // send request
      std::cout << "[INFO] GET" << std::endl;
      send_multi(server_socket_info.socket_fd, header);
      std::cout << "[DEBUG] send to server successfully" << std::endl;

      // receive response
      read_header(server_socket_info.socket_fd, response);

      std::string key = "Content-Length";
      cout << "[DEBUG] Content-Lenght " << response.get_value(key) << endl;
      if (response.get_body().size() != 0) {
        read_multi(server_socket_info.socket_fd, response.get_body(),
                   atoi(response.get_value(key).c_str()));
      }

      std::cout << "[DEBUG] body received successfully" << std::endl;
      std::string response_header = "";
      response.reconstruct_header(header);
      std::cout << "[DEBUG] reconstruct header " << response_header
                << std::endl;
      send_multi(client_fd, header);
      std::cout << "[DEBUG] send header successfully" << std::endl;
      send_multi(client_fd, response.get_body());
      std::cout << "[DEBUG] send body successfully" << std::endl;
    } // if method == GET
    else if (request.get_method() == "POST") {
      // finish reading request
      std::cout << "[INFO] POST" << std::endl;
      std::string key = "Content-Length";
      cout << "[DEBUG] Request Content-Lenght " << request.get_value(key)
           << endl;
      read_multi(client_fd, request.get_body(),
                 atoi(request.get_value(key).c_str()));

      // send to server
      std::cout << "[DEBUG] send to server successfully" << std::endl;
      send_multi(client_fd, header);
      // receive response
      read_header(server_socket_info.socket_fd, response);

      cout << "[DEBUG] Response Content-Lenght " << response.get_value(key)
           << endl;
      if (response.get_body().size() != 0) {
        read_multi(server_socket_info.socket_fd, response.get_body(),
                   atoi(response.get_value(key).c_str()));
      }

      std::cout << "[DEBUG] body received successfully" << std::endl;

      std::string response_header = "";
      response.reconstruct_header(header);
      std::cout << "[DEBUG] reconstruct header " << response_header
                << std::endl;
      send_multi(client_fd, header);
      std::cout << "[DEBUG] send header successfully" << std::endl;
      send_multi(client_fd, response.get_body());
      std::cout << "[DEBUG] send body successfully" << std::endl;
    } else if (request.get_method() == "CONNECT") {
      // select
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

      // answer the client
      std::string message = "200 OK\r\n\r\n";
      send(client_fd, message.c_str(), message.size(), 0);
      std::cout << "[INFO] CONNECT RESPONSE TO CLIENT" << std::endl;
      while (1) {
        int ret = select(maxfd + 1, &sockset, nullptr, nullptr, &time);

        if (ret == -1) {
          std::cout << "select error" << std::endl;
          break;
        }
        if (ret == 0) {
          break;
        }
        try {
          if (FD_ISSET(client_fd, &sockset)) {
            std::cout << "[DEBUG] client to server" << std::endl;
            exchange_data(client_fd, server_socket_info.socket_fd);
          } else {
            std::cout << "[DEBUG] server to client" << std::endl;
            exchange_data(server_socket_info.socket_fd, client_fd);
          }
        } catch (ErrorException &e) {
          std::cout << e.what() << std::endl;
          break;
        }
      }
      close(client_fd);
      close(server_socket_info.socket_fd);
      // exit
    }
    close(client_fd);
  } catch (ErrorException &e) {
    std::cout << e.what() << std::endl;
    close(client_fd);
    return;
  }
}

int main(int argc, char **argv) {
  socket_info_t socket_info;
  socket_info.hostname = nullptr;
  socket_info.port = "12345";

  try {
    setup(&socket_info); // Create Socket
  } catch (ErrorException &e) {
    std::cout << e.what() << std::endl;

    return EXIT_FAILURE;
  }
  try {
    wait(&socket_info);
  } // Bind & Listen
  catch (ErrorException &e) {
    std::cout << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  while (1) {
    int client_fd;

    try {
      acc(&socket_info, &client_fd);
    } // Bind & Listen
    catch (ErrorException &e) {
      std::cout << e.what() << std::endl;
      return EXIT_FAILURE;
    }
    std::thread th(handler, client_fd);
    th.detach();
  }

  return EXIT_SUCCESS;
}
