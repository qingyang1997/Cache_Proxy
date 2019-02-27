#include "Request.h"
#include "Response.h"
#include <ctime>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

#define CACHESIZE 300
typedef std::unordered_map<std::string, Response> cachemap;

std::mutex mtx_cache;

class Cache {
public:
  Cache();

  bool validate(Request &request, Response &cache_response,
                std::string &message);
  void update(Request &request, Response &response, std::string &message);

private:
  bool checkControlHeader(Http &http);
  bool checkControlField(Http &http, const char *header);
  bool checkReValidateField(Http &http);

  bool checkExpireHeader(Response &response);
  bool checkPragmaHeader(Http &http);
  double checkFresh(Response &cache_response, int max_age, int extra_age);

  bool findCache(std::string &url);
  Response getCache(std::string &url);
  void getCache(std::string &url, Response &cache_response);

  void replaceCache(Request &request, Response &response);

  time_t getExpireTime(Response &cache_response, int max_age, int extra_age);
  time_t getUTCTime(std::string time);
  time_t addTime(time_t time, int value);
  char *displayTime(time_t time);

  void eraseAllSubStr(std::string &mainStr, const std::string &toErase);

  void addValidateHeader(Request &request, Response &response);
  void removeValidateHeader(Request &request);

  void evictCache();
  void refreshLRU(std::string);

  std::list<std::string> lru_queue;

  cachemap caches;
};

Cache::Cache() {}

/*
 Cache Validate
if (cache_response.receive_time + cache_response.max_age > current_time
 */

bool Cache::validate(Request &request, Response &cache_response,
                     std::string &message) {

  //  Find Cache
  std::string host_name = request.getUrl();
  if (findCache(host_name) == true) {

    getCache(host_name, cache_response);
    int max_age = 999999999;
    int stale_age = 0;
    int extra_age = 0;

    //  Check Request Cache Control Headers
    if (checkControlHeader(request) == true) {

      std::string temp;
      request.reconstructHeader(temp);
      std::cout << temp << std::endl;
      if (checkControlField(request, "no-cache") == true) {
        message = "in cache, requires validation";
        addValidateHeader(request, cache_response);
        return false;
      } else {
        if (checkControlField(request, "max-age") == true) {
          max_age = std::stoi(request.getCacheControlValue("max-age"));
        }
        if (checkControlField(request, "max-stale") == true) {
          stale_age = std::stoi(request.getCacheControlValue("max-stale"));
        } else if (checkControlField(request, "min-fresh") == true) {
          extra_age = std::stoi(request.getCacheControlValue("min-fresh"));
        }
      }
    } else if (checkPragmaHeader(request) == true) {
      std::string pragma_header = "Pragma";
      std::string pragma = request.getValue(pragma_header);
      if (pragma.find("no-cache") != std::string::npos) {
        message = "in cache, requires validation";
        addValidateHeader(request, cache_response);
        return false;
      }
    }

    // Cache Control in cache repsonse
    if (checkControlHeader(cache_response) == true) {
      if (checkControlField(cache_response, "no-cache") == true) {
        message = "in cache, requires validation";
        addValidateHeader(request, cache_response);
        return false;
      }
      // Check Freshness
      if (checkControlField(cache_response, "max-age") == true) {
        max_age = std::min(
            max_age, std::stoi(cache_response.getCacheControlValue("max-age")));
      }
      if (max_age == 0) {
        message = "in cache, requires validation";
        addValidateHeader(request, cache_response);
        return false;
      }

      std::cout << "[CACHES] Uid " << request.getUid()
                << " Request's max-age is " << max_age << std::endl;
      double fresh_diff = checkFresh(cache_response, max_age, extra_age);
      if (fresh_diff > 3600) {
        time_t expire_time = getExpireTime(cache_response, max_age, extra_age);
        message =
            "in cache, but expired at " + std::string(displayTime(expire_time));
        if (checkControlField(cache_response, "must-revalidate") == true) {
          addValidateHeader(request, cache_response);
          return false;
        } else {
          if (fresh_diff < stale_age) {
            return true;
          } else {
            addValidateHeader(request, cache_response);
            return false;
          }
        }

      } else {
        message = "in cache, valid";
        return true;
      }
    }

    // Step.3 Expire in cache repsonse
    else if (checkExpireHeader(cache_response) == true) {
      std::string expire_header = "Expires";
      time_t expire_time_response =
          getUTCTime(cache_response.getValue(expire_header));
      time_t expire_time_request =
          getExpireTime(cache_response, max_age, extra_age);
      time_t expire_time = std::min(expire_time_response, expire_time_request);
      time_t request_time = time(0);
      double fresh_diff = difftime(request_time, expire_time);
      if (fresh_diff > 3600) {
        message =
            "in cache, but expired at " + std::string(displayTime(expire_time));
        addValidateHeader(request, cache_response);
        return false;
      } else {
        message = "in cache, valid";
        return true;
      }
    } else {
      double fresh_diff = checkFresh(cache_response, max_age, extra_age);
      if (max_age == 0 || fresh_diff > 3600) {
        time_t expire_time = getExpireTime(cache_response, max_age, extra_age);
        message =
            "in cache, but expired at " + std::string(displayTime(expire_time));
        return false;
      } else {
        message = "in cache, valid";
        return true;
      }
    }
  } else {
    message = "not in cache";

    /*std::string request_header = "";
    request.reconstructHeader(request_header);
    std::cout << "[CACHES] Uid " << request.getUid() << " Cache before remove "
              << request_header << std::endl;
    */
    removeValidateHeader(request);
    /*request_header = "";
    request.reconstructHeader(request_header);

    std::cout << "[CACHES] Uid " << request.getUid() << " Cache after remove "
              << request_header << std::endl;
    */
    return false;
  }
  return false;
}

void Cache::update(Request &request, Response &response, std::string &message) {
  int status_num = std::stoi(response.getStatusNum());
  if (status_num == 304) {
    std::string host_name = request.getUrl();
    std::cout << "[CACHES] Validate After Request URL " << request.getUrl()
              << std::endl;
    std::cout << "[CACHES] Validate After Request UID " << request.getUid()
              << std::endl;
    std::cout << "[DEBUG] Receive 304 Cache Exists" << findCache(host_name)
              << std::endl;
    Response response_return = getCache(host_name);
    response = response_return;
  } else {
    std::cout << "[LOG] Response for Request UID" << request.getUid()
              << std::endl;
    std::string response_entity = "";
    response.reconstructHeader(response_entity);
    std::cout << response_entity << std::endl;
    if (checkControlHeader(request) == true) {
      if (checkControlField(request, "no-store") == true) {
        message = "not cacheable because request contains \"no-store\" field";
        return;
      }
    }

    if (checkControlHeader(response) == true) {
      if (checkControlField(response, "no-store") == true) {
        message = "not cacheable because response contains \"no-store\" field";
        return;
      }
      if (checkControlField(response, "no-cache") == true) {
        message = "cached, but requires validation";
        replaceCache(request, response);
        return;
      }
      if (checkControlField(request, "max-age") == true) {
        int max_age = std::stoi(request.getCacheControlValue("max-age"));
        if (max_age == 0) {
          message = "cached, but requires validation";
          replaceCache(request, response);
          return;
        } else {
          time_t expire_time = getExpireTime(response, max_age, 0);
          message =
              "cached, expires at " + std::string(displayTime(expire_time));
          replaceCache(request, response);
          return;
        }
      }
    } else if (checkExpireHeader(response) == true) {
      std::string expire_header = "expires";
      std::string expire_string = response.getValue(expire_header);
      message = "cached, expires at " + expire_string;
      replaceCache(request, response);
      return;
    }
    message = "cache stores successfully";
    replaceCache(request, response);
    return;
  }
}

bool Cache::findCache(std::string &url) {
  auto iter = caches.find(url);
  return iter == caches.end() ? false : true;

  if (iter == caches.end()) {
    return false;
  } else {
    return true;
  }
}

Response Cache::getCache(std::string &url) {
  std::lock_guard<std::mutex> lck(mtx_cache);
  std::unordered_map<std::string, Response>::iterator iter = caches.find(url);
  return iter->second;
}

void Cache::getCache(std::string &url, Response &cache_response) {
  std::lock_guard<std::mutex> lck(mtx_cache);
  std::unordered_map<std::string, Response>::iterator iter = caches.find(url);
  cache_response = iter->second;
}

void Cache::replaceCache(Request &request, Response &response) {
  std::lock_guard<std::mutex> lck(mtx_cache);
  std::string host_name = request.getUrl();
  if (lru_queue.size() > CACHESIZE)
    evictCache();
  caches[host_name] = response;
  refreshLRU(host_name);
  std::cout << "[CACHES] Replacing Caches for Request UID: " << request.getUid()
            << std::endl;
  std::cout << "[CACHES] After replacing Caches: " << std::endl;
  std::string temp;
  caches[host_name].reconstructHeader(temp);
  std::cout << temp << std::endl;
}

bool Cache::checkControlHeader(Http &http) {
  return http.checkExistsHeader("Cache-Control");
}

bool Cache::checkControlField(Http &http, const char *header) {
  std::string header_str = header;
  return http.checkExistsControlHeader(header_str);
}

bool Cache::checkReValidateField(Http &http) {
  std::string header = "must-revalidate";
  return http.checkExistsControlHeader(header);
}

bool Cache::checkExpireHeader(Response &response) {
  return response.checkExistsHeader("Expires");
}

bool Cache::checkPragmaHeader(Http &http) {
  return http.checkExistsHeader("Pragma");
}

time_t Cache::getExpireTime(Response &cache_response, int max_age,
                            int extra_age) {
  std::string date_header = "Date";
  time_t cache_time = getUTCTime(cache_response.getValue(date_header));
  time_t expire_time = addTime(cache_time, max_age - extra_age);
  return expire_time;
}

double Cache::checkFresh(Response &cache_response, int max_age, int extra_age) {
  std::string date_header = "Date";
  time_t request_time = time(0);
  tm *request_time_gmt = gmtime(&request_time);
  request_time = mktime(request_time_gmt);
  time_t cache_time = getUTCTime(cache_response.getValue(date_header));

  time_t expire_time = addTime(cache_time, max_age - extra_age);
  std::cout << "[CACHES] Cache Request Time is " << displayTime(request_time)
            << std::endl;
  std::cout << "[CACHES] Cache Expire Time is " << displayTime(cache_time)
            << std::endl;
  std::cout << "[CACHES] Cache Real Date is "
            << cache_response.getValue(date_header)
            << " and max_age = " << max_age << std::endl;
  double tinterval = difftime(request_time, expire_time);
  return tinterval;
}

void Cache::eraseAllSubStr(std::string &mainStr, const std::string &toErase) {
  size_t pos = std::string::npos;

  // Search for the substring in string in a loop untill nothing is found
  while ((pos = mainStr.find(toErase)) != std::string::npos) {
    // If found then erase it from string
    mainStr.erase(pos, toErase.length());
  }
}
time_t Cache::getUTCTime(std::string time_k) {
  std::lock_guard<std::mutex> lck(mtx_cache);
  tm tm;
  eraseAllSubStr(time_k, " GMT");
  std::cout << "[CACHES] Cache Time is " << time_k << std::endl;
  strptime(time_k.c_str(), "%a, %d %b %Y %H:%M:%S", &tm);
  time_t t = mktime(&tm);
  std::cout << "[CACHES] Cache Time is " << displayTime(t) << std::endl;

  return t;
}

time_t Cache::addTime(time_t time, int value) { return time + value; }

void Cache::addValidateHeader(Request &request, Response &cache_response) {

  if (cache_response.checkExistsHeader("Etag")) {
    std::string header = "If-None-Match";
    std::string etag = cache_response.getValue(header);
    request.addHeaderPair(header, etag);
  }
  if (cache_response.checkExistsHeader("Last-Modified")) {
    std::string header = "If-Modified-Since";
    std::string modified = cache_response.getValue(header);
    request.addHeaderPair(header, modified);
  }
  return;
}

void Cache::removeValidateHeader(Request &request) {
  request.removeHeaderPair("If-None-Match");
  request.removeHeaderPair("If-Modified-Since");
  return;
}

char *Cache::displayTime(time_t time) {
  tm *gmtm = localtime(&time);
  char *dt = asctime(gmtm);
  return dt;
}

void Cache::evictCache() {
  std::string last = lru_queue.back();
  std::cout << "[LRU] EVICT CACHE URL : " << last << std::endl;
  lru_queue.pop_back();
  caches.erase(last);
}
void Cache::refreshLRU(std::string url) {
  std::cout << "[LRU] CACHE SIZE IS : " << lru_queue.size() << std::endl;
  std::cout << "[LRU] REFRESH : " << url << std::endl;
  if (findCache(url)) {
    lru_queue.remove(url);
    lru_queue.push_front(url);
  } else {
    lru_queue.push_front(url);
  }
}
