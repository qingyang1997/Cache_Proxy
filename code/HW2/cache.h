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

  bool validate(Request &request, Response &cache_response,
                std::string &message);
  void update(Request &request, Response &response, std::string &message);

private:
  bool checkControlHeader(Http &http);
  bool checkControlField(Http &http, const char *header);
  bool checkReValidateField(Http &http);
  bool findCache(std::string &url);
  bool checkExpireHeader(Response &response);
  bool checkPragmaHeader(Http &http);
  int checkFresh(Response &cache_response, int max_age);
  bool timeCompare(time_t t1, time_t t2);
  void getCache(std::string &url, Response &cache_response);
  void replaceCache(Request &request, Response &response);
  time_t getUTCTime(std::string time);
  time_t addTime(time_t time, int value);
  void eraseAllSubStr(std::string &mainStr, const std::string &toErase);
  void addValidateHeader(Request &request, Response &response);
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

  //  Find Cache
  std::string host_name = request.getHost();
  if (findCache(host_name) == true) {
    getCache(host_name, cache_response);

    int max_age = 0;
    int stale_age = 0;
    int extra_age = 0;

    //  Check Request Cache Control Headers
    if (checkControlHeader(request) == true) {
      if (checkControlField(request, "no-cache") == true) {
        message = "in cache, requires validation";
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
        return false;
      }
    }

    // Cache Control in cache repsonse
    if (checkControlHeader(cache_response) == true) {
      if (checkControlField(cache_response, "no-cache") == true) {
        message = "in cache, requires validation";
        return false;
      }
      // Check Freshness
      if (checkControlField(cache_response, "max-age") == true) {
        max_age = std::min(
            max_age, std::stoi(cache_response.getCacheControlValue("max-age")));
      }
      std::string date_header = "date";
      time_t request_time = time(0);

      time_t expire_time = addTime(request_time, max_age);
      time_t cache_time = getUTCTime(cache_response.getValue(date_header));
      if (timeCompare(expire_time, cache_time) == false) {
        message = "not fresh";
        return false;
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

        return false;
      }
    } else {
      // Maybe useful
    }

    message = "in cache, valid";

    return true;
  } else {
    message = "not in cache";
    return false;
  }
  return false;
}

void Cache::update(Request &request, Response &response, std::string &message) {
  int status_num = std::stoi(response.getStatusNum());
  if (status_num == 304) {
    std::string host_name = request.getHost();
    getCache(host_name, response);
  } else {
    if (checkControlHeader(request) == true) {
      if (checkNoCacheField(request) == true) {
        message = "not cache because request contains \"no-cache\" field";
        return;
      }
    }

    if (checkControlHeader(response) == true) {
      if (checkNoCacheField(response) == true) {
        message = "not cache because response contains \"no-cache\" field";
        return;
      } else if (checkReValidateField(response) == true) {
        message = "cached, but requires re-validation";
      }

    } else if (checkExpireHeader(response) == true) {
      std::string expire_header = "expires";
      std::string expire_string = response.getValue(expire_header);
      message = "cached, expires at " + expire_string;
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

time_t Cache::addTime(time_t time, int value) { return time + value; }

void addValidateHeader(Request &request, Response &response) {}
