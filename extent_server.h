// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"
#include <pthread.h>

class extent_server {
 public:
  extent_server();
  ~extent_server();

  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);

  private:
	pthread_mutex_t mutexServer;
	
	//Entry is a file or content of dirctory	
	typedef struct Entry{
		Entry(std::string _content, extent_protocol::attr _attribute){
			content = _content;
			attribute = _attribute;
		} 
    std::string content;
	extent_protocol::attr attribute;
	} Entry, *pEntry;

  typedef std::map<extent_protocol::extentid_t, pEntry> EntrysMap;
  typedef std::map<extent_protocol::extentid_t, pEntry>::value_type valType;
  EntrysMap contents;
  
};
#endif 
