#include "io_transactor.h"

#include <cassert>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace std;
using namespace dj;

io_transactor_t::~io_transactor_t() {
  assert(this);
  assert(!bg_thread.joinable());
  close(fds[0]);
  close(fds[1]);
}

bool io_transactor_t::has_exited() {
  assert(this);
  assert(bg_thread.joinable());
  unique_lock<std::mutex> lock(mutex);
  if (ex_ptr) {
    rethrow_exception(ex_ptr);
  }
  return exited;
}

void io_transactor_t::wait_until_exited() {
  assert(this);
  assert(bg_thread.joinable());
  unique_lock<std::mutex> lock(mutex);
  while (!exited) {
    exited_set.wait(lock);
  }
  if (ex_ptr) {
    rethrow_exception(ex_ptr);
  }
}

io_transactor_t::io_transactor_t() {
  if (pipe(fds) < 0) {
    throw runtime_error("pipe");
  }
}

future_t io_transactor_t::send(json_t &&request) {
  assert(this);
  assert(&request);
  auto future = promise_keeper.make_promise();
  cout << json_t(json_t::object_t({
    { "op", "request" },
    { "id", future->get_id() },
    { "body", move(request) }
  }));
  return future;
}

void io_transactor_t::start() {
  assert(this);
  stop();
  exited = false;
  ex_ptr = exception_ptr();
  bg_thread = thread(&io_transactor_t::bg_main, this);
}

void io_transactor_t::stop() {
  assert(this);
  if (bg_thread.joinable()) {
    write(fds[1], "x", 1);
    bg_thread.join();
  }
}

void io_transactor_t::bg_main() {
  assert(this);
  try {
    pollfd polls[2];
    polls[0].fd = fds[0];
    polls[0].events = POLLIN;
    polls[1].fd = 0;
    polls[1].events = POLLIN;
    bool go = true;
    do {
      if (poll(polls, 2, -1) < 0) {
        throw runtime_error("poll");
      }
      if (polls[0].revents & POLLIN) {
        break;
      }
      if (polls[1].revents & POLLIN) {
        json_t msg;
        cin >> msg;
        op_t op;
        int id;
        json_t body;
        if (try_parse_msg(msg, op, id, body)) {
          switch (op) {
            case op_t::request: {
              auto reply = on_request(body);
              cout << json_t(json_t::object_t({
                { "op", "response" },
                { "id", id },
                { "body", move(reply) }
              }));
              break;
            }
            case op_t::response: {
              promise_keeper.keep_promise(id, move(body));
              break;
            }
            case op_t::stop: {
              go = false;
              break;
            }
          }
        } else {
          cerr << "bad msg " << msg << endl;
        }
      }
    } while (go);
    unique_lock<std::mutex> lock(mutex);
    exited = true;
    exited_set.notify_one();
  } catch (...) {
    unique_lock<std::mutex> lock(mutex);
    ex_ptr = current_exception();
    exited = true;
    exited_set.notify_one();
  }
}

bool io_transactor_t::try_parse_msg(
    json_t &msg, op_t &op, int &id, json_t &body) {
  assert(&msg);
  assert(&op);
  assert(&id);
  assert(&body);
  static const map<string, op_t> ops = {
    { "request", op_t::request },
    { "response", op_t::response },
    { "stop", op_t::stop }
  };
  const auto *elem = msg.try_get_elem("op");
  if (!elem) {
    return false;
  }
  const auto *str = elem->try_get_state<json_t::string_t>();
  if (!str) {
    return false;
  }
  auto iter = ops.find(*str);
  if (iter == ops.end()) {
    return false;
  }
  op = iter->second;
  if (op == op_t::stop) {
    return true;
  }
  elem = msg.try_get_elem("id");
  if (!elem) {
    return false;
  }
  const auto *num = elem->try_get_state<json_t::number_t>();
  if (!num) {
    return false;
  }
  id = *num;
  elem = msg.try_get_elem("body");
  if (!elem) {
    return false;
  }
  body = move(*elem);
  return true;
}
