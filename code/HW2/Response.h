//
// Created by Duck2 on 2019-02-19.
//

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
};

#endif // PROXY_RESPONSE_H
