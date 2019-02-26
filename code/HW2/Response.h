

#ifndef PROXY_RESPONSE_H
#define PROXY_RESPONSE_H

#include "Http.h"

class Response : public Http {
private:
  struct First_line_msg {
    std::string status_num;
    std::string status_char;
    std::string protocol;
  };
  First_line_msg first_line_msg;

public:
  Response() {
    first_line_msg.protocol = "";
    first_line_msg.status_num = "";
    first_line_msg.status_char = "";
  }
  virtual ~Response() {}
  Response(const Response &rhs) {
    first_line_msg.status_num = rhs.first_line_msg.status_num;
    first_line_msg.status_char = rhs.first_line_msg.status_char;
    first_line_msg.protocol = rhs.first_line_msg.protocol;
  }
  Response &operator=(const Response &rhs) {
    if (this == &rhs) {
      return *this;
    }
    first_line_msg.status_num = rhs.first_line_msg.status_num;
    first_line_msg.status_char = rhs.first_line_msg.status_char;
    first_line_msg.protocol = rhs.first_line_msg.protocol;
    return *this;
  }
  void setProtocol(std::string value) { first_line_msg.protocol = value; }
  virtual void parseFirstLine() { // HTTP/1.1 200 OK
    std::string first_line = getFirstLine();
    size_t space0 = first_line.find(' ');
    if (space0 == first_line.npos) {
      throw ErrorException("invalid response first line");
    }
    std::string value = first_line.substr(0, space0);
    first_line_msg.protocol = value;
    size_t space1 = first_line.find(' ', space0 + 1);
    if (space1 == first_line.npos) {
      throw ErrorException("invalid response first line");
    }
    value = first_line.substr(space0 + 1, space1 - space0 - 1);
    first_line_msg.status_num = value;
    first_line_msg.status_char = first_line.substr(space1 + 1);
  }
  std::string getStatusNum() { return first_line_msg.status_num; }
};

#endif // PROXY_RESPONSE_H
