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

class yfs_client {
  extent_client *ec;
  lock_client *lc;
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
  int unlink(inum parentnum, const char *filename);
};

#endif 
