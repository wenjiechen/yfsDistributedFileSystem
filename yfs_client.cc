// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client.h"
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
  lc = new lock_client(lock_dst);
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
  lc->acquire(inum);
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the file lock

  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
  	lc->release(inum);
	return r;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

  lc->release(inum);
  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
	lc->acquire(inum);
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the directory lock

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
  	lc->release(inum);
  	return r;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

  lc->release(inum);
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
	lc->acquire(finum);
	if(isdir(finum)){
		lc->release(finum);
		return IOERR;
	}
	
	extent_protocol::attr attr;
	std::string content;
	if(ec->getattr(finum,attr) != extent_protocol::OK || ec->get(finum,content) != extent_protocol::OK){
		lc->release(finum);
		return IOERR;									
	}
	
	if(newSize > attr.size){
		content.resize(newSize,'\0');
	}
	else{
		content.resize(newSize);
	}
	
	if(ec->put(finum,content) != extent_protocol::OK){
		lc->release(finum);
		return IOERR;	
	}
	
    lc->release(finum);
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
	lc->acquire(finum);
	if(isdir(finum)){
		lc->release(finum);
		return IOERR;
	}	
	std::string tempBuf;
	if(ec->get(finum,tempBuf) != extent_protocol::OK){
		lc->release(finum);
		return IOERR;
	}

	if(offset+readSize > tempBuf.size()){
		buf = tempBuf.substr(offset);
	}
	else{
		buf = tempBuf.substr(offset,readSize);
	}

    lc->release(finum);
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
	lc->acquire(finum);
	if(isdir(finum)){
		lc->release(finum);
		return IOERR;
	}

	std::string curContent;
	if(ec->get(finum, curContent) != extent_protocol::OK){
		lc->release(finum);
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
		lc->release(finum);
		return IOERR;
	}

	lc->release(finum);
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
	lc->acquire(parent_inum);
	std::string dirEnts;
	if( ec->get(parent_inum, dirEnts) != extent_protocol::OK)
	{
		lc->release(parent_inum);
		return IOERR;
	}

	std::string nameString = fName;	
	if(dirEnts.find(nameString + "=") != std::string::npos)
	{	
		lc->release(parent_inum);
		return EXIST;
	}

	finum = rand() | 0x80000000;
	lc->acquire(finum);
	std::string empty;
	if(ec->put(finum,empty) != extent_protocol::OK)
	{
		lc->release(finum);
		lc->release(parent_inum);
		return IOERR;
	}
	lc->release(finum);	
	//update parent_inum's directory's content
	dirEnts += fName;
  	dirEnts += "=";
  	dirEnts += filename(finum);
  	dirEnts += ";";	
	if(ec->put(parent_inum, dirEnts) != extent_protocol::OK) {
		lc->release(parent_inum);
    	return IOERR;
  }
	lc->release(parent_inum);
	return OK;
}


int yfs_client::lookup(inum parent_inum, const std::string &fName, inum &finum)
{
	std::string dirEnts;
	if(ec->get(parent_inum, dirEnts) != extent_protocol::OK)
	{
		return IOERR;
	}

	if(dirEnts.find(fName+"=") == std::string::npos)
	{
		return NOENT;
	}
	std::vector<dirent_t> entrys = parseDirent(dirEnts);
	
	for(size_t i = 0; i < entrys.size() ; ++i)
	{
		if(entrys[i].name == fName)
		{
			finum = entrys[i].inum;
			return OK;
		}
 	}

	return NOENT;
}

//don't need add a lock
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

int yfs_client::mkdir(const inum &parent_inum, const char* dir_name, inum &dir_inum)
{
	lc->acquire(parent_inum);
	std::string parentEnts;
	if( ec->get(parent_inum, parentEnts) != extent_protocol::OK)
	{
		lc->release(parent_inum);
		return IOERR;
	}

	std::string dir_name_s(dir_name); // care for string(buf,size)???	
	if(parentEnts.find(dir_name_s + "=") != std::string::npos)
	{	
		lc->release(parent_inum);
		return EXIST;
	}
	
	//create inum for dir
	dir_inum = rand() & 0x7FFFFFFF; // 32bit is 0

	//update parent directory's content	
	parentEnts += dir_name_s;
  	parentEnts += "=";
  	parentEnts += filename(dir_inum);
  	parentEnts += ";";	
	if(ec->put(parent_inum, parentEnts) != extent_protocol::OK) {
	  lc->release(parent_inum);
      return IOERR;
  }
	lc->release(parent_inum);
  
	lc->acquire(dir_inum);	
//	std::string empty;
	if(ec->put(dir_inum,"") != extent_protocol::OK)
	{
		lc->release(dir_inum);
		return IOERR;
	}
    lc->release(dir_inum);
	return OK;
}

// Remove the file named @name from directory @parent.
// Free the file's extent.
// If the file doesn't exist, indicate error ENOENT.
//
// Do *not* allow unlinking of a directory.
int yfs_client::unlink(const inum &parent_inum, const char* file_name)
{
	lc->acquire(parent_inum);	
	inum file_inum;
	//bug, can't add lock for lookup(), otherwise this lookup() involve will be be blocked by
	//unlink, because unlink() get the lock first. 
	int ret = lookup(parent_inum, file_name, file_inum);  
	if( ret == NOENT)
	{
		lc->release(parent_inum);
		return NOENT;
	}
	else if( ret == IOERR)
	{
		lc->release(parent_inum);
		return IOERR;
	} 

	//not allow for directory
	if(	isdir(file_inum) == true)
	{
		lc->release(parent_inum);
		return IOERR;
	}		
   
	std::string parentEnts;
	ec->get(parent_inum, parentEnts);
	std::vector<dirent_t> entrys = parseDirent(parentEnts);
	for(std::vector<dirent_t>::iterator it = entrys.begin(); it != entrys.end(); it++)
	{
		if(it->inum == file_inum)
		{
			entrys.erase(it);
			break;
		}
	}
	
	std::ostringstream oss;
	for(std::vector<dirent_t>::iterator it = entrys.begin(); it != entrys.end(); ++it)
	{
		oss << it->name << "=" << it->inum << ";";
	}

	if(ec->put(parent_inum, oss.str()) != extent_protocol::OK)
	{
		lc->release(parent_inum);
		return IOERR;
	}
	lc->release(parent_inum);

	lc->acquire(file_inum);
	if(ec->remove(file_inum) != extent_protocol::OK)
	{
		lc->release(file_inum);
		return IOERR;
	}
	lc->release(file_inum);
	return OK;
}
