// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <thread>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache() : is_close(false)
{
  // todo: to quit background threads when the obj is destructed
  worker_revoke = new std::thread (&lock_server_cache::revoke_background, this);
  worker_retry = new std::thread (&lock_server_cache::retry_background, this);
}

void lock_server_cache::init_client(std::string id)
{
  if (clients.find(id) == clients.end()) {
    sockaddr_in csock;
    make_sockaddr(id.c_str(), &csock);
    clients[id] = std::make_shared<rpcc>(csock);
    if (clients[id]->bind() < 0) {
      printf("lock_server: call bind %s\n", id.c_str());
    }
  }
}

void lock_server_cache::revoke_background()
{
  std::unique_lock<std::mutex> l(m);
  printf("revoke_background running\n");
  int r;
  while (true) {
    if (is_close) return;
    while (to_revoke.empty()) {
      cond_revoke.wait(l);
    }
    auto item = to_revoke.begin();
    init_client(item->first);
    auto cid = item->first;
    auto lid = item->second;
    to_revoke.erase(item);

    printf("revoke %lld to clt %s\n", lid, cid.c_str());
    l.mutex()->unlock();
    clients[cid]->call(rlock_protocol::revoke, lid, r);
    l.mutex()->lock();
  }
}

void lock_server_cache::retry_background()
{
  std::unique_lock<std::mutex> l(m);
  printf("retry_background running\n");
  int r;
  while (true) {
    if (is_close) return;
    while (to_retry.empty()) {
      cond_retry.wait(l);
    }
    auto item = to_retry.begin();
    init_client(item->first);
    auto cid = item->first;
    auto lid = item->second;
    to_retry.erase(item);

    printf("retry %lld to clt %s\n", lid, cid.c_str());
    l.mutex()->unlock();
    clients[cid]->call(rlock_protocol::retry, lid, r);
    l.mutex()->lock();
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
    printf("acquire done\n");
    return lock_protocol::OK;
  } else {
    // NOTE: here we do not wait
    waiting[lid].insert(id);
    to_revoke.insert(std::make_pair(locked[lid], lid));
    cond_revoke.notify_all();
    printf("RETRY done\n");
    return lock_protocol::RETRY;
  }
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id,
         int &r)
{
  std::unique_lock<std::mutex> l(m);
  printf("release %lld request from clt %s\n", lid, id.c_str());

//  if (locked.find(lid) == locked.end() || locked[lid] != id)
//    return lock_protocol::OK;

  printf("size: %d\n", waiting[lid].size());
  locked.erase(lid);
  if (!waiting[lid].empty()) {
    to_retry.insert(std::make_pair(*waiting[lid].begin(), lid));
    cond_retry.notify_all();
  }
  printf("release done\n");
  return lock_protocol::OK;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

