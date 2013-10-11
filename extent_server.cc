// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>

extent_server::extent_server() {
	pthread_mutex_init(&mutexServer,NULL);
}

extent_server::~extent_server(){
  	pthread_mutex_destroy(&mutexServer);
	for(EntrysMap::iterator it = contents.begin(); it != contents.end(); ++it){
		delete it->second;
	}
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  pthread_mutex_lock(&mutexServer);
  // You fill this in for Lab 2.
  //setup ctime, mtime
  time_t curTime;
  time(&curTime);
  extent_protocol::attr attr;
  attr.ctime = curTime;
  attr.mtime = curTime;
  attr.size = buf.size();
  
  if(contents.find(id) != contents.end()){
	//found the entry
		pEntry pCon= contents[id];
		attr.atime = pCon->attribute.atime;
		delete pCon;
	}
  pEntry pCon = new Entry(buf,attr);
  contents[id] = pCon;

  pthread_mutex_unlock(&mutexServer);
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  pthread_mutex_lock(&mutexServer);
  // You fill this in for Lab 2.
  if(contents.find(id) != contents.end()){
	//found the entry, modify buf, update atime
	contents[id]-> attribute.atime = time(NULL);
  	buf = contents[id]-> content;
	pthread_mutex_unlock(&mutexServer);
   	return extent_protocol::OK;	
}
	pthread_mutex_unlock(&mutexServer);
   	return extent_protocol::NOENT;			  
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
	pthread_mutex_lock(&mutexServer);
	if(contents.find(id) != contents.end()){
	//found the entry, return attr
	pEntry pe= contents[id];
	a.size = pe->attribute.size;
  	a.atime = pe->attribute.atime;
  	a.mtime = pe->attribute.mtime;
  	a.ctime = pe->attribute.ctime;

	pthread_mutex_unlock(&mutexServer);
	return extent_protocol::OK;
	}

  pthread_mutex_unlock(&mutexServer);
  return extent_protocol::NOENT;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  pthread_mutex_lock(&mutexServer);
  // You fill this in for Lab 2.
  if(contents.find(id) != contents.end()){
	pEntry pe = contents[id];
	delete pe;
	pe = NULL;
    contents.erase(id);
  pthread_mutex_unlock(&mutexServer);
  return extent_protocol::OK;
}

  pthread_mutex_unlock(&mutexServer);
  return extent_protocol::IOERR;
}

