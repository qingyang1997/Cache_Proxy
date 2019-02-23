//
// Created by Duck2 on 2019-02-18.
//

#ifndef PROXY_REQUEST_H
#define PROXY_REQUEST_H

#include "Http.h"
#include "common.h"
#include "myexception.h"

class Request : public Http {
private:
  struct First_line_msg {
    std::string method;
    std::string host;
    std::string port;
    // string url;
    std::string protocol; // may not need this
  };
  First_line_msg first_line_msg;

public:
  Request() {}
  virtual ~Request(){};
  virtual void parseFirstLine() {
    size_t space = getFirstLine().find(' ');
    std::string value = getFirstLine().substr(0, space);
    if (value != "GET" && value != "POST" && value != "CONNECT") {
      throw ErrorException("invalid method");
    }

    first_line_msg.method = value;
    size_t slash = getFirstLine().find('/');
    size_t http = getFirstLine().find("http");
    value = "http";
    if (http != getFirstLine().npos && http + 4 < getFirstLine().size() &&
        getFirstLine()[http + 4] == 's') {
      value = "https";
    }
    first_line_msg.protocol = value;
    space = getFirstLine().find(' ', space + 1);
    value = getFirstLine().substr(slash + 2, space - slash - 3);
    size_t colon = value.find(':');
    if (http != getFirstLine().npos) {
      colon = value.find(':', colon);
    }
    std::string port = "";
    if (colon != value.npos) {
      port = value.substr(colon + 1);
    }
    if (port.empty()) {
      port = (first_line_msg.protocol == "http") ? "80" : "443";
    }
    if (first_line_msg.method == "CONNECT") {
      port = "443";
    }
    std::string key = "Host";
    value = getValue(key);
    colon = value.find(':');
    if (colon != value.npos) {
      value = value.substr(0, colon);
    }
    first_line_msg.host = value;
    first_line_msg.port = port;
  }
  std::string get_method() { return first_line_msg.method; }
  std::string get_protocol() { return first_line_msg.protocol; }
  std::string get_host() { return first_line_msg.host; }
  std::string get_port() { return first_line_msg.port; }

  // for testing
  // void display() {
  //   cout << "[DEBUG] in first_line_msg" << endl;
  //   cout << "[DEBUG] method " << first_line_msg.method << endl;
  //   cout << "[DEBUG] protocol " << first_line_msg.protocol << endl;
  //   cout << "[DEBUG] port " << first_line_msg.port << endl;
  //   cout << "[DEBUG] host " << first_line_msg.host << endl;
  //   cout << "------------------------" << endl;
  //   cout << "[DEBUG] first_line " << first_line << endl;
  //   unordered_map<string, string>::iterator it = header_pair.begin();
  //   while (it != header_pair.end()) {
  //     cout << "[DEBUG] " << it->first << " " << it->second << endl;
  //     ++it;
  //   }
  // }
  // bool isequal(Request &rhs) {
  //   if (uid == rhs.uid && first_line == rhs.first_line &&
  //       first_line_msg.protocol == rhs.first_line_msg.protocol &&
  //       first_line_msg.method == rhs.first_line_msg.method &&
  //       first_line_msg.port == rhs.first_line_msg.port &&
  //       first_line_msg.host == rhs.first_line_msg.host &&
  //       header_pair == rhs.header_pair) {
  //     return true;
  //   }
  //   return false;
  // }
};

#endif // PROXY_REQUEST_H
