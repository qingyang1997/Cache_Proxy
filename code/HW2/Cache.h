#include "Request.h"
#include "Response.h"
#include <ctime>
#include <list>
#include <string>
#include <unordered_map>

typedef std::unordered_map<std::string, Response> cachemap;

class Cache {
public:
  Cache();

  int validate(Request &request, Response &cache_response,
               std::string &message);
  int update(Request &request, Response &response, std::string &message);

private:
  bool checkControlHeader(Http &http);
  bool checkNoCacheField(Http &http);

  bool checkReValidateField(Http &http);
  bool checkMaxAgeField(Http &http);
  bool findCache(std::string &url);
  bool checkExpireHeader(Response &response);
  bool checkFresh(const Request &request, const Response &response);
  bool timeCompare(time_t t1, time_t t2);
  void getCache(std::string &url, Response &cache_response);
  void replaceCache(Request &request, Response &response);
  time_t getUTCTime(std::string time);
  time_t addTime(time_t time, std::string value);
  void eraseAllSubStr(std::string &mainStr, const std::string &toErase);

  void writeLog(const char *info);
  cachemap caches;
};

Cache::Cache() {}

/*
 Cache Validate
if (cache_response.receive_time + cache_response.max_age > current_time
 */

int Cache::validate(Request &request, Response &cache_response,
                    std::string &message) {

  int signal = 0;
  /* ----Request Level----- */
  // Step.1 Check Request Cache Control Headers
  if (checkControlHeader(request) == true) {
    // Maybe Useful
    // Step 1.1 no-cache header
    if (checkNoCacheField(request) == true) {
      message = "Request no-cache";
      signal = 1;
      return signal;
    }
  }

  // Step.2 Find Cache
  std::string host_name = request.getHost();
  if (findCache(host_name) == true) {
    getCache(host_name, cache_response);

    /* ----Cache Level----- */
    // Step.3 Cache Control in cache repsonse
    if (checkControlHeader(cache_response) == true) {
      if (checkNoCacheField(cache_response) == true) {
        message = "in cache, no-cache";
        signal = 2;
        return signal;
      }
      // Step.4 Check Freshness
      if (checkMaxAgeField(cache_response) == true) {
        std::string date_header = "date";
        time_t request_time = getUTCTime(request.getValue(date_header));
        std::string max_age = cache_response.getCacheControlValue("max-age");
        time_t expire_time = addTime(request_time, max_age);
        time_t cache_time = getUTCTime(cache_response.getValue(date_header));
        if (timeCompare(expire_time, cache_time) == false) {
          message = "not fresh";
          signal = 3;
          return signal;
        }
      }

    }
    // Step.3 Expire in cache repsonse
    else if (checkExpireHeader(cache_response) == true) {
      std::string expire_header = "expires";
      time_t expire_time = getUTCTime(cache_response.getValue(expire_header));
      std::string date_header = "date";
      time_t request_time = getUTCTime(request.getValue(date_header));
      if (timeCompare(request_time, expire_time) == true) {
        message = "expire";
        signal = 4;
        return signal;
      }
    } else {
      // Maybe useful
    }

    message = "in cache, valid";
    signal = 0;
    return signal;

  } else {
    message = "not in cache";
    signal = 5;
    return signal;
  }
  return signal;
}

int Cache::update(Request &request, Response &response, std::string &message) {
  int signal = 0;
  if (checkControlHeader(request) == true) {
    if (checkNoCacheField(request) == true) {
      message = "not cache because request contains \"no-cache\" field";
      signal = 1;
      return signal;
    }
  }

  if (checkControlHeader(response) == true) {
    if (checkNoCacheField(response) == true) {
      message = "not cache because response contains \"no-cache\" field";
      signal = 2;
      return signal;
    } else if (checkReValidateField(response) == true) {
      message = "cached, but requires re-validation";
      signal = 4;
    }

  } else if (checkExpireHeader(response) == true) {
    std::string expire_header = "expires";
    std::string expire_string = response.getValue(expire_header);
    message = "cached, expires at " + expire_string;
    signal = 3;
  }

  replaceCache(request, response);
  return signal;
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

void Cache::getCache(std::string &url, Response &cache_response) {
  auto iter = caches.find(url);
  cache_response = iter->second;
}

void Cache::replaceCache(Request &request, Response &response) {
  std::string host_name = request.getHost();
  caches[host_name] = response;
}

bool Cache::checkControlHeader(Http &http) {
  return http.checkExistsHeader("Cache-Control");
}

bool Cache::checkNoCacheField(Http &http) {
  std::string header = "no-cache";
  return http.checkExistsControlHeader(header);
}

bool Cache::checkReValidateField(Http &http) {
  std::string header = "must-revalidate";
  return http.checkExistsControlHeader(header);
}

bool Cache::checkMaxAgeField(Http &http) {
  std::string header = "max-age";
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
