#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>
#include <queue>
#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache {
 private:
  int nacquire;
  enum lock_protocol_state {FREE,OCCUPIED};

	typedef struct lock_state
	{
		lock_protocol_state lps;
		std::string owner_id;
		std::queue<std::string> waiting_queue;		 		
} lock_state, *plock_state;

	std::map<lock_protocol::lockid_t, plock_state> locks_map;
	pthread_mutex_t m_map;
	
 public:
  lock_server_cache();
	~lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
