#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <unordered_set>
#include <unordered_map>
#include <thread>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

struct pair_hash {
    template <class T1, class T2>
    std::size_t operator () (const std::pair<T1,T2> &p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);

        // Mainly for demonstration purposes, i.e. works but is overly simple
        // In the real world, use sth. like boost.hash_combine
        return h1 ^ h2;
    }
};

class lock_server_cache {
 private:
  int nacquire;
  std::unordered_map<lock_protocol::lockid_t, std::string> locked;
  std::mutex m;
  std::condition_variable cond;
  std::unordered_map<lock_protocol::lockid_t, std::unordered_set<std::string> > waiting;
  std::unordered_map<std::string, std::shared_ptr<rpcc> > clients;

  bool is_close;
  std::unordered_set<std::pair<std::string, lock_protocol::lockid_t>, pair_hash> to_revoke;
  std::unordered_set<std::pair<std::string, lock_protocol::lockid_t>, pair_hash> to_retry;
  std::thread *revoke_worker;
  std::thread *retry_worker;

  void init_client(std::string id);
  void revoke_background();
  void retry_background();
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
