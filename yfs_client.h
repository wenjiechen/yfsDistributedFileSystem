#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>


class yfs_client {
  extent_client *ec;
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

  typedef struct dirent dirent_t;

 private:
  static std::string filename(inum);
  static inum n2i(std::string);
 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  //my functions
  int setFileSize(inum finum, unsigned long long size);
  int readFile(inum finum,unsigned long long readSize,unsigned long long offset,std::string &buf);
  int writeFile(inum finum, size_t size, off_t off,const std::string &wBuf);
  int createFile(inum dir_inum, const std::string &fName, inum &finum);
  std::vector<yfs_client::dirent_t> parseDirent(const std::string &entrysCon);
  bool lookupFile(inum parent_inum, const std::string &fname, inum &finum);
  int readDir(inum dinum, std::vector<yfs_client::dirent_t> &entrys);

};

#endif 
