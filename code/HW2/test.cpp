#include "Response.h"
#include <string>
using namespace std;

int main() {
  Response a;
  Response b;
  std::string key = "Host";
  std::string value = "google.com";
  b.addHeaderPair(key, value);
  b.setProtocol(value);
  a = b;
  return 0;
}
