#include "Response.h"

int main() {
  std::ofstream log;
  log.open("log.txt", std::ofstream::out);
  std::string m = "hello\n";
  log << m;
  log.close();
  return 0;
}
