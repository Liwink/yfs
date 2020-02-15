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

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.
  store[id] = buf;

  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // You fill this in for Lab 2.
  if (store.find(id) == store.end()) {
    return extent_protocol::NOENT;
  }
  buf = store[id];

//  std::cout << "get: " << id << ", " << buf << std::endl;
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
  a = attr_store[id];
  return extent_protocol::OK;
}

int extent_server::putattr(extent_protocol::extentid_t id, extent_protocol::attr a, int &)
{
  attr_store[id] = a;

  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  store.erase(id);
  attr_store.erase(id);
  return extent_protocol::OK;
}

