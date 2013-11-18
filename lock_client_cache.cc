// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"
#include "extent_protocol.h"

using namespace std;

extent_cache_releaser::extent_cache_releaser(class extent_client* ec_out) : ec(ec_out) {}

void extent_cache_releaser::dorelease(lock_protocol::lockid_t lid)
{
	cout <<"++dorelease,thread="<<pthread_self() <<", eid="<<lid<< endl;
	ec->flush(extent_protocol::extentid_t (lid));
}

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  rpcs *rlsrpc = new rpcs(0);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlsrpc->port();
  id = host.str();

  pthread_mutex_init(&m_cache,NULL);
}

lock_client_cache::~lock_client_cache()
{
	pthread_mutex_destroy(&m_cache);
	map<lock_protocol::lockid_t,plock_state>::iterator it;
	for(it = locks_cache.begin() ; it != locks_cache.end(); ++it)
 	{
		pthread_cond_destroy(&it->second->c_waitlock);
		pthread_cond_destroy(&it->second->c_retry);		
		delete it->second;
	}
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int replyByServer = 0;
  lock_protocol::status ret = lock_protocol::OK;
  pthread_mutex_lock(&m_cache);
  cout <<"++lcc::acquire, start, eid"<< lid<< endl;

  if(locks_cache.find(lid) == locks_cache.end())
	{ // don't have this lock
		plock_state ptrls = new lock_state();
		ptrls->retry = 0;
		ptrls->wait_num = 0;
		ptrls->revoke_num = 0;
		ptrls->release_wait_num = 0;
		ptrls->lps = NONE;
		pthread_cond_init(&ptrls->c_waitlock,NULL);
		pthread_cond_init(&ptrls->c_retry,NULL);
		locks_cache.insert(map<lock_protocol::lockid_t, plock_state>::value_type(lid,ptrls));    
	}
	plock_state pls = locks_cache.find(lid)->second;
	
	// revoking this lock,block
	pls->release_wait_num++;
	while(pls->lps == RELEASING)
	{	
	//	cout << "---------waiting lock---------" << pthread_self() << endl;
		pthread_cond_wait(&pls->c_waitlock,&m_cache);
	}
	pls->release_wait_num--;

	bool have_lock = (pls->lps != RELEASING && pls->lps != NONE);
	if(have_lock)
	{//client has the lock in cache;
		//have the lock at local
		pls->wait_num++;
		while(pls->lps != FREE)
			pthread_cond_wait(&pls->c_waitlock, &m_cache);
		pls->wait_num--;
		pls->lps = LOCKED;		
		pthread_mutex_unlock(&m_cache);
	}		
	else
	{	//client doesn't have the lock
		pls->lps = ACQUIRING;
		//unlock locks_cache
		pthread_mutex_unlock(&m_cache);
		ret = cl->call(lock_protocol::acquire, lid, id, replyByServer);
		//lock locks_cache
		pthread_mutex_lock(&m_cache);
		bool retry_c = (ret == lock_protocol::RETRY);
		if(!retry_c)
		 	pls->lps = LOCKED;
		else
		{
			while(pls->retry == 0)	pthread_cond_wait(&pls->c_retry,&m_cache);
			pls->retry--;
		 	pls->lps = LOCKED;
		}
	 pthread_mutex_unlock(&m_cache);
	}
  return ret;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
	pthread_mutex_lock(&m_cache);
	plock_state pls = locks_cache.find(lid)->second;
	bool cache_locally = (pls->wait_num != 0 || pls->revoke_num == 0); 	
	if(cache_locally)
	{  //cache the lock locally
		pls->lps = FREE;
		pthread_cond_signal(&pls->c_waitlock);
	}
	else
	{	
		//return the lock to server
		pls->revoke_num--;
		pls->lps = RELEASING;
		pthread_mutex_unlock(&m_cache);
		int rep = 0;
		cout <<"++lcc::release, release, thread= "<<pthread_self()<< ", eid"<< lid<< endl;
		lu->dorelease(lid);
		cl->call(lock_protocol::release, lid, id, rep);
		pthread_mutex_lock(&m_cache);
		pls->lps = NONE;
		bool can_erase_lock = (pls->release_wait_num==0);
		if(can_erase_lock)
		{
//			cout <<"************erase lock***********\n";
			map<lock_protocol::lockid_t, plock_state>::iterator it = locks_cache.find(lid);
			pthread_cond_destroy(&it->second->c_waitlock);
			pthread_cond_destroy(&it->second->c_retry);		
			delete it->second;
			locks_cache.erase(it);
		}
		else
		{
			pthread_cond_signal(&pls->c_waitlock);
//			cout <<"**********can't erase lock***********\n";
		}
	}
	pthread_mutex_unlock(&m_cache);
  return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, int &)
{
 int ret = rlock_protocol::OK;
	pthread_mutex_lock(&m_cache);
	plock_state pls = locks_cache.find(lid)->second;
	pls->revoke_num++;
	bool cant_release = (pls->lps != FREE || pls->wait_num != 0); 
	if(cant_release)
		pthread_mutex_unlock(&m_cache);
	else
	{	//release
		pls->lps = RELEASING;
		pthread_mutex_unlock(&m_cache);
		cout <<"++lcc::revoke_handler, release, thread= "<<pthread_self()<< ", eid"<< lid<< endl;
		lu->dorelease(lid);
		int reply;
		cl->call(lock_protocol::release, lid, id, reply);
		pthread_mutex_lock(&m_cache);
		pls->lps = NONE;
//		cout << "++++++++++revoke num = " << pls->revoke_num-- << "thread id="<<pthread_self() << endl;
		bool can_erase_lock = (pls->release_wait_num==0);
		if(can_erase_lock)
		{
	//		cout <<"+++++++++erase lock, "<< "thread id="<< pthread_self() <<endl;
			map<lock_protocol::lockid_t, plock_state>::iterator it = locks_cache.find(lid);
			pthread_cond_destroy(&it->second->c_waitlock);
			pthread_cond_destroy(&it->second->c_retry);		
			delete it->second;
			locks_cache.erase(it);
		}
		else
			pthread_cond_signal(&pls->c_waitlock);
		pthread_mutex_unlock(&m_cache);
	}
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, int &)
{
	pthread_mutex_lock(&m_cache);
	plock_state pls = locks_cache.find(lid)->second;
	pls->retry++;
	pthread_cond_signal(&pls->c_retry);
	pthread_mutex_unlock(&m_cache);
  return rlock_protocol::OK;
}
