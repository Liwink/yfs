// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  // printf("acquire %lld request from clt %d\n", lid, clt);

  std::unique_lock<std::mutex> ul(m);

  while (locked.find(lid) != locked.end()) {
    cond.wait(ul);
  }

  // printf("DONE: acquire %lld request from clt %d\n", lid, clt);
  locked.insert(lid);

  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  // printf("release %lld request from clt %d\n", lid, clt);

  std::unique_lock<std::mutex> ul(m);

  locked.erase(lid);

  cond.notify_all();

  // printf("DONE: release %lld request from clt %d\n", lid, clt);
  return ret;
}

