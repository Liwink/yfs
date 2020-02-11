// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache()
{
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &)
{
  // if the lock is free
  //    acquired and record the id
  // if the lock is lock
  //    RETRY
  //    revoke
  std::unique_lock<std::mutex> l(m);

  if (locked.find(lid) == locked.end()) {
    locked[lid] = id;
    waiting[lid].erase(id);
    return lock_protocol::OK;
  } else {
    // NOTE: here we do not wait
    waiting[lid].insert(id);
    // todo: to revoke
    if (clients.find(id) == clients.end()) {
      sockaddr_in csock;
      make_sockaddr(id.c_str(), &csock);
      clients[id] = std::make_unique<rpcc>(csock);
    }
    clients[id]->call(lock_protocol::revoke, lid);
    return lock_protocol::RETRY;
  }
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  std::unique_lock<std::mutex> l(m);

  if (locked.find(lid) == locked.end() || locked[lid] != id)
    return lock_protocol::OK;

  locked.erase(lid);
  if (!waiting[lid].empty()) {
    clients[*waiting[lid].begin()]->call(lock_protocol::retry, lid);
  }
  return lock_protocol::OK;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

