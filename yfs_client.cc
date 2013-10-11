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
#include <stdlib.h>
#include <time.h>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  srand(unsigned(time(0)));

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
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;

  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

 release:

  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;

  printf("getdir %016llx\n", inum);
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

// Set the attributes of a file. Often used as part of overwriting
// a file, to set the file length to zero.
//
// to_set is a bitmap indicating which attributes to set. You only
// have to implement the FUSE_SET_ATTR_SIZE bit, which indicates
// that the size of the file should be changed. The new size is
// in attr->st_size. If the new size is bigger than the current
// file size, fill the new bytes with '\0'.
int yfs_client::setFileSize(inum finum, unsigned long long newSize)
{
	if(isdir(finum)){
		return IOERR;
	}
	
	extent_protocol::attr attr;
	std::string content;
	if(ec->getattr(finum,attr) != extent_protocol::OK || ec->get(finum,content) != extent_protocol::OK){
		return IOERR;									
	}
	
	if(newSize > attr.size){
		content.resize(newSize,'\0');
	}
	else{
		content.resize(newSize);
	}
	
	if(ec->put(finum,content) != extent_protocol::OK){
		return IOERR;	
	}
	
	return OK;
}

// Read up to @size bytes starting at byte offset @off in file @ino.
//
// Pass the number of bytes actually read to fuse_reply_buf.
// If there are fewer than @size bytes to read between @off and the
// end of the file, read just that many bytes. If @off is greater
// than or equal to the size of the file, read zero bytes.
int yfs_client::readFile(inum finum,unsigned long long readSize,unsigned long long offset, std::string &buf)
{
	if(isdir(finum)){
		return IOERR;
	}	
	std::string tempBuf;
	if(ec->get(finum,tempBuf) != extent_protocol::OK){
		return IOERR;
	}

	if(offset+readSize > tempBuf.size()){
		buf = tempBuf.substr(offset);
	}
	else{
		buf = tempBuf.substr(offset,readSize);
	}

	return OK;
}

// If @off + @size is greater than the current size of the
// file, the write should cause the file to grow. If @off is
// beyond the end of the file, fill the gap with null bytes.
//
// Set the file's mtime to the current time.
//
// Ignore @fi.
//
// @req identifies this request, and is used only to send a 
// response back to fuse with fuse_reply_buf or fuse_reply_err.
int yfs_client::writeFile(inum finum, size_t size, off_t offset, const std::string &wBuf)
{
	if(isdir(finum)){
		return IOERR;
	}

	std::string curContent;
	if(ec->get(finum, curContent) != extent_protocol::OK){
		return IOERR;
	}
	
	std::string newContent;
	if(offset > curContent.size()){
		//fill the gap will null bytes
		newContent = curContent;
		std::string gap(offset-curContent.size(),'\0');
		newContent.append(gap);
		newContent.append(wBuf);
	}
	else{
		newContent = curContent.substr(0,offset);
		newContent.append(wBuf);
		if(offset+ size < curContent.size()){
			//write back the end of file
			newContent.append(curContent.substr(newContent.size()));
		}
	}

	if(ec->put(finum,newContent) != extent_protocol::OK){
		return IOERR;
	}

	return OK;
}


std::vector<yfs_client::dirent_t> yfs_client::parseDirent(const std::string &entrysCon)
{
	std::vector<dirent_t> entrys;
	if(entrysCon.size() == 0)
	{
		return entrys;
	}
	std::istringstream iss(entrysCon);
	std::string entry;
	while (std::getline(iss, entry, ';')) {
		dirent_t ent;
		size_t delimPos = entry.find('=');
		ent.name = entry.substr(0,delimPos);
		ent.inum = n2i(entry.substr(delimPos+1));
	
		entrys.push_back(ent);
    }
	return entrys;
}

// Create file @name in directory @parent. 
//
// - @mode specifies the create mode of the file. Ignore it - you do not
//   have to implement file mode.
// - If a file named @name already exists in @parent, return EXIST.
// - Pick an ino (with type of yfs_client::inum) for file @name. 
//   Make sure ino indicates a file, not a directory!
// - Create an empty extent for ino.
// - Add a <name, ino> entry into @parent.
// - Change the parent's mtime and ctime to the current time/date
//   (this may fall naturally out of your extent server code).
// - On success, store the inum of newly created file into @e->ino, 
//   and the new file's attribute into @e->attr. Get the file's
//   attributes with getattr().
//
// @return yfs_client::OK on success, and EXIST if @name already exists.
// don't need check parent_inum exist or not.
int yfs_client::createFile(inum parent_inum,const std::string &fName, inum &finum)
{
	std::string dirEnts;
	ec->get(parent_inum, dirEnts);
	std::vector<dirent_t> entrys = parseDirent(dirEnts);
	
	for(std::vector<dirent_t>::iterator it = entrys.begin(); it !=entrys.end(); ++it)
	{
		if(it->name == fName)
		{
			return EXIST;
		}	
	}

	finum = rand() | 0x80000000;
	std::string empty;
	if(ec->put(finum,empty) != extent_protocol::OK)
	{
		return IOERR;
	}
	//update parent_inum's directory's content
	dirEnts += fName;
  	dirEnts += "=";
  	dirEnts += filename(finum);
  	dirEnts += ";";	
	if(ec->put(parent_inum, dirEnts) != extent_protocol::OK) {
    return IOERR;
  }
	return OK;
}

bool yfs_client::lookupFile(inum parent_inum, const std::string &fName, inum &finum)
{
	std::string dirEnts;
	if(ec->get(parent_inum, dirEnts) != extent_protocol::OK)
	{
		return false;
	}
	std::vector<dirent_t> entrys = parseDirent(dirEnts);
	
	for(std::vector<dirent_t>::iterator it = entrys.begin(); it !=entrys.end(); ++it)
	{
		if(it->name == fName)
		{
			finum = it -> inum;
			return true;
		}	
	}
	return false;
}

int yfs_client::readDir(inum dinum, std::vector<yfs_client::dirent_t> &entrys)
{
	std::string dirEnts;
	if(ec->get(dinum, dirEnts) != extent_protocol::OK)
	{
		return IOERR;
	}

	entrys = parseDirent(dirEnts);
	return OK;
}
