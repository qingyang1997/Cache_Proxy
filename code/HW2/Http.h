//
// Created by Duck2 on 2019-02-19.
//

#ifndef PROXY_HTTP_H
#define PROXY_HTTP_H

#include "common.h"
#include "myexception.h"
using namespace std;

class Http {
public:
  int uid;
  string first_line;
  unordered_map<string, string> header_pair;
  unordered_map<string, string> Cache_Control;
  string body;
  Http() {}
  virtual ~Http(){};
  virtual void parse_first_line(){};
  void parse_head(string &message);
  void reconstruct_header(string &destination);
  void parse_by_line(string &message,
                     unordered_map<string, string> &header_pair);
  void update_body(char *buff, int length) {
    for (int i = 0; i < length; ++i) {
      body.push_back(buff[i]);
    }
  }
  string get_value(string &key);
  void parse_cache_control();
  void set_uid(int s_uid) { uid = s_uid; }
  int get_uid() { return uid; }
  string get_first_line() { return first_line; }
  string &get_body() { return body; }
};

void Http::parse_by_line(string &message,
                         unordered_map<string, string> &header_pair) {
  size_t start = 0;
  size_t end = message.find("\r\n");
  while (1) {
    size_t space = message.find(' ', start);
    string key = message.substr(start, space - start - 1);
    string value = message.substr(space + 1, end - space - 1);
    // if (key == "Host") {
    //   size_t colon = value.find(':');
    //   if (colon != value.npos) {
    //     string port = value.substr(colon + 1);
    //     header_pair["Port"] = port;
    //   }
    // }
    header_pair[key] = value;
    start = end + 2;
    if (start >= message.size()) {
      break;
    }
    end = message.find("\r\n", start);
  }
}

void Http::parse_head(string &message) {
  size_t end_pos = message.find("\r\n");
  if (end_pos == message.npos) {
    throw ErrorException("Invalid request header");
  }
  string first_line = message.substr(0, end_pos);
  this->first_line = first_line;
  string remain = message.substr(end_pos + 2);
  end_pos = remain.find("\r\n\r\n");
  if (end_pos == remain.npos) {
    throw ErrorException("missing end");
  }
  remain = remain.substr(0, end_pos + 2);
  parse_by_line(remain, header_pair);
  parse_cache_control();
  parse_first_line();
  // error check?
}

void Http::reconstruct_header(string &destination) {
  string tmp = first_line;
  unordered_map<string, string>::iterator it = header_pair.begin();
  while (it != header_pair.end()) {
    if (it->first == "Host") {
      size_t cut = first_line.find(it->second);
      if (cut != first_line.npos) {
        tmp = tmp.substr(0, cut - 7) + tmp.substr(cut + it->second.size());
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

string Http::get_value(string &key) {
  unordered_map<string, string>::iterator it = header_pair.find(key);
  return it == header_pair.end() ? "" : it->second;
}

void Http::parse_cache_control() {
  unordered_map<string, string>::iterator it =
      header_pair.find("Cache-Control");
  if (it == header_pair.end()) {
    return;
  }
  string cache_control = it->second;
  stringstream ss;
  ss.str(it->second);
  string tmp = "";
  while (getline(ss, tmp, ',')) {
    size_t comma = tmp.find('=');
    if (comma != tmp.npos) {
      Cache_Control[tmp.substr(0, comma)] = tmp.substr(comma + 1);
    } else {
      Cache_Control[tmp] = "TRUE";
    }
  }
}
#endif // PROXY_HTTP_H
