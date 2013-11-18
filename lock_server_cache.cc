// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"

using namespace std;

lock_server_cache::lock_server_cache()
{
	pthread_mutex_init(&m_map,NULL);
}

lock_server_cache::~lock_server_cache()
{
	pthread_mutex_destroy(&m_map);
}

int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, int &)
{
  pthread_mutex_lock(&m_map);
	map<lock_protocol::lockid_t, plock_state>::iterator it = locks_map.find(lid);
	if(it == locks_map.end())
	{ //no record, create this lock and grant it
		plock_state pls = new lock_state();
		pls->lps = OCCUPIED;
		pls->owner_id = id;
		locks_map.insert(map<lock_protocol::lockid_t,plock_state>::value_type(lid,pls));
		pthread_mutex_unlock(&m_map);
  		return lock_protocol::OK;		
	}

	plock_state pls = it->second;
	if(pls->lps == FREE)
	{
		pls->lps = OCCUPIED;
		pls->owner_id = id;
		pthread_mutex_unlock(&m_map);
  		return lock_protocol::OK;		
	}
	else
	{
		if(pls->waiting_queue.empty())
		{ // revoke lock
//			cout << "@@@@@@@@@@@@@@revoke lock"<< endl;
			pls->waiting_queue.push(id);
			string revoke_owner = pls->owner_id;				
			pthread_mutex_unlock(&m_map);
			handle h(revoke_owner);
			rpcc* cl = h.safebind();
			int reply = 0;
			cl->call(rlock_protocol::revoke, lid, reply);
		}
		else
		{  //return RETRY				
			pls->waiting_queue.push(id);
			pthread_mutex_unlock(&m_map);
		}
		return lock_protocol::RETRY;
	}
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, int &r)
{
	pthread_mutex_lock(&m_map);
	plock_state pls = locks_map.find(lid)->second;
	if(pls->lps == FREE)
	{
		pthread_mutex_unlock(&m_map);
		return lock_protocol::RPCERR;
	}

	bool waiting_retry = (pls->waiting_queue.empty() == false);
	if(waiting_retry)
	{	// wake up waiting client
		string new_owner = pls->waiting_queue.front();
		pls->waiting_queue.pop();
		pls->owner_id = new_owner;
		pthread_mutex_unlock(&m_map);
		handle h(new_owner);
		rpcc *cl = h.safebind();
		int reply = 0;
		cl->call(rlock_protocol::retry, lid, reply);
		pthread_mutex_lock(&m_map);
		bool revoke = (pls->waiting_queue.empty() == false);
		pthread_mutex_unlock(&m_map);		
		if(revoke)
			cl -> call(rlock_protocol::revoke, lid, reply);
	}
	else
	{
		cout <<"###############unlock" << endl;
		pls->lps = FREE;
		pls->owner_id = "";
		pthread_mutex_unlock(&m_map);
	}
 return lock_protocol::OK;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}
