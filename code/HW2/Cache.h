#include "Request.h"
#include "Response.h"
#include <ctime>
#include <list>
#include <string>
#include <unordered_map>

typedef unordered_map<string, Response> cachemap;

class Cache {
public:
  Cache();
  bool validate(Request &request, Response &cache_response, string &message);
  int update(const Response &response);

private:
  bool checkControlHeader(Http &http);
  bool checkNoCacheField(Http &http);
  bool findCache(std::string &url);
  bool checkExpireHeader(Response &response);
  bool checkFresh(const Request &request, const Response &response);
  bool timeCompare(time_t t1, time_t t2);
  time_t getUTCTime(std::string time);
  time_t addTime(time_t time, string value);
  void eraseAllSubStr(std::string &mainStr, const std::string &toErase);
  void writeLog(const char *info);
  cachemap caches;
};

Cache::Cache() {}

/*
 Cache Validate
if (cache_response.receive_time + cache_response.max_age > current_time
 */
bool Cache::validate(Request &request, Response &cache_response,
                     std::string &message) {

  /* ----Request Level----- */
  // Step.1 Check Request Cache Control Headers
  if (checkControlHeader(request) == true) {
    // Maybe Useful
    // Step 1.1 no-cache header
    if (checkNoCacheField(request) == true) {
      message = "Request no-cache";
      return false;
    }
  }

  // Step.2 Find Cache
  if (findCache(request.getHost()) == true) {
    getCache(request.getHost(), cache_response);

    /* ----Cache Level----- */
    // Step.3 Cache Control in cache repsonse
    if (checkControlHeader(cache_response) == true) {
      if (checkNoCacheField(cache_response) == true) {
        message = "in cache, no-cache";
        return false;
      }
      // Step.4 Check Freshness

    }
    // Step.3 Expire in cache repsonse
    else if (checkExpireHeader(cache_response) == true) {

    } else {
      // Maybe useful
    }

  } else {
    message = "not in cache";
    return false;
  }
  return true;
}

bool Cache::findCache(string &url) {
  auto iter = caches.find(url);
  return iter == caches.end() ? false : true;
  if (iter == caches.end()) {
    return false;
  } else {
    return true;
  }
}

void Cache::getCache(string &url, Response &cache_response) {
  auto iter = caches.find(url);
  cache_response = *iter;
}

bool Cache::find_block(const Response &server_response) {
  const string &host = server_response.get_host();
  auto = caches.find(host);
  if (iter == caches.end()) {
    return false;
  } else {
    auto &block_list = iter->second;
    auto list_iter = std::find(block_list.begin(), block_list.end(), 1);
    if (list_iter == block_list.end()) {
      return false;
    } else {
      return true;
    }
  }
}

bool Cache::checkControlHeader(Http &http) {
  return http.checkExistsHeader("Cache-Control");
}

bool Cache::checkNoCacheField(Http &http) {
  std::string header = "no-cache";
  return http.checkExistsControlHeader(header);
}

bool Cache::checkExpireHeader(Response &response) {
  return response.checkExistsHeader("Expires");
}

void Cache::writeLog(const char *info) { return; }

bool Cache::timeCompare(time_t t1, time_t t2) {
  float tinterval = difftime(t1, t2);
  if (tinterval > 0)
    return true;
  else
    return false;
}

void Cache::eraseAllSubStr(std::string &mainStr, const std::string &toErase) {
  size_t pos = std::string::npos;

  // Search for the substring in string in a loop untill nothing is found
  while ((pos = mainStr.find(toErase)) != std::string::npos) {
    // If found then erase it from string
    mainStr.erase(pos, toErase.length());
  }
}

time_t Cache::getUTCTime(std::string time) {
  struct tm tm;
  eraseAllSubStr(time, " GMT");
  strptime(time.c_str(), "%a, %d %b %Y %H:%M:%S", &tm);
  time_t t = mktime(&tm);
  return t;
}

time_t Cache::addTime(time_t time, std::string value) {
  int add_value = stoi(value);
  return time + add_value;
}
int Cache::update(const Response &response) {

  if (server_response.isContained("no_store")) {
    return;
  } else {
    Response &cache_block;
                if(find_block(server_response){
      cache_block = get_block(server_response);
      cache_block = server_response;
		}else{
      cache_block = new_block(server_response);
		}
  }
}
