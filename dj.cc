// g++ -std=c++11 -odj io_transactor.cc json.cc dj.cc -Wl,--no-as-needed -lpthread

#include <iostream>

#include "io_transactor.h"

using namespace std;
using namespace dj;

class foo_t final
    : public io_transactor_t {
  public:

  foo_t() {
    start();
  }

  virtual ~foo_t() {
    stop();
  }

  private:

  virtual json_t on_request(json_t &request) override {
    return request;
  }

};

int main(int, char *[]) {
  foo_t foo;
  for (;;) {
    if (foo.has_exited()) {
      break;
    }
  }
}
