// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <ctime>
#include <mutex>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() {}

unsigned int time() {
  return static_cast<unsigned  int>(time(NULL));
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.
  std::unique_lock<std::mutex> lock(m);

  store[id] = buf;
  attr_store[id] = extent_protocol::attr {
    time(),
    time(),
    time(),
    0
  };
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // You fill this in for Lab 2.
  std::unique_lock<std::mutex> lock(m);

  if (store.find(id) == store.end()) {
    return extent_protocol::NOENT;
  }
  buf = store[id];
  attr_store[id].atime = time();
  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  // a.size = 0;
  // a.atime = 0;
  // a.mtime = 0;
  // a.ctime = 0;
  std::unique_lock<std::mutex> lock(m);

  a = attr_store[id];
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  std::unique_lock<std::mutex> lock(m);

  store.erase(id);
  attr_store.erase(id);
  return extent_protocol::OK;
}

