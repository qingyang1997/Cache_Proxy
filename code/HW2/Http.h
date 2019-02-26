#ifndef PROXY_HTTP_H
#define PROXY_HTTP_H

#include "common.h"
#include "myexception.h"

class Http {
private:
  int uid;
  std::string first_line;
  std::map<std::string, std::string> header_pair;
  std::map<std::string, std::string> Cache_Control;
  std::string body;

public:
  Http() {}
  virtual ~Http(){};
  Http(const Http &rhs) { // strong guarantee
    // uid = rhs.uid;
    // first_line = rhs.first_line;
    // header_pair = rhs.header_pair;
    // Cache_Control = rhs.Cache_Control;
    // body = rhs.body;
  }

  // Http &operator=(const Http &rhs) { // strong guarantee
  //   if (this == &rhs) {
  //     return *this;
  //   }
  //   Http temp;
  //   try {
  //     temp.uid = rhs.uid;
  //     temp.first_line = rhs.first_line;
  //     temp.header_pair = rhs.header_pair;
  //     temp.Cache_Control = rhs.Cache_Control;
  //     temp.body = rhs.body;
  //   } catch (...) {
  //     throw ErrorException("Http = failed");
  //   }
  //   std::swap(temp, *this);
  //   return *this;
  // }

  // Http &operator=(const Http &rhs) { // strong guarantee
  //   if (this == &rhs) {
  //     return *this;
  //   }
  //   //    Http temp;
  //   try {
  //     uid = rhs.uid;
  //     first_line = rhs.first_line;
  //     header_pair = rhs.header_pair;
  //     Cache_Control = rhs.Cache_Control;
  //     body = rhs.body;
  //   } catch (...) {
  //     throw ErrorException("Http = failed");
  //   }
  //   // std::swap(temp, *this);
  //   return *this;
  // }

  virtual void parseFirstLine(){};
  void parseHeader(std::string &message);
  void reconstructHeader(std::string &destination);
  void parseByLine(std::string &message);
  void updateBody(char *buff, int length) { // basic guarantee
    for (int i = 0; i < length; ++i) {
      body.push_back(buff[i]);
    }
  }

  std::string getValue(std::string &key);
  void parseCacheControl();
  void setUid(int s_uid) { uid = s_uid; }           // no throw
  int getUid() { return uid; }                      // no throw
  std::string getFirstLine() { return first_line; } // strong guarantee
  std::string &getBody() { return body; }           // strong guarantee
  std::string getCacheControlValue(std::string key);

  bool checkExistsHeader(const char *header_name);
  bool checkExistsControlHeader(std::string header_name);
  void addHeaderPair(std::string &key, std::string &value);
};

void Http::addHeaderPair(std::string &key,
                         std::string &value) { // strong guarantee
  // what if the key is already there?
  // should this function support update?
  header_pair[key] = value;
}
void Http::parseByLine(std::string &message) { // strong guarantee
  size_t start = 0;
  size_t end = message.find("\r\n");
  while (1) {
    size_t space = message.find(' ', start);
    if (space == message.npos) {
      throw ErrorException("Invalid request header");
    }
    std::string key = message.substr(start, space - start - 1);
    std::string value = message.substr(space + 1, end - space - 1);
    if (key.empty() || value.empty()) {
      throw ErrorException("Invalid request header");
    }

    header_pair[key] = value;
    start = end + 2;
    if (start >= message.size()) {
      break;
    }
    end = message.find("\r\n", start);
  }
}

void Http::parseHeader(std::string &message) { // strong guarantee
  size_t end_pos = message.find("\r\n");
  if (end_pos == message.npos) {
    throw ErrorException("Invalid request header");
  }
  std::string first_line = message.substr(0, end_pos);
  this->first_line = first_line;
  std::string remain = message.substr(end_pos + 2);
  end_pos = remain.find("\r\n\r\n");
  if (end_pos == remain.npos) {
    throw ErrorException("Invalid request header");
  }
  remain = remain.substr(0, end_pos + 2);
  parseByLine(remain);
  parseCacheControl();
  parseFirstLine();
  // error check?
}

void Http::reconstructHeader(std::string &destination) { // strong guarantee
  std::string tmp = first_line;
  std::map<std::string, std::string>::iterator it = header_pair.begin();
  while (it != header_pair.end()) {
    if (it->first == "Host") {
      size_t cut = first_line.find(it->second);
      if (cut != first_line.npos) {
        size_t space = first_line.find(' ');
        tmp = tmp.substr(0, space + 1) + tmp.substr(cut + it->second.size());
      }
    }
    tmp += "\r\n";
    tmp += it->first + ": ";
    tmp += (it->first == "Connection") ? "close" : it->second;
    ++it;
  }
  tmp += "\r\n\r\n";
  destination = tmp;
}

std::string Http::getValue(std::string &key) { // strong guarantee
  std::map<std::string, std::string>::iterator it = header_pair.find(key);
  return it == header_pair.end() ? "" : it->second;
}

bool Http::checkExistsHeader(const char *header_name) { // strong guarantee
  std::string header = header_name;
  std::string header_value = getValue(header);
  if (header_value == "")
    return false;
  else
    return true;
}

bool Http::checkExistsControlHeader(
    std::string header_name) { // strong guarantee
  std::map<std::string, std::string>::iterator it =
      Cache_Control.find(header_name);
  if (it == Cache_Control.end())
    return false;
  else
    return true;
}

void Http::parseCacheControl() { // strong guarantee
  std::map<std::string, std::string>::iterator it =
      header_pair.find("Cache-Control");
  if (it == header_pair.end()) {
    return;
  }
  std::string cache_control = it->second;
  std::stringstream ss;
  ss.str(it->second);
  std::string tmp = "";
  while (getline(ss, tmp, ',')) { // seperate Cache-Control by comma
    size_t comma = tmp.find('=');
    if (comma != tmp.npos) {
      Cache_Control[tmp.substr(0, comma)] = tmp.substr(comma + 1);
    } else {
      Cache_Control[tmp] = "";
    }
  }
}
std::string Http::getCacheControlValue(std::string key) { // strong guarantee
  std::map<std::string, std::string>::iterator it = Cache_Control.find(key);
  if (it == Cache_Control.end()) {
    return "";
  }
  return it->second;
}
#endif // PROXY_HTTP_H
