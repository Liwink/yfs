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
  // if r == 1, to release lid immediately
  int r;
  tprintf("client acquire: %s\n", id.c_str());
  while (true) {
    if (available.find(lid) != available.end()) {
        if (available[lid]) {
          break;
        }
        waiting[lid]++;
        cond.wait(l);
        waiting[lid]--;
    } else {
      auto ret = cl->call(lock_protocol::acquire, lid, id, r);

      if (ret == lock_protocol::OK) {
        available[lid] = true;
      } else if (ret == lock_protocol::RETRY){
        while (retry.find(lid) == retry.end()) {
          tprintf("wait %s\n", id.c_str());
          cond.wait(l);
        }
        retry.erase(lid);
      } else {
        return ret;
      }
    }
  }
  available[lid] = false;
  if (r == 1)
    revoke.insert(lid);
  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::_release(lock_protocol::lockid_t lid)
{
  // fixme: potential deadlock
  // fixme: order
  int r;
  // upload cache before release lock
  lu->dorelease(lid);
  auto ret = cl->call(lock_protocol::release, lid, id, r);
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
  tprintf("client release: %s\n", id.c_str());
  available[lid] = true;
  if (waiting[lid] > 0) {
    cond.notify_all();
    return lock_protocol::OK;
  }
  if (revoke.find(lid) != revoke.end()) {
    tprintf("release to server\n");
    return _release(lid);
  }
  return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  std::unique_lock<std::mutex> l(m);
  tprintf("revoke %s\n", id.c_str());
  if (available.find(lid) != available.end()) {
    if (waiting[lid] == 0 && available[lid]) {
      return _release(lid);
    }
  }
  revoke.insert(lid);
  int ret = rlock_protocol::OK;
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  tprintf("pre retry %s\n", id.c_str());
  std::unique_lock<std::mutex> l(m);
  // XXX: How do you handle a retry showing up on the client
  // before the response on the corresponding acquire?
  tprintf("retry %s\n", id.c_str());

  retry.insert(lid);
  cond.notify_all();
  int ret = rlock_protocol::OK;
  return ret;
}



