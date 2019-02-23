#include "Request.h"
#include "Response.h"
#include <list>
#include <string>
#include <unordered_map>

typedef unordered_map<string, Response> cachemap;

class Cache {
public:
  Cache();
  bool validate(Request &request, Response &cache_response, string &message);
  void update_block(const Response &server_response);

private:
  bool checkControlHeader(Http &http);
  bool checkNoCacheField(Http &http);
  bool findCache(string &url);
  void getCache(string &url, Response &cache_response);
  bool checkExpireHeader(Request &request);
  bool checkFresh(const Request &request, const Response &response);
  void writeLog(const char *info);
  cachemap caches;
};

Cache::Cache() {}

/*
 Cache Validate
 */
bool Cache::validate(Request &request, Response &cache_response,
                     string &message) {

  /* ----Request Level----- */
  // Step.1 Check Request Cache Control Headers
  if (checkControlHeader(request) == false) {
    // Step.2 Find Cache
    if (findcache(request.get_host()==false) {
      message = "request contains no_cache, request server for fresh data";
      return false;
    } else {

      return false;
      } else {
      if (cache_response.receive_time + cache_response.max_age > current_time) {
        message = "in cache, requires validation";
        return false;
      } else {
        return true;
      }
      }
    message = "Request no-cache";
    return false;
  } else {
  }
}
return true;
}

bool Cache::findCache(string &url) {
  auto iter = caches.find(url);
  return iter == caches.end() ? false : true;
  if (iter == caches.end()) {
    return false;
  } else {
    auto &block_list = iter->second;
    auto list_iter = std::find(block_list.begin(), block_list.end(),
                               client_request.get_sub_url());
    if (list_iter == block_list.end()) {
      return false;
    } else {
      cache_response = *list_iter;
      return true;
    }
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

bool Cache::checkExpireHeader(Request &request) {
  return request.checkExistsHeader("Expires");
}

void Cache::writeLog(const char *info) { return; }
void Cache::update_block(const Response &server_response) {
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
