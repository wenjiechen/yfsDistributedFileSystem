//
// Lock demo
//

#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"
#include <arpa/inet.h>
#include <vector>
#include <stdlib.h>
#include <stdio.h>

std::string dst;
lock_client *lc;

int
main(int argc, char *argv[])
{

  if(argc != 2){
    fprintf(stderr, "Usage: %s [host:]port\n", argv[0]);
    exit(1);
  }

  dst = argv[1];
  lc = new lock_client(dst);
  //lock_client::stat(lock_protocol::lockid_t lid)
//  r = lc->stat(1);
//  printf ("stat returned %d\n", r);
	lc ->acquire(100);
	std::cout << "**********************************************" << std::endl;
	lc ->acquire(100);
	std::cout << "**********************************************" << std::endl;
    lc ->release(100);
	std::cout << "**********************************************" << std::endl;
    lc ->release(200);
	std::cout << "**********************************************" << std::endl;
	lc ->acquire(100);
}
