// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "rpc.h"
#include <pthread.h>
#include <map>
#include "lock_client_cache.h"


class extent_client {
 private:
  rpcc *cl;

	typedef struct extent_cache
	{
		bool dirty;
		bool removed;
		std::string content;
		extent_protocol::attr attribute;
	}extent_cache, *pExtent_cache;

	typedef std::map<extent_protocol::extentid_t, pExtent_cache>::iterator caches_iterator;
	std::map<extent_protocol::extentid_t, pExtent_cache> caches;
	pthread_mutex_t m_cache;

 public:
  extent_client(std::string dst);
	~extent_client();
  extent_protocol::status get(extent_protocol::extentid_t eid, 
			      std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);	
	int flush(extent_protocol::extentid_t eid);
};

#endif 

