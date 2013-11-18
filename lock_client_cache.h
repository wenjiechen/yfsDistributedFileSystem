// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"
#include "pthread.h"
#include <vector>
#include <map>
#include "extent_client.h"

using namespace std;
// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

class extent_cache_releaser: public lock_release_user
{
	private:
		class	extent_client *ec;
	public:
		extent_cache_releaser(class extent_client* ec_out);
		virtual void dorelease(lock_protocol::lockid_t lid);
};

class lock_client_cache : public lock_client {
 private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;

	//protocol state
	enum lock_protocol_state { NONE, FREE, LOCKED, ACQUIRING, RELEASING };

	//lock 
	typedef struct lock_state
	{
		lock_protocol_state lps;
		int retry;
		int wait_num;
		int revoke_num;
		int release_wait_num;

		pthread_cond_t c_waitlock; //wait for the same lock
		pthread_cond_t c_retry;	   //server send retry to wake up waitting thread

	} lock_state, *plock_state;
	map<lock_protocol::lockid_t, plock_state> locks_cache;
	pthread_mutex_t m_cache;

 public:
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache();
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
                                       int &);
};


#endif
