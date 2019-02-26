#include "Cache.h"
#include "Mysocket.h"
#include "Request.h"
#include "Response.h"
#include <mutex>
#include <typeinfo>
#define RECV_LENGTH 2048
#define SEND_LENGTH 2048
#define HEADER_LENGTH 8192
#define DEBUG 1

std::ofstream log;
std::mutex mtx;
int uid = 0;

void readHeader(int read_fd, Http &http) { // strong guarantee
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
  if (DEBUG == 1) {
    if (type_info == typeid(Request)) {
      std::cout << "[DEBUG] REQUEST " << http.getUid() << std::endl;
    } else {
      std::cout << "[DEBUG] RESPONSE " << http.getUid() << std::endl;
    }
  }
  std::cout << temp << std::endl;

  http.parseHeader(temp);
  size_t end_of_header = temp.find("\r\n\r\n");
  if (end_of_header == temp.npos) {
    throw ErrorException("invalid header");
  } else {
    end_of_header += 4;
  }
  http.updateBody(&message[end_of_header], recv_bytes - end_of_header);
}
std::string getCurrentTime() { // strong guarantee
  time_t current_time = time(0);
  tm *tm = localtime(&current_time);
  char *dt = asctime(tm);
  return std::string(dt);
}
void readMulti(int read_fd, std::string &body,
               int content_length) { // strong guarantee
  int total_bytes = body.size();
  std::string temp = body;
  while (1) {
    temp.resize(total_bytes + RECV_LENGTH);
    ssize_t recv_bytes = recv(read_fd, &temp[total_bytes], RECV_LENGTH, 0);
    total_bytes += recv_bytes;
    temp.resize(total_bytes);
    if (recv_bytes == 0) {
      if (errno == EINTR) {
        // client exits
        throw ErrorException("client exit");
      }
      // if (content_length > 0 && total_bytes < content_length) {
      //   throw ErrorException("not finish");
      // }
    }
    if ((content_length > 0 && total_bytes >= content_length) ||
        (recv_bytes == 0)) {
      // finished receiving
      break;
    }
  }
  std::swap(body, temp);
}

void exchangeData(int client_fd, int destination_fd) {
  std::string temp;
  temp.resize(8192);
  ssize_t recv_bytes;
  recv_bytes = recv(client_fd, &temp[0], 8192, 0);
  temp.resize(recv_bytes);
  if (DEBUG == 1) {
    std::cout << "[INFO] client " << client_fd << " sent " << temp.size()
              << std::endl;
  }
  if (temp.size() == 0) {
    throw ErrorException("read nothing");
  }
  send(destination_fd, &temp.data()[0], temp.size(), 0);
  if (DEBUG == 1) {
    std::cout << "[INFO] proxy sent " << temp.size() << " to " << destination_fd
              << std::endl;
  }
}

void logMsg(std::string &msg) {
  std::lock_guard<std::mutex> lck(mtx);
  log << msg;
}

void handler(int client_fd, Cache *cache) {
  Request request;
  Response response;
  std::string request_header = "";
  std::string response_header = "";
  std::stringstream log_msg("");
  std::string bad_request = "HTTP/1.1 400 Bad Request\r\n\r\n";
  std::string bad_gateway = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
  int status = 0;

  struct sockaddr_in sa;
  socklen_t len = sizeof(sa);
  getpeername(client_fd, (struct sockaddr *)&sa, &len);
  // CCLog("[SEND]on sending data %d\n", ntohs(sa.sin_port));
  std::string peername(inet_ntoa(*(struct in_addr *)&sa.sin_addr.s_addr));
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
    send(client_fd, &bad_request[0], bad_request.size(), 0);
    log_msg << request.getUid() << ": Responding "
            << "HTTP/1.1 400 Bad Request" << std::endl;
    std::string logmsg = log_msg.str();
    logMsg(logmsg);
    log_msg.str("");
    return;
  }
  try {
    request.reconstructHeader(request_header);
  } catch (ErrorException &e) {
    std::cout << e.what() << std::endl;
    return;
  }

  log_msg << request.getUid() << ": " << request.getFirstLine() << " from "
          << peername << " @ " << getCurrentTime();
  std::string logmsg = log_msg.str();
  logMsg(logmsg);
  log_msg.str("");
  // only for dubugging
  if (DEBUG == 1) {
    std::cout << "[DEBUG] parsed header " << request_header << std::endl;
  }
  //
  response.setUid(request.getUid());

  SocketInfo server_socket_info;
  std::string hostname = request.getHost();
  server_socket_info.hostname = hostname.c_str();
  std::string port = request.getPort();
  server_socket_info.port = port.c_str();

  // std::cout << "[INFO] CONNECTING SERVER" << std::endl;
  try {
    server_socket_info.clientSetup();
  } catch (ErrorException &e) {
    std::cout << e.what() << std::endl;
    //    close(client_fd);
    return;
  }

  // std::cout << "[DEBUG] setup successfully" << std::endl;
  try {
    server_socket_info.connectSocket();
  } catch (ErrorException &e) {
    std::cout << e.what() << std::endl;
    //    close(client_fd);
    return;
  }
  //  std::cout << "[DEBUG] connect successfully" << std::endl;

  if (request.getMethod() == "GET") {
    std::cout << "[INFO] GET" << std::endl;
    std::string log_message = "";
    bool result_cache = cache->validate(request, response, log_message);
    std::cout << "[LOG] request " << log_message
              << " And the UID is: " << request.getUid() << std::endl;
    if (result_cache == true) { // response to client directly
                                //    only for dubbging, no tryand catch
      response.reconstructHeader(response_header);
      if (DEBUG == 1) {
        std::cout << "[DEBUG] body received successfully" << std::endl;
        std::cout << "[DEBUG] reconstruct header " << response_header
                  << std::endl;
      }

      log_msg << response.getUid() << " " << log_message << std::endl;
      logmsg = log_msg.str();
      logMsg(logmsg);
      log_msg.str("");

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
      log_msg << response.getUid() << ": Responding " << response.getFirstLine()
              << std::endl;
      logmsg = log_msg.str();
      logMsg(logmsg);
      log_msg.str("");

      if (response.getStatusNum() == "200") {
        log_msg << response.getUid() << ": Tunnel closed" << std::endl;
        logmsg = log_msg.str();
        logMsg(logmsg);
        log_msg.str("");
      }
    } else { // send to server

      request.reconstructHeader(request_header);

      std::cout << "[DASHABI] Uid " << request.getUid()
                << " request before send " << request_header << std::endl;

      log_msg << request.getUid() << ": Requesting " << request.getFirstLine()
              << " from " << request.getHost() << std::endl;
      logmsg = log_msg.str();
      logMsg(logmsg);
      log_msg.str("");

      send(server_socket_info.socket_fd, &request_header[0],
           request_header.size(), 0);
      std::cout << "[DEBUG] send to server successfully" << std::endl;
      try {
        readHeader(server_socket_info.socket_fd, response);
      } catch (ErrorException &e) {
        std::cout << e.what() << std::endl;
        send(client_fd, &bad_gateway[0], bad_gateway.size(), 0);
        log_msg << request.getUid() << ": Responding "
                << "HTTP/1.1 502 Bad Gateway" << std::endl;
        std::string logmsg = log_msg.str();
        logMsg(logmsg);
        log_msg.str("");
        return;
      }

      // // only for dubugging
      std::string key = "Content-Length";
      // std::cout << "[DEBUG] Content-Lenght " << response.getValue(key)
      //           << std::endl;

      if (response.getBody().size() != 0) { // check other readMulti
        try {
          readMulti(server_socket_info.socket_fd, response.getBody(),
                    atoi(response.getValue(key).c_str()));

        } catch (ErrorException &e) {
          std::cout << "[ERROR] reading response body failed" << std::endl;
          std::cout << e.what() << std::endl;
          send(client_fd, &bad_gateway[0], bad_gateway.size(), 0);
          log_msg << request.getUid() << ": Responding "
                  << "HTTP/1.1 502 Bad Gateway" << std::endl;
          std::string logmsg = log_msg.str();
          logMsg(logmsg);
          log_msg.str("");
          return;
        }
      }

      log_msg << request.getUid() << ": Received " << response.getFirstLine()
              << " from " << request.getHost() << std::endl;
      logmsg = log_msg.str();
      logMsg(logmsg);
      log_msg.str("");

      std::cout << "[DEBUG] body received successfully" << std::endl;
      log_message = "";
      cache->update(request, response,
                    log_message); // only for dubbging, no try and catch
      std::cout << "[LOG] response " << log_message << std::endl;
      response.reconstructHeader(response_header); // no exception
      std::cout << "[DEBUG] reconstruct header " << response_header
                << std::endl;
      log_msg << response.getUid() << ": " << log_message << std::endl;
      logmsg = log_msg.str();
      logMsg(logmsg);
      log_msg.str("");

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
      log_msg << response.getUid() << ": Responding " << response.getFirstLine()
              << std::endl;
      logmsg = log_msg.str();
      logMsg(logmsg);
      log_msg.str("");
      if (response.getStatusNum() == "200") {
        log_msg << response.getUid() << ": Tunnel closed" << std::endl;
        logmsg = log_msg.str();
        logMsg(logmsg);
        log_msg.str("");
      }
    } // if send to server

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
        std::cout << e.what() << std::endl;
        send(client_fd, &bad_request[0], bad_request.size(), 0);
        log_msg << request.getUid() << ": Responding "
                << "HTTP/1.1 400 Bad Request" << std::endl;
        std::string logmsg = log_msg.str();
        logMsg(logmsg);
        log_msg.str("");
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

    std::string body = request.getBody();
    status = send(server_socket_info.socket_fd, &body[0], body.size(), 0);
    if (status == -1) {
      std::cout << "[ERROR] send to server failed" << std::endl;
      return;
    }

    log_msg << request.getUid() << ": Requesting " << request.getFirstLine()
            << " from " << request.getFirstLine() << std::endl;
    logmsg = log_msg.str();
    logMsg(logmsg);
    log_msg.str("");

    std::cout << "[DEBUG] waiting for response" << std::endl;
    try {
      readHeader(server_socket_info.socket_fd, response);
    } catch (ErrorException &e) {
      std::cout << e.what() << std::endl;
      send(client_fd, &bad_gateway[0], bad_gateway.size(), 0);
      log_msg << request.getUid() << ": Responding "
              << "HTTP/1.1 502 Bad Gateway" << std::endl;
      std::string logmsg = log_msg.str();
      logMsg(logmsg);
      log_msg.str("");
      return;
    }

    std::cout << "[DEBUG] Response Content-Lenght " << response.getValue(key)
              << std::endl;

    if (response.getBody().size() != 0) {
      try {
        readMulti(server_socket_info.socket_fd, response.getBody(),
                  atoi(response.getValue(key).c_str()));
      } catch (ErrorException &e) {
        std::cout << e.what() << std::endl;
        send(client_fd, &bad_gateway[0], bad_gateway.size(), 0);
        log_msg << request.getUid() << ": Responding "
                << "HTTP/1.1 502 Bad Gateway" << std::endl;
        std::string logmsg = log_msg.str();
        logMsg(logmsg);
        log_msg.str("");
        return;
      }
    }
    log_msg << response.getUid() << ": Received " << response.getFirstLine()
            << " from " << request.getFirstLine() << std::endl;
    logmsg = log_msg.str();
    logMsg(logmsg);
    log_msg.str("");

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
    log_msg << request.getUid() << ": Responding " << response.getFirstLine()
            << std::endl;
    logmsg = log_msg.str();
    logMsg(logmsg);
    log_msg.str("");

    std::cout << "[DEBUG] send body successfully" << std::endl;
    if (response.getStatusNum() == "200") {
      log_msg << request.getUid() << ": Tunnel closed" << std::endl;
      logmsg = log_msg.str();
      logMsg(logmsg);
      log_msg.str("");
    }
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
      time.tv_usec = 1000000000;

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

        try {
          exchangeData(client_fd, server_socket_info.socket_fd);
        } catch (ErrorException &e) {
          std::cout << "[ERROR] exchange data failed" << std::endl;
          std::cout << e.what() << std::endl;
          break;
        }

      } else {
        std::cout << "[DEBUG] server to client" << std::endl;

        try {
          exchangeData(server_socket_info.socket_fd, client_fd);
        } catch (ErrorException &e) {
          std::cout << "[ERROR] exchange data failed" << std::endl;
          std::cout << e.what() << std::endl;
          break;
        }
      }

    } // while
    // close(client_fd);
    // close(server_socket_info.socket_fd);
  } // if CONNECT
}

int main(int argc, char **argv) {

  try {
    log.open("proxy.log", std::ostream::out);
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
