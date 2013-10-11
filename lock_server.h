// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include <map>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"
#include <pthread.h>


class lock_server {

 protected:
  int nacquire;

 public:
  lock_server();
  ~lock_server();
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status granting(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status releasing(int clt, lock_protocol::lockid_t lid, int &);
 
 private:
   std::map<lock_protocol::lockid_t, std::string> locksTable;
   pthread_mutex_t mutexServer;
   pthread_cond_t condOfLock;

};

#endif
