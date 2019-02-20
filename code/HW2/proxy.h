// created by panjoy 2/20/2018
#include <arpa/inet.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <netdb.h>
#include <netinet/in.h>
#include <set>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <vector>

#define NOCACHE 1
#define NOSTORE 2
#define MR 3 // must-revalidate
std::string bad_request = "HTTP/1.1 400 Bad Request";
std::string bad_gateway = "HTTP/1.1 502 Bad Gateway";
int UID = 0;
std::mutex mtx;
time_t get_current_time() {
  // UTC standard
  time_t now;
  struct tm *timeinfo;
  time(&now);
  timeinfo = gmtime(&now);
  return mktime(timeinfo);
}
time_t parse_gmt_time(std::string temp) {

  tm tmobj;
  std::string a;
  std::string mon[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  size_t filter = temp.find(" ");
  size_t len = temp.length();
  if (filter == std::string::npos && (filter + 1) > len) {
    return 0;
  }
  temp = temp.substr(filter + 1);

  filter = temp.find(" ");
  len = temp.length();
  if (filter == std::string::npos && (filter + 1) > len) {
    return 0;
  }
  a = temp.substr(0, filter);
  tmobj.tm_mday = atoi(a.c_str());
  temp = temp.substr(filter + 1);

  filter = temp.find(" ");
  len = temp.length();
  if (filter == std::string::npos && (filter + 1) > len) {
    return 0;
  }
  a = temp.substr(0, filter);
  for (int i = 0; i < 12; ++i) {
    if (a.compare(mon[i]) == 0) {
      tmobj.tm_mon = i;
    }
  }
  temp = temp.substr(filter + 1);

  filter = temp.find(" ");
  len = temp.length();
  if (filter == std::string::npos && (filter + 1) > len) {
    return 0;
  }
  a = temp.substr(0, filter);
  tmobj.tm_year = atoi(a.c_str()) - 1900;
  temp = temp.substr(filter + 1);

  filter = temp.find(":");
  len = temp.length();
  if (filter == std::string::npos && (filter + 1) > len) {
    return 0;
  }
  a = temp.substr(0, filter);
  tmobj.tm_hour = atoi(a.c_str());
  temp = temp.substr(filter + 1);

  filter = temp.find(":");
  len = temp.length();
  if (filter == std::string::npos && (filter + 1) > len) {
    return 0;
  }
  a = temp.substr(0, filter);
  tmobj.tm_min = atoi(a.c_str());
  temp = temp.substr(filter + 1);

  filter = temp.find(" ");
  len = temp.length();
  if (filter == std::string::npos && (filter + 1) > len) {
    return 0;
  }
  a = temp.substr(0, filter);
  tmobj.tm_sec = atoi(a.c_str());
  temp = temp.substr(filter + 1);

  if (temp.compare("GMT") != 0) {
    return 0;
  }
  return mktime(&tmobj);
}
class cache_control {
private:
  long age;
  long maxage;
  long smaxage;
  long expires;
  long date;
  long last_modi;
  bool nocache;
  bool nostore;
  bool must_reval;
  bool pri;
  std::string etag;

public:
  cache_control() {
    age = 0;
    date = 0;
    maxage = 0;  //
    smaxage = 0; //
    expires = 0;
    last_modi = 0;
    nocache = false;    //
    nostore = false;    //
    must_reval = false; //
    pri = false;        //
  }
  void print_cc() {
    std::cout << "age is " << age << std::endl;
    std::cout << "maxage is " << maxage << std::endl;
    std::cout << "smaxage is " << smaxage << std::endl;
    std::cout << "expires is " << expires << std::endl;
    std::cout << "last_modi is " << last_modi << std::endl;
    std::cout << "date is " << date << std::endl;
    std::cout << "nocache is " << nocache << std::endl;
    std::cout << "nostore is " << nostore << std::endl;
    std::cout << "must_reval is " << must_reval << std::endl;
    std::cout << "private is " << pri << std::endl;
    std::cout << "etag is " << etag << std::endl;
  }
  int cache_status() {
    if (get_nocache()) {
      return NOCACHE;
    }
  }
  void set_etag(std::string temp) { etag = temp; }
  void set_age(long temp) { age = temp; }
  void set_date(long temp) { date = temp; }
  void set_maxage(long temp) { maxage = temp; }
  void set_smaxage(long temp) { smaxage = temp; }
  void set_expires(long temp) { expires = temp; }
  void set_last_modi(long temp) { last_modi = temp; }
  void set_nocache(bool temp) { nocache = temp; }
  void set_nostore(bool temp) { nostore = temp; }
  void set_private(bool temp) { pri = temp; }

  void set_mustreval(bool temp) { must_reval = temp; }
  std::string get_etag() { return etag; }
  long get_age() { return age; }
  long get_date() { return date; }
  long get_maxage() { return maxage; }
  long get_smaxage() { return smaxage; }
  long get_expires() { return expires; }
  long get_last_modi() { return last_modi; }
  bool get_nocache() { return nocache; }
  bool get_nostore() { return nostore; }
  bool get_mustreval() { return must_reval; }
  bool get_private() { return pri; }
};
class request {
private:
  int uid;
  std::string request_line;
  std::string method;
  std::string agreement;
  std::string hostname;
  std::string uri;
  std::map<std::string, std::string> kv_table;
  std::vector<char> *content;
  int port_num;

public:
  request(int id) {
    uid = id;
    content = new std::vector<char>();
  }
  ~request() { delete content; }
  int parse_request_line(std::string http_request) {
    try {
      request_line = http_request;
      size_t filter = http_request.find_first_of(" ");
      // if(filter==std::string::npos){return -1;}
      method = std::string(http_request.substr(0, filter));
      if (method.compare("GET") == 0 || method.compare("POST") == 0) {
        size_t filter3 = http_request.find_first_of(":");
        agreement = http_request.substr(filter + 1, filter3 - filter - 1);
        size_t filter2 = http_request.find_first_of("/");
        http_request = http_request.substr(filter2 + 2);
        filter2 = http_request.find_first_of("/");
        hostname = std::string(http_request.substr(0, filter2));
        filter = http_request.find_first_of(" ");
        uri = std::string(http_request.substr(filter2, filter - filter2));
        if ((filter = hostname.find_first_of(":")) != std::string::npos) {
          port_num = atoi(hostname.substr(filter + 1).c_str());
          hostname = hostname.substr(0, filter);
        } else {
          if (agreement.compare("http") == 0) {
            port_num = 80;
          } else if (agreement.compare("https") == 0) {
            port_num = 443;
          }
        }
      } else if (method.compare("CONNECT") == 0) {
        http_request = http_request.substr(filter + 1);
        filter = http_request.find_first_of(":");
        hostname = std::string(http_request.substr(0, filter));
        http_request = http_request.substr(filter + 1);
        filter = http_request.find_first_of(" ");
        port_num = atoi(http_request.substr(0, filter).c_str());
      } else {
        return -1;
      }
      return 0;
    } catch (...) {
      return -1;
    }
  }
  int add_kv(std::string temp) { // might be repeat
    try {
      if (temp.length() == 0) {
        return -1;
      }
      size_t colon = temp.find_first_of(":");
      if (colon == std::string::npos) {
        std::cout << temp << std::endl;
        std::cout << "invalid KV" << std::endl;
        return -1;
      }
      kv_table.insert(std::pair<std::string, std::string>(
          temp.substr(0, colon), temp.substr(colon + 2)));
    } catch (...) {
      return -1;
    }
    return 0;
  }
  void update_content(char data[], size_t size) {
    for (size_t i = 0; i < size; ++i) {
      content->push_back(data[i]);
    }
  }
  std::string get_request() {
    std::string err("construct request failed");
    std::string res = request_line;
    std::map<std::string, std::string>::iterator it = kv_table.begin();
    while (it != kv_table.end()) {
      if (it->first.compare("Host") == 0) {
        size_t cut = request_line.find(it->second);
        // std::cout<<cut<<"\r\n"<<it->second<<request_line;
        if (cut != std::string::npos && cut > 7) {
          res = request_line.substr(0, cut - 7);
          if (cut + it->second.length() > request_line.length()) {
            throw err;
          }
          res = res + request_line.substr(cut + it->second.length());
        } else {
          throw err;
        }
        break;
      }
      ++it;
    }

    it = kv_table.begin();
    while (it != kv_table.end()) {
      res += "\r\n";
      res += it->first;
      res += ": ";
      if (it->first.compare("Connection") == 0) {
        res += "close";
      } else {
        res += it->second;
      }
      ++it;
    }
    res += "\r\n\r\n";
    return res;
  }
  size_t get_length() {
    std::map<std::string, std::string>::iterator it =
        kv_table.find("Content-Length");
    if (it != kv_table.end()) {
      return (size_t)atoi(it->second.c_str());
    }
    return 0;
  }
  std::string get_port() {
    std::map<std::string, std::string>::iterator it = kv_table.find("Host");
    if (it != kv_table.end()) {
      size_t filter = it->second.find_first_of(":");
      return it->second.substr(filter + 1);
    }
    return "INVALID";
  }
  void set_cache_info(cache_control *cc) {
    std::map<std::string, std::string>::iterator it = kv_table.find("Age");
    if (it != kv_table.end()) {
      cc->set_age((long)atoi(it->second.c_str()));
    }
    it = kv_table.find("ETag");
    if (it != kv_table.end()) {
      cc->set_etag(it->second);
    }
    it = kv_table.find("Date");
    if (it != kv_table.end()) {
      cc->set_date((long)parse_gmt_time(it->second));
    }
    it = kv_table.find("Expires");
    if (it != kv_table.end()) {
      cc->set_expires((long)parse_gmt_time(it->second));
    }
    it = kv_table.find("Last-Modified");
    if (it != kv_table.end()) {
      cc->set_last_modi((long)parse_gmt_time(it->second));
    }
    it = kv_table.find("Cache-Control");
    if (it != kv_table.end()) {
      std::string temp = it->second;
      size_t filter = 0;
      while ((filter = temp.find_first_of(",")) != std::string::npos) {
        std::string a = temp.substr(0, filter);
        temp = temp.substr(filter + 2);
        if (a.compare("no-cache") == 0) {
          cc->set_nocache(true);
          continue;
        }
        if (a.compare("no-store") == 0) {
          cc->set_nostore(true);
          continue;
        }
        if (a.compare("must-revalidate") == 0) {
          cc->set_mustreval(true);
          continue;
        }
        if (a.compare("private") == 0) {
          cc->set_private(true);
          continue;
        }
        size_t index = 0;
        if ((index = a.find_first_of("=")) != std::string::npos) {
          if (a.substr(0, index).compare("max-age") == 0) {
            cc->set_maxage((long)atoi(a.substr(index + 1).c_str()));
          }
          if (a.substr(0, index).compare("s-maxage") == 0) {
            cc->set_smaxage((long)atoi(a.substr(index + 1).c_str()));
          }
        }
      }
      if (temp.compare("no-cache") == 0) {
        cc->set_nocache(true);
      }
      if (temp.compare("no-store") == 0) {
        cc->set_nostore(true);
      }
      if (temp.compare("must-revalidate") == 0) {
        cc->set_mustreval(true);
      }
      if (temp.compare("private") == 0) {
        cc->set_private(true);
      }
      size_t index = 0;
      if ((index = temp.find_first_of("=")) != std::string::npos) {
        if (temp.substr(0, index).compare("max-age") == 0) {
          cc->set_maxage((long)atoi(temp.substr(index + 1).c_str()));
        }
        if (temp.substr(0, index).compare("s-maxage") == 0) {
          cc->set_smaxage((long)atoi(temp.substr(index + 1).c_str()));
        }
      }
    }
  }
  int get_portnum() { return port_num; }
  std::string get_hostname() { return hostname; }
  std::string get_agreement() { return agreement; }
  std::string get_request_line() { return request_line; }
  std::string get_method() { return method; }
  std::string get_URI() { return uri; }
  std::vector<char> *get_content() { return content; }
  int get_uid() { return uid; }
};
class response {
private:
  int uid;
  std::string status_line;
  int status;
  std::string agreement;
  std::string detail;
  std::map<std::string, std::string> kv_table;
  std::vector<char> *content;

public:
  response(int id) {
    uid = id;
    content = new std::vector<char>();
  }
  response(const response &rhs) {
    uid = rhs.uid;
    status_line = rhs.status_line;
    status = rhs.status;
    agreement = rhs.agreement;
    detail = rhs.detail;
    std::map<std::string, std::string>::const_iterator it =
        rhs.kv_table.begin();
    while (it != rhs.kv_table.end()) {
      kv_table.insert(
          std::pair<std::string, std::string>(it->first, it->second));
    }
    content = new std::vector<char>(*rhs.content);
  }
  ~response() { delete content; }
  int parse_status_line(std::string http_response) {
    try {
      status_line = http_response;
      size_t filter = http_response.find_first_of(" ");
      if (filter == std::string::npos) {
        return -1;
      }
      agreement = http_response.substr(0, filter);
      http_response = http_response.substr(filter + 1);
      filter = http_response.find_first_of(" ");
      if (filter == std::string::npos) {
        return -1;
      }
      std::string temp = http_response.substr(0, filter);
      status = (size_t)atoi(temp.c_str());
      http_response = http_response.substr(filter + 1);
      filter = http_response.find_first_of(" ");
      detail = http_response.substr(0, filter);
      return 0;
    } catch (...) {
      return -1;
    }
  }
  int add_kv(std::string temp) {
    try {
      if (temp.length() == 0) {
        return -1;
      }
      size_t colon = temp.find_first_of(":");
      if (colon == std::string::npos) {
        std::cout << "invalid KV" << std::endl;
        return -1;
      }
      kv_table.insert(std::pair<std::string, std::string>(
          temp.substr(0, colon), temp.substr(colon + 2)));
    } catch (...) {
      return -1;
    }
    return 0;
  }
  void update_content(char data[], size_t size) {
    for (size_t i = 0; i < size; ++i) {
      content->push_back(data[i]);
    }
  }
  void set_cache_info(cache_control *cc) {
    std::map<std::string, std::string>::iterator it = kv_table.find("Age");
    if (it != kv_table.end()) {
      cc->set_age((long)atoi(it->second.c_str()));
    }
    it = kv_table.find("ETag");
    if (it != kv_table.end()) {
      cc->set_etag(it->second);
    }
    it = kv_table.find("Date");
    if (it != kv_table.end()) {
      cc->set_date((long)parse_gmt_time(it->second));
    }
    it = kv_table.find("Expires");
    if (it != kv_table.end()) {
      cc->set_expires((long)parse_gmt_time(it->second));
    }
    it = kv_table.find("Last-Modified");
    if (it != kv_table.end()) {
      cc->set_last_modi((long)parse_gmt_time(it->second));
    }
    it = kv_table.find("Cache-Control");
    if (it != kv_table.end()) {
      // std::cout<<it->second<<"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC\r\n";
      std::string temp = it->second;
      size_t filter = 0;
      while ((filter = temp.find_first_of(",")) != std::string::npos) {
        std::string a = temp.substr(0, filter);
        // std::cout<<a<<"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\n";
        temp = temp.substr(filter + 2);
        if (a.compare("no-cache") == 0) {
          cc->set_nocache(true);
          continue;
        }
        if (a.compare("no-store") == 0) {
          cc->set_nostore(true);
          continue;
        }
        if (a.compare("must-revalidate") == 0) {
          cc->set_mustreval(true);
          continue;
        }
        if (a.compare("private") == 0) {
          cc->set_private(true);
          continue;
        }
        size_t index = 0;
        if ((index = a.find_first_of("=")) != std::string::npos) {
          if (a.substr(0, index).compare("max-age") == 0) {
            cc->set_maxage((long)atoi(a.substr(index + 1).c_str()));
          }
          if (a.substr(0, index).compare("s-maxage") == 0) {
            cc->set_smaxage((long)atoi(a.substr(index + 1).c_str()));
          }
        }
      }
      if (temp.compare("no-cache") == 0) {
        cc->set_nocache(true);
      }
      if (temp.compare("no-store") == 0) {
        cc->set_nostore(true);
      }
      if (temp.compare("must-revalidate") == 0) {
        cc->set_mustreval(true);
      }
      if (temp.compare("private") == 0) {
        cc->set_private(true);
      }
      size_t index = 0;
      if ((index = temp.find_first_of("=")) != std::string::npos) {
        if (temp.substr(0, index).compare("max-age") == 0) {
          // std::cout<<temp.substr(index+1)<<"MAMAMAMAMAMAMAMAMAMAMAMAMAMAMAMAMAMA\r\n";
          cc->set_maxage((long)atoi(temp.substr(index + 1).c_str()));
        }
        if (temp.substr(0, index).compare("s-maxage") == 0) {
          cc->set_smaxage((long)atoi(temp.substr(index + 1).c_str()));
        }
      }
    }
  }
  std::string get_response() {
    std::string res = status_line;
    std::map<std::string, std::string>::iterator it = kv_table.begin();
    while (it != kv_table.end()) {
      res += "\r\n";
      res += it->first;
      res += ": ";
      res += it->second;
      ++it;
    }
    res += "\r\n\r\n";
    return res;
  }
  void update_uid(int id) { uid = id; }
  size_t get_length() {
    std::map<std::string, std::string>::iterator it =
        kv_table.find("Content-Length");
    if (it != kv_table.end()) {
      return (size_t)atoi(it->second.c_str());
    }
    return 0;
  }
  int get_status() { return status; }
  std::string get_status_line() { return status_line; }
  std::string get_agreement() { return agreement; }
  std::string get_detail() { return detail; }
  std::vector<char> *get_content() { return content; }
  int get_uid() { return uid; }
};

class Cache {
private:
  size_t capacity;
  size_t used;
  std::map<std::string, response *> database;

public:
  Cache(size_t size) {
    used = 0;
    capacity = size; // max store capacity bytes
  }
  ~Cache() {
    std::map<std::string, response *>::iterator it = database.begin();
    while (it != database.end()) {
      delete it->second;
      it++;
    }
  }
  response *find(std::string uri) {
    std::map<std::string, response *>::iterator it = database.find(uri);
    if (it != database.end()) {
      return it->second;
    }
    return NULL;
  }
  void delete_cache() {
    // delete smallest uid, which means eariliest response
    try {
      std::lock_guard<std::mutex> lck(mtx);
    } catch (std::logic_error &) {
      std::cout << "[exception caught] when delete from cache\n";
    }
    if (database.empty()) {
      std::cout << "cache is empty, can not delete" << std::endl;
      return;
    }
    std::map<std::string, response *>::iterator it = database.begin();
    std::map<std::string, response *>::iterator temp = it;
    int id = (int)capacity;
    while (it != database.end()) {
      if (it->second->get_uid() < id) {
        id = it->second->get_uid();
        temp = it;
      }
      it++;
    }
    used = used - temp->second->get_content()->size();
    delete temp->second;
    database.erase(temp);
    if (used < 0) {
      std::cout << "FATAL ERR, CACHE STORE LESS THAN 0\r\n";
    }
  }
  void add_to_cache(std::string URI, response *http_response) {
    try {
      std::lock_guard<std::mutex> lck(mtx);
    } catch (std::logic_error &) {
      std::cout << "[exception caught] when add to cache\n";
    }
    while (used + http_response->get_content()->size() > capacity) {
      if (http_response->get_content()->size() > capacity) {
        std::cout
            << "oversize single file, we can not store it in this cache\r\n";
        return;
      }
      delete_cache();
    }
    std::map<std::string, response *>::iterator it = database.find(URI);
    if (it != database.end()) {
      used = used - it->second->get_content()->size();
      delete it->second;
      database.erase(it);
    }
    std::cout << "cache cap is " << capacity << " used size is " << used
              << " insert size is " << http_response->get_content()->size()
              << "\r\n";
    used = used + http_response->get_content()->size();
    database.insert(std::pair<std::string, response *>(URI, http_response));
  }
};
class proxy {
private:
  int host_socket_fd;
  int port;
  std::string hostname;

public:
  proxy(int port_num) {
    host_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    port = port_num;
    char host[64];
    gethostname(host, 64);
    hostname = std::string(host);
  }
  proxy(const proxy &rhs) {
    host_socket_fd = rhs.host_socket_fd;
    port = rhs.port;
    hostname = rhs.hostname;
  }
  ~proxy() { close(host_socket_fd); }
  int create_socket_fd() {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    return socket_fd;
  }
  void close_socket_fd(int socket_fd) { close(socket_fd); }
  int connect_host(std::string hostname, int port, int socket_fd) {
    struct sockaddr_in server_in;
    // memset(server_in,0,sizeof(server_in));
    server_in.sin_family = AF_INET;
    server_in.sin_port = htons(port);

    struct hostent *host_info = gethostbyname(hostname.c_str());
    if (host_info == NULL) {
      std::cout << hostname << " host info is null" << std::endl;
      return -1;
    }
    memcpy(&server_in.sin_addr, host_info->h_addr_list[0], host_info->h_length);
    int connect_status =
        connect(socket_fd, (struct sockaddr *)&server_in, sizeof(server_in));
    return connect_status;
  }
  int transfer_TLS(int sour_fd, int dest_fd) {
    std::vector<char> temp;
    temp.resize(8192);
    size_t cap;
    cap = recv(sour_fd, &temp.data()[0], 8192, 0);
    temp.resize(cap);
    // std::cout<<"get "<<cap<<" from "<<sour_fd;
    if (cap == 0) {
      std::cout << "TUNNEL CLOSED\r\n";
      return -1;
    }
    send(dest_fd, &temp.data()[0], cap, 0);
    // std::cout<<" send "<<cap<<" to "<<dest_fd<<std::endl;
    return 0;
  }
  size_t send_message(int socket_fd, std::vector<char> *content) {
    if (socket_fd == 0) {
      std::cout << "socket not established" << std::endl;
    }
    size_t sent = 0;
    // std::cout<<"size is "<<content->size()<<std::endl;
    while (1) {
      if (sent + 1024 < content->size()) {
        sent += send(socket_fd, &(content->data()[sent]), 1024, 0);
      } else {
        sent += send(socket_fd, &(content->data()[sent]),
                     content->size() - sent, 0);
        break;
      }
    }
    std::cout << "successfully send " << sent << "bytes\r\n";
    return sent;
  }
  void send_header(std::string header, int socket_fd) {
    if (socket_fd == 0) {
      std::cout << "socket not established" << std::endl;
    }
    char message[8192];
    memset(message, 0, sizeof(message));
    memcpy(message, header.c_str(), header.length());
    send(socket_fd, message, header.length(), 0);
  }
  int recv_request_header(request *HTTP, int socket_fd) {
    char message[8192];
    memset(message, 0, sizeof(message));
    size_t cap = recv(socket_fd, &message, sizeof(message),
                      0); // std::cout<<cap<<" line 336\r\n";
    if (cap == 0) {
      std::cout << "client close socket\r\n";
      std::string temp("recive header failed for client close socket\r\n");
      throw temp;
      return -1;
    }
    std::string temp(message);
    std::cout << "raw header is " << temp << "\r\n"
              << "size is " << temp.length() << "\r\n";
    size_t sign = 0;
    size_t filter = 0;
    while ((filter = temp.find_first_of("\r\n")) != 0) {
      if (HTTP->get_request_line().length() == 0) {
        std::cout << "request line is " << temp.substr(0, filter) << "\r\n";
        int flag = HTTP->parse_request_line(temp.substr(0, filter));
        if (flag < 0) {
          throw bad_request;
        }
      } else {
        int flag = HTTP->add_kv(temp.substr(0, filter));
        if (flag < 0) {
          throw bad_request;
        }
      }
      temp = temp.substr(filter + 2);
      sign += filter;
      sign += 2;
    }
    sign += 2;
    if (sign < cap) {
      for (size_t i = sign; i < cap; ++i) {
        HTTP->get_content()->push_back(message[i]);
      }
    }
    return 0;
  }
  void recv_response_header(response *HTTP, int socket_fd) {
    char message[8192];
    memset(message, 0, sizeof(message));
    size_t cap = recv(socket_fd, &message, sizeof(message), 0);
    std::string temp(message);
    size_t sign = 0;
    size_t filter = 0;
    while ((filter = temp.find_first_of("\r\n")) != 0) {
      if (HTTP->get_status_line().length() == 0) {
        int flag = HTTP->parse_status_line(temp.substr(0, filter));
        if (flag < 0) {
          throw bad_gateway;
        }
      } else {
        int flag = HTTP->add_kv(temp.substr(0, filter));
        if (flag < 0) {
          throw bad_gateway;
        }
      }
      temp = temp.substr(filter + 2);
      sign += filter;
      sign += 2;
    }
    sign += 2;
    std::cout << "response header size is " << sign << " recv size is " << cap
              << "\r\n\r\n";
    if (sign < cap) {
      for (size_t i = sign; i < cap; ++i) {
        HTTP->get_content()->push_back(message[i]);
      }
    }
    // std::cout<<"header size is "<<sign<<" recv size is "<<cap<<" content size
    // is "<<HTTP->get_content()->size()<<std::endl;
  }
  int recv_message(int socket_fd, std::vector<char> *v, size_t length) {
    size_t cap = 0;
    size_t index = v->size();
    if (length > 0) {
      while (v->size() < length) {
        v->resize(index + 1024);
        cap = recv(socket_fd, &(v->data()[index]), 1024, 0);
        index += cap;
        if (cap < 1024 && cap > 0) {
          v->resize(index);
        }
        if (cap == 0) {
          v->resize(index);
          std::cout << "socket close\r\n";
          return -1;
        }
      }
    } else {
      cap = 1;
      while (cap != 0) {
        v->resize(index + 1024);
        cap = recv(socket_fd, &(v->data()[index]), 1024, 0);
        index += cap;
        if (cap < 1024 && cap > 0) {
          v->resize(index);
        }
        if (cap == 0) {
          v->resize(index);
          std::cout << "socket close\r\n";
          return -1;
        }
      }
    }
    std::cout << "successfully recv " << index << "bytes\r\n";
    return 0;
  }
  void bind_addr() {
    struct hostent *host_info = gethostbyname(hostname.c_str());
    struct sockaddr_in server_in;
    server_in.sin_family = AF_INET;
    server_in.sin_port = htons(port);
    memcpy(&server_in.sin_addr, host_info->h_addr_list[0], host_info->h_length);
    int bind_status =
        bind(host_socket_fd, (struct sockaddr *)&server_in, sizeof(server_in));
    if (bind_status < 0) {
      std::cout << "bind fail" << std::endl;
    }
    listen(host_socket_fd, 5);
  }
  int accept_connection(struct sockaddr_in *incoming) {
    socklen_t len = sizeof(*incoming);
    return accept(host_socket_fd, (struct sockaddr *)incoming, &len);
  }
};
class Log {
private:
  std::string path;

public:
  Log(std::string logpath) : path(logpath) {
    const char *cpath = path.c_str();
    mkdir(cpath, ACCESSPERMS);
  }
  void add(std::string str) {
    char buf[512]; // to store the cwd
    getcwd(buf, 512);
    const char *cpath = path.c_str();
    chdir(cpath);
    std::ofstream logfile("proxy.log", std::ios_base::out | std::ios_base::app);
    logfile << str << "\r\n";
    chdir(buf);
  }
  ~Log() {}
};

/*

*/
