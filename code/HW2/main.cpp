#include "Mysocket.h"
#include "Request.h"
#include "Response.h"
#include <mutex>
#define RECV_LENGTH 2048
#define SEND_LENGTH 2048
#define HEADER_LENGTH 8192
std::mutex mtx;
int uid = 0;

void read_multi(int read_fd, std::string &body, int content_length) {
  int total_bytes = body.size();
  if (content_length > 0) {
    while (1) {
      body.resize(total_bytes + RECV_LENGTH);
      ssize_t recv_bytes = recv(read_fd, &body[total_bytes], RECV_LENGTH, 0);
      total_bytes += recv_bytes;
      if (recv_bytes == 0) {
        throw ErrorException("client exit in read_multi");
      } else if (recv_bytes < 1024) {
        // didn't get enough data
        body.resize(total_bytes);
      }
      if (total_bytes >= content_length) {
        // finished receiving
        break;
      }
    }
  } else {
    // no specific content_length in the response header
    // read until no data in the buffer (NOT SURE)
    while (1) {
      body.resize(total_bytes + RECV_LENGTH);
      ssize_t recv_bytes = recv(read_fd, &body[total_bytes], RECV_LENGTH, 0);
      total_bytes += recv_bytes;
      if (recv_bytes == 0) {
        // finished reading? (maybe the socket was closed...)
        break;
      } else if (recv_bytes < 1024) {
        // didn't get enough data
        body.resize(total_bytes);
      }
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
    char message[HEADER_LENGTH];
    memset(message, 0, sizeof(message));
    ssize_t cap = recv(client_fd, &message, sizeof(message), 0);
    std::cout << "[DEBUG] request received length " << cap << std::endl;
    if (cap == 0) {
      std::cout << "client close socket\r\n";
      std::string temp("recive header failed for client close socket\r\n");
      throw ErrorException(temp);
    }
    std::string temp(message);
    std::cout << "[DEBUG] request of REQ " << request.get_uid() << std::endl;
    std::cout << temp << std::endl;
    request.parse_head(temp);
    std::cout << "[DEBUG] parse request header successfully" << std::endl;
  } catch (ErrorException &e) {
    std::cout << e.what() << std::endl;
  }
  try {
    if (request.get_method() == "GET") {
      std::cout << "-------------GET--------------"
                << "\r\n";

      socket_info_t server_socket_info;
      std::string hostname = request.get_Host();
      server_socket_info.hostname = hostname.c_str();
      std::string port = request.get_port();
      server_socket_info.port = port.c_str();
      int status = client_setup(&server_socket_info);
      if (status == -1)
        throw ErrorException("setup failure");
      status = connect_socket(&server_socket_info);
      if (status == -1) {
        freeaddrinfo(server_socket_info.host_info_list);
        close(server_socket_info.socket_fd);
        throw ErrorException("connect failure");
      }
      std::cout << "[DEBUG] connect successfully" << std::endl;
      if (status == -1) {
        std::cout << "connect fail" << std::endl;
        close(server_socket_info.socket_fd);
        close(client_fd);
        return;
      }

      std::string header = "";
      request.reconstruct_header(header);

      std::cout << "[DEBUG] reconstruct header " << header << std::endl;
      send_multi(server_socket_info.socket_fd, header);
      std::cout << "[DEBUG] send to server successfully" << std::endl;
      Response response;
      response.set_uid(request.get_uid());
      try {
        char message[HEADER_LENGTH];
        memset(message, 0, sizeof(message));
        ssize_t recv_bytes =
            recv(server_socket_info.socket_fd, &message, sizeof(message), 0);
        std::string temp(message);
        std::cout << "[DEBUG] received response header successfully"
                  << std::endl;
        std::cout << "[DEBUG] RESPONSE from RES " << response.get_uid() << temp
                  << std::endl;
        response.parse_head(temp);
        if (response.first_line.find("301 Moved Permanently") !=
            response.first_line.npos) {
        }
        size_t end_of_header = temp.find("\r\n\r\n") + 4;
        response.update_body(&message[end_of_header],
                             recv_bytes - end_of_header);
      } catch (...) {
      }
      std::string key = "Content-Length";
      cout << "[DEBUG] Content-Lenght " << response.get_value(key) << endl;
      read_multi(server_socket_info.socket_fd, response.get_body(),
                 atoi(response.get_value(key).c_str()));
      std::cout << "[DEBUG] body received successfully" << std::endl;
      std::string response_header = "";
      response.reconstruct_header(header);
      std::cout << "[DEBUG] reconstruct header " << response_header
                << std::endl;
      send_multi(client_fd, header);
      std::cout << "[DEBUG] send header successfully" << std::endl;
      send_multi(client_fd, response.get_body());
      std::cout << "[DEBUG] send body successfully" << std::endl;
    }

    close(client_fd);
  } catch (...) {
    close(client_fd);
    return;
  }
}

int main(int argc, char **argv) {
  socket_info_t socket_info;
  socket_info.hostname = nullptr;
  socket_info.port = "12345";
  int status = setup(&socket_info);
  if (status == -1)
    return EXIT_FAILURE;
  status = wait(&socket_info);
  if (status == -1)
    return EXIT_FAILURE;
  while (1) {
    int client_fd;
    status = acc(&socket_info, &client_fd);

    if (status == EXIT_FAILURE) {
      std::cout << "[ERROR] accept, exit" << endl;
      break;
    }
    std::thread th(handler, client_fd);
    th.detach();
  }

  return EXIT_SUCCESS;
}
