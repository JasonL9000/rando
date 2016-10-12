#pragma once

#include "danny.h"

#include <cerrno>
#include <new>

#include <unistd.h>

namespace danny {

inline fd_t::fd_t() noexcept
    : handle(closed_handle) {}

inline fd_t::fd_t(fd_t &&that) noexcept
    : handle(std::exchange(that.handle, -closed_handle)) {}

inline fd_t::~fd_t() {
  reset();
}

inline fd_t &fd_t::operator=(fd_t &&that) noexcept {
  if (this != &that) {
    this->~fd_t();
    new (this) fd_t(std::move(that));
  }
  return *this;
}

inline fd_t &fd_t::operator=(const fd_t &that) {
  return *this = fd_t { that };
}

inline fd_t::operator int() const {
  return get();
}

inline int fd_t::get() const {
  if (!is_open()) {
    throw_system_error(EBADF);
  }
  return handle;
}

inline bool fd_t::is_open() const noexcept {
  return handle >= 0;
}

inline int fd_t::release() {
  int result = get();
  reset();
  return result;
}

inline fd_t make_fd(int new_handle) {
  fd_t result;
  result.handle = throw_if_lt0(new_handle);
  return result;
}

///////////////////////////////////////////////////////////////////////////////

inline if_not_exists_t::if_not_exists_t() noexcept
    : create(false), mode(0) {}

inline if_not_exists_t::if_not_exists_t(mode_t new_mode) noexcept
    : create(true), mode(new_mode) {}

///////////////////////////////////////////////////////////////////////////////

inline size_t read_at_most(int fd, void *data, size_t size) {
  return static_cast<size_t>(throw_if_lt0(read(fd, data, size)));
}

inline size_t write_at_most(int fd, const void *data, size_t size) {
  return static_cast<size_t>(throw_if_lt0(write(fd, data, size)));
}

template <typename arg_t>
const arg_t &throw_if_lt0(const arg_t &arg) {
  if (arg < 0) {
    throw_system_error();
  }
  return arg;
}

[[noreturn]] inline void throw_system_error() {
  throw_system_error(errno);
}

}  // danny
