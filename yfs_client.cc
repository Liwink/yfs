// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unordered_map>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);

}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::readfile(inum ino, std::string &buf)
{
  if (ec->get(ino, buf) != extent_protocol::OK) {
      return IOERR;
  }
  return OK;
}

int
yfs_client::writefile(inum ino, std::string &buf)
{
  if (ec->put(ino, buf) != extent_protocol::OK) {
      return IOERR;
  }
  return OK;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;

//  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
//  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

 release:

  return r;
}

void
parsedir(const std::string &dir, std::unordered_map<std::string, unsigned long> &files)
{
  std::size_t start = 1;
  std::size_t end;
  while ((end = dir.find('>', start)) != std::string::npos) {
    auto tmp = dir.substr(start, end -  start);
    auto pos = tmp.find('|');
    files[tmp.substr(0, pos)] = std::stol(tmp.substr(pos + 1, tmp.size() - pos - 1));
    start = end + 2;
  }
}

unsigned long
dircontainfile(const std::string &dir, const char *filename)
{
  std::string key(filename);
  std::size_t pos = dir.find("<" + key + "|");
  if (pos == std::string::npos) {
    return 0;
  }
  pos += 2 + key.size();
  int len = 1;
  while(dir[pos + len] != '>') {
    len += 1;
  }

  return static_cast<unsigned long>(std::stol(dir.substr(pos, len)));
}

void
yfs_client::readdir(inum parent, std::unordered_map<std::string, unsigned long> &files)
{
  std::string buf;
  if (ec->get(parent, buf) != extent_protocol::OK) {
    return;
  }

  parsedir(buf, files);
}

void
diraddfile(std::string &dir, const char *filename, yfs_client::inum ino)
{
  dir.append("<");
  dir.append(filename);
  dir.append("|");
  dir.append(std::to_string(ino));
  dir.append(">");
}

unsigned long
yfs_client::lookup(inum parentnum, const char *filename)
{
  std::string buf;
  if (ec->get(parentnum, buf) != extent_protocol::OK) {
    return 0;
  }

  return dircontainfile(buf, filename);
}

int
yfs_client::createfile(inum parentnum, const char *filename, unsigned long &filenum)
{
  std::cout << "createfile: " << parentnum << ", " << filename << std::endl;

  std::string buf;
  if (ec->get(parentnum, buf) != extent_protocol::OK) {
    if (parentnum == 1) {
      buf = "";
    } else {
      return IOERR;
    }
  }

  if (dircontainfile(buf, filename) != 0) {
    return EXIST;
  }

  // generate a inum for new file
  inum ino = static_cast<inum>(random());
  ino |= 0x80000000;

  filenum = static_cast<unsigned long>(ino);

  diraddfile(buf, filename, ino);

  ec->put(parentnum, buf);
  ec->put(ino, "");

  return OK;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;

//  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

 release:
  return r;
}



