#include "Request.h"
#include "Response.h"
#include <list>
#include <string>
#include <unordered_map>

typedef unordered_map<string, list<Response>> cachemap;

class Cache {
public:
  Cache();
  bool validate(const Request &client_request, Response &cache_response,
                string &message);
  void update_block(const Response &server_response);

private:
  bool find_block(const Request &client_request, Response &cache_response);
  cachemap caches;
};

Cache::Cache() {}

/*
 Cache Validate
 */
bool Cache::validate(const Request &client_request, Response &cache_response,
                     string &message) {

  // Step1
  if (!find_block(client_request, cache_response)) {
    message = "not in cache";
    return false;
  } else {
    if (client_request.isContained("no_cache")) {
      message = "request contains no_cache, request server for fresh data";
      return false;
    } else {
      if (cache_response.receive_time < cache_response.expire) {
        message = "in cache, but expired at EXPIREDTIME";
        return false;
      } else {
        if (cache_response.receive_time + cache_response.max_age >
            current_time) {
          message = "in cache, requires validation";
          return false;
        } else {
          return true;
        }
      }
    }
  }
  return true;
}

bool Cache::find_block(const Request &client_request,
                       Response &cache_response) {
  const string &host = client_request.get_host();
  auto = caches.find(host);
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
