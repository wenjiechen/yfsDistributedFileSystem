#########################################################################
# File Name: test.sh
# Author: Wenjie Chen
# mail: wenjie.chen@nyu.edu
# Created Time: Wed 13 Nov 2013 11:57:33 AM EST
#########################################################################
#!/bin/bash
./stop.sh
./stop.sh > tmplog
./stop.sh > tmplog
make
./lock_server 3333 &
./lock_tester 3333 
echo "========================================="
