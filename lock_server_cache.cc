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

void lock_server_cache::init_client(std::string id)
{
  if (clients.find(id) == clients.end()) {
    sockaddr_in csock;
    make_sockaddr(id.c_str(), &csock);
    clients[id] = std::make_unique<rpcc>(csock);
    if (clients[id]->bind() < 0) {
      printf("lock_server: call bind %s\n", id.c_str());
    }
  }
}

int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id,
                               int &r)
{
  // if the lock is free
  //    acquired and record the id
  // if the lock is lock
  //    RETRY
  //    revoke
  std::unique_lock<std::mutex> l(m);
  printf("acquire %lld request from clt %s\n", lid, id.c_str());

  if (locked.find(lid) == locked.end()) {
    locked[lid] = id;
    waiting[lid].erase(id);
    // to invoke
    r = 1;
    return lock_protocol::OK;
  } else {
    // NOTE: here we do not wait
    int r;
    waiting[lid].insert(id);
    init_client(locked[lid]);
    // todo: client!
    printf("revoke %lld to clt %s\n", lid, locked[lid].c_str());
    clients[locked[lid]]->call(rlock_protocol::revoke, lid, r);
    return lock_protocol::RETRY;
  }
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id,
         int &r)
{
  std::unique_lock<std::mutex> l(m);
  printf("release %lld request from clt %s\n", lid, id.c_str());

  if (locked.find(lid) == locked.end() || locked[lid] != id)
    return lock_protocol::OK;

  printf("size: %d\n", waiting[lid].size());
  locked.erase(lid);
  if (!waiting[lid].empty()) {
    int r;
    printf("retry %lld to clt %s\n", lid, waiting[lid].begin()->c_str());
    auto wid = *waiting[lid].begin();
    init_client(wid);
    clients[wid]->call(rlock_protocol::retry, lid, r);
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

