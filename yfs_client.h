#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include "lock_client.h"
#include <vector>
#include <unordered_map>

#include "lock_protocol.h"
#include "lock_client.h"
#include "lock_client_cache.h"


class file_cache;

class yfs_client {
  extent_client *ec;
  lock_client *lc;
  lock_release_user *lu;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);

  std::unordered_map<lock_protocol::lockid_t, std::unique_ptr<file_cache> > files;

 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  int createfile(inum, const char*, unsigned long&, bool);
  unsigned long lookup(inum parentnum, const char *filename);
  void readdir(inum parent, std::unordered_map<std::string, unsigned long> &files);
  int readexpendfile(inum ino, std::string &buf, size_t size);
  int readfile(inum ino, std::string &buf);
  int writefile(inum ino, std::string &buf);
  int writefile(inum ino, const char *buf, size_t size, off_t off);
  int unlink(inum parentnum, const char *filename);

  void flush(lock_protocol::lockid_t lid);

  file_cache* _cache_file(inum ino);
  file_cache* _new_cache_file(inum ino);
};

// todo: remove the friendship
class file_cache {
public:
    enum Status {LOCKED, UNLOCKED};
    file_cache(lock_protocol::lockid_t lid)
      : lid(lid), _status(Status::UNLOCKED), _buf(std::string()), _dirty(false), _deleted(false) {}
    bool is_dirty() {return _dirty;};
    bool is_locked() {return _status == Status::LOCKED;};
    bool is_deleted() {return _deleted;};
    void lock() {_status = Status::LOCKED;};
    void dirty() {_dirty = true;};
    void put(std::string buf);
    void get(std::string &buf);
    void remove() {_buf = ""; _deleted=true;};
    extent_protocol::attr getattr() {return _attr;};

private:
    lock_protocol::lockid_t lid;
    Status _status;
    std::string _buf;
    extent_protocol::attr _attr;
    bool _dirty;
    bool _deleted;

    friend yfs_client;

};

class lock_release_yfs : public lock_release_user {
public:
    lock_release_yfs(yfs_client* yfs) : _yfs(yfs) {};
    void dorelease(lock_protocol::lockid_t lid) {
        _yfs->flush(lid);
    }

private:
    yfs_client* _yfs;

};

#endif 
