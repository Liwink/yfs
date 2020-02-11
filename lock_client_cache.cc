// RPC stubs for clients to talk to lock_server, and cache the available
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  rpcs *rlsrpc = new rpcs(0);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlsrpc->port();
  id = host.str();
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  // mutex.lock
  // while
  // if the lock is hold by the cache
  //    wait for other thread to release
  // else if the lock is not hold
  //    call server to acquire
  //    setup local lock state
  //
  // update local state

  // todo: provide local port to server
  // todo: fine-grained lock and cond
  std::unique_lock<std::mutex> l(m);
  while (true) {
    if (available.find(lid) != available.end()) {
        if (available[lid]) {
          break;
        }
        waiting[lid]++;
        cond.wait(l);
        waiting[lid]--;
    } else {
      auto ret = cl->call(lock_protocol::acquire, lid, id);
      if (ret == lock_protocol::OK) {
        available[lid] = false;
      } else if (ret == lock_protocol::RETRY){
        cond.wait(l);
      } else {
        return ret;
      }
    }
  }
  available[lid] = false;
  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::_release(lock_protocol::lockid_t lid)
{
  auto ret = cl->call(lock_protocol::release, lid, id);
  if (ret != lock_protocol::OK) return ret;
  available.erase(lid);
  revoke.erase(lid);
  return ret;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  // 1. update the local lock state
  // 2. if other threads are waiting -> notify
  // 3. else if revoked, release
  std::unique_lock<std::mutex> l(m);
  available[lid] = true;
  if (waiting[lid] > 0) {
    cond.notify_all();
    return lock_protocol::OK;
  }
  if (revoke.find(lid) != revoke.end()) {
    return _release(lid);
  }
  return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  std::unique_lock<std::mutex> l(m);
  if (available.find(lid) != available.end()) {
    if (waiting[lid] == 0 && available[lid]) {
      // fixme: potential deadlock
      return _release(lid);
    }
    revoke.insert(lid);
  }
  int ret = rlock_protocol::OK;
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  std::unique_lock<std::mutex> l(m);
  cond.notify_all();
  int ret = rlock_protocol::OK;
  return ret;
}



