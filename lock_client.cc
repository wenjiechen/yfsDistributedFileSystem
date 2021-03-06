// RPC stubs for clients to talk to lock_server

#include "lock_client.h"
#include "rpc.h"
#include <arpa/inet.h>

#include <sstream>
#include <iostream>
#include <stdio.h>

lock_client::lock_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() < 0) {
    printf("lock_client: call bind\n");
  }
}

int
lock_client::stat(lock_protocol::lockid_t lid)
{
  int r;
 //client invoke RPC call
  lock_protocol::status ret = cl->call(lock_protocol::stat, cl->id(), lid, r);
  std::cout << "in client stat, id: " << cl->id() << ", r: " << r << std::endl;
  VERIFY (ret == lock_protocol::OK);
  return r;
}

lock_protocol::status
lock_client::acquire(lock_protocol::lockid_t lockId)
{
  int replyByServer;
  lock_protocol::status ret = cl->call(lock_protocol::acquire, cl->id(), lockId, replyByServer);
//  std::cout << "CLIENT: acquire lockId = " << lockId << ", clientId = " << cl->id() <<", replyByServer = "<<replyByServer << std::endl;
  VERIFY(ret == lock_protocol::OK);
  return replyByServer;
}

lock_protocol::status
lock_client::release(lock_protocol::lockid_t lockId)
{
  int replyByServer;
  lock_protocol::status ret = cl->call(lock_protocol::release, cl->id(), lockId, replyByServer);
//  std::cout << "CLIENT: release lockId = " << lockId << ", clientId = " << cl->id() <<", replyByServer = "<<replyByServer << std::endl;
  
  VERIFY(ret == lock_protocol::OK);
  return replyByServer;
}

