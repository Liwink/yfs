#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <unordered_set>
#include <unordered_map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache {
 private:
  int nacquire;
  std::unordered_map<lock_protocol::lockid_t, std::string> locked;
  std::mutex m;
  std::condition_variable cond;
  std::unordered_map<lock_protocol::lockid_t, std::unordered_set<std::string> > waiting;
  std::unordered_map<std::string, std::unique_ptr<rpcc> > clients;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
