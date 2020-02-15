// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client.h"
#include "lock_client_cache.h"
#include <ctime>
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unordered_map>

unsigned int local_time() {
  return static_cast<unsigned int>(time(NULL));
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

unsigned long
dirunlinkfile(std::string &dir, const char *filename)
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

  auto ino = static_cast<unsigned long>(std::stol(dir.substr(pos, len)));

  dir.erase(pos - key.size() - 2, key.size() + len + 3);

  return ino;
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

// ------------------- file_cache -------------------
void
file_cache::put(std::string buf)
{
  _buf = std::move(buf);
  _dirty = true;
  _attr.atime = time(NULL);
  _attr.ctime = local_time();
  _attr.mtime = local_time();
  _attr.size = _buf.size();
}

void
file_cache::get(std::string &buf) {
  buf = _buf;
  _attr.atime = local_time();
};

// ------------------- yfs_client -------------------

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  lu = new lock_release_yfs(this);
  lc = new lock_client_cache(lock_dst, lu);
//  lc = new lock_client(lock_dst);
  srandom(getpid());
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

file_cache *
yfs_client::_new_cache_file(inum ino)
{
  // XXX: caller should hold the lock of ino
  files[ino] = std::make_unique<file_cache>(ino);
  file_cache *f = files[ino].get();
  f->put("");
  f->lock();
  return f;
}

file_cache *
yfs_client::_cache_file(inum ino)
{
  // XXX: caller should hold the lock of ino

  // todo: does it handle a deleted file correctly?
  if (files.find(ino) == files.end()) {
    files[ino] = std::make_unique<file_cache>(ino);
  }
  file_cache *f = files[ino].get();

  // todo: move this init function to file_cache class
  if (!f->is_locked()) {
    while (ec->get(ino, f->_buf) != extent_protocol::OK) {
      if (ino == 1) break;
    }
    while (ec->getattr(ino, f->_attr) != extent_protocol::OK) {
      if (ino == 1) break;
    }
    f->lock();
  }
  return f;
}

int
yfs_client::readexpendfile(inum ino, std::string &buf, size_t size)
{
  // it's for seek out-of-range
  auto l = unique_lock_client(lc, ino);
  auto f = _cache_file(ino);

  f->get(buf);

  // FIXME: HACK: Sooooo wield! It's for out-of-range seek on MacOS
  if (size > buf.size()) {
    buf.append(std::string(size - buf.size(), '\0'));
    f->put(buf);
  }
  return OK;
}

int
yfs_client::readfile(inum ino, std::string &buf)
{
  auto l = unique_lock_client(lc, ino);
  auto f = _cache_file(ino);

  f->get(buf);
  return OK;
}

int
yfs_client::writefile(inum ino, const char *buf, size_t size, off_t off)
{
  auto l = unique_lock_client(lc, ino);
  auto f = _cache_file(ino);

  std::string doc;
  f->get(doc);
  std::cout << "old doc: " << doc << std::endl;
  if (off >= doc.size()) {
    doc.append(std::string(doc.size() - off + 1, '\0'));
  }
  doc.replace(doc.begin() + off, doc.begin() + off + size, buf, size);
  std::cout << "new doc: " << doc << std::endl;

  f->put(doc);
  return OK;
}

int
yfs_client::writefile(inum ino, std::string &buf)
{
  auto l = unique_lock_client(lc, ino);
  auto f = _cache_file(ino);

  f->put(buf);
  return OK;
}

int
yfs_client::getfile(inum ino, fileinfo &fin)
{
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the file lock
  printf("getfile\n");

  auto l = unique_lock_client(lc, ino);
  auto f = _cache_file(ino);

  extent_protocol::attr a = f->getattr();
  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;

  return r;
}

void
yfs_client::readdir(inum parent, std::unordered_map<std::string, unsigned long> &files)
{
  auto l = unique_lock_client(lc, parent);
  auto f = _cache_file(parent);

  std::string buf;
  f->get(buf);

  parsedir(buf, files);
}

unsigned long
yfs_client::lookup(inum parent, const char *filename)
{
  auto l = unique_lock_client(lc, parent);
  auto f = _cache_file(parent);

  std::string buf;
  f->get(buf);

  return dircontainfile(buf, filename);
}

int
yfs_client::unlink(inum parent, const char *filename)
{
  std::cout << "unlink: " << parent << ", " << filename << std::endl;

  // 1. find the ino
  // 2. remove file from dir
  // 3. remove from extend_server

  auto l = unique_lock_client(lc, parent);
  auto p = _cache_file(parent);

  // update the p->buf: removed the file
  std::string buf;
  p->get(buf);
  auto ino = dirunlinkfile(buf, filename);
  if (ino == 0) {
    return ENOSYS;
  }
  p->put(buf);

  auto lf = unique_lock_client(lc, ino);
  auto f = _cache_file(ino);
  f->remove();

  return OK;
}

int
yfs_client::createfile(inum parent, const char *filename,
        unsigned long &filenum, bool isdir)
{
  std::cout << "createfile: " << parent << ", " << filename << std::endl;
  auto l = unique_lock_client(lc, parent);
  auto p = _cache_file(parent);

  std::string buf;
  p->get(buf);
  if (dircontainfile(buf, filename) != 0) {
    return EXIST;
  }

  // generate a inum for new file
  inum ino = static_cast<inum>(random());
  if (isdir) {
    ino &= ~0x80000000;
  } else {
    ino |= 0x80000000;
  }

  filenum = static_cast<unsigned long>(ino);

  diraddfile(buf, filename, ino);
  p->put(buf);

  // todo: create a function to create file
  auto lf = unique_lock_client(lc, ino);
  auto f = _new_cache_file(ino);

  return OK;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  auto l = unique_lock_client(lc, inum);
  auto f = _cache_file(inum);
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the directory lock

//  printf("getdir %016llx\n", inum);
  extent_protocol::attr a = f->getattr();
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

  return r;
}

void
yfs_client::flush(lock_protocol::lockid_t lid)
{
  if (files.find(lid) == files.end()) {
    return;
  }
  if (files[lid]->is_deleted()) {
    ec->remove(lid);
    return;
  }
  if (files[lid]->is_dirty()) {
    std::string buf;
    files[lid]->get(buf);
    ec->put(lid, buf);
  }
  ec->putattr(lid, files[lid]->getattr());

  files.erase(lid);
}



