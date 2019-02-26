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
  Response() {}
  virtual ~Response() {}
  Response(const Response &rhs) : Http(rhs) { // strong guarantee
    first_line_msg.status_num = rhs.first_line_msg.status_num;
    first_line_msg.status_char = rhs.first_line_msg.status_char;
    first_line_msg.protocol = rhs.first_line_msg.protocol;
  }
  // Response &operator=(const Response &rhs) { // strong guarantee
  //   if (this == &rhs) {
  //     return *this;
  //   }
  //   Response temp;
  //   try {
  //     temp.Http::operator=(rhs);
  //     temp.first_line_msg.status_num = rhs.first_line_msg.status_num;
  //     temp.first_line_msg.status_char = rhs.first_line_msg.status_char;
  //     temp.first_line_msg.protocol = rhs.first_line_msg.protocol;
  //   } catch (...) {
  //     throw ErrorException("Response = failed");
  //   }
  //   std::swap(temp, *this);
  //   return *this;
  // }
  Response &operator=(const Response &rhs) { // strong guarantee
    if (this == &rhs) {
      return *this;
    }
    try {
      this->Http::operator=(rhs);
      first_line_msg.status_num = rhs.first_line_msg.status_num;
      first_line_msg.status_char = rhs.first_line_msg.status_char;
      first_line_msg.protocol = rhs.first_line_msg.protocol;
    } catch (...) {
      throw ErrorException("Response = failed");
    }
    // std::swap(temp, *this);
    return *this;
  }
  void setProtocol(std::string value) {
    first_line_msg.protocol = value;
  }                               // strong guarantee
  virtual void parseFirstLine() { // strong guarantee
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
  std::string getStatusNum() {
    return first_line_msg.status_num;
  } // strong guarantee
};

#endif // PROXY_RESPONSE_H
