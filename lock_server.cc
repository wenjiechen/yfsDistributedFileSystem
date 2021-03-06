// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <map>
#include <string>
#include <iostream>
#include <pthread.h>

using namespace std;

lock_server::lock_server():
  nacquire (0)
{
	pthread_mutex_init(&mutexServer,NULL);
    pthread_cond_init(&condOfLock,NULL);	
}

lock_server::~lock_server()
{
  pthread_mutex_destroy(&mutexServer);
  pthread_cond_destroy(&condOfLock); 
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

//for client acquire() call 
lock_protocol::status 
lock_server::granting(int clientId, lock_protocol::lockid_t lockId, int &replyClient)
{
  pthread_mutex_lock(&mutexServer);
//  cout << "SERVER: clientId = " << clientId << " ACQUIRE lockId = " << lockId << endl;

  lock_protocol::status ret = lock_protocol::OK;
  replyClient = ++nacquire;  
  map<lock_protocol::lockid_t, string>::iterator it = locksTable.find(lockId);
  if(it == locksTable.end())
  { 
    locksTable.insert(map<lock_protocol::lockid_t, string>::value_type(lockId,"locked"));
//    cout << "create a new lock, lockid = " << lockId << endl;
    pthread_mutex_unlock(&mutexServer);
    return ret;
  }

  while(it->second == "locked")
  {
	pthread_cond_wait(&condOfLock, &mutexServer);
  }
  
  it->second = "locked";
  pthread_mutex_unlock(&mutexServer);
  return ret;
}

//for client release() call
lock_protocol::status
lock_server::releasing(int clientId, lock_protocol::lockid_t lockId, int &replyClient){

  pthread_mutex_lock(&mutexServer);
//  cout << "SERVER: clientId = " << clientId << " RELEASE lockId = " << lockId << endl;

  lock_protocol::status ret = lock_protocol::OK;
  map<lock_protocol::lockid_t, string>::iterator it = locksTable.find(lockId);
  if(it == locksTable.end())
  {
	cout << "ERROR: doesn't exist lockid = " << lockId << endl;
    //no entry
    ret = lock_protocol::RPCERR;//NOENT;
    pthread_mutex_unlock(&mutexServer);
    return ret;	
  }
  
  it -> second = "free";
  pthread_cond_signal(&condOfLock);
  pthread_mutex_unlock(&mutexServer);
  return ret;
}
