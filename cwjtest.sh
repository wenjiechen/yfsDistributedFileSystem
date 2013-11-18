#########################################################################
# File Name: test.sh
# Author: Wenjie Chen
# mail: wenjie.chen@nyu.edu
# Created Time: Wed 13 Nov 2013 11:57:33 AM EST
#########################################################################
#!/bin/bash
./stop.sh > tmplog
./stop.sh > tmplog
./stop.sh > tmplog
make
export RPC_COUNT=25
./start.sh
./test-lab-3-a.pl ./yfs1 
echo "==========================================="
./test-lab-3-b ./yfs1 ./yfs2 
echo "==========================================="
./test-lab-3-c ./yfs1 ./yfs2 
./stop.sh > tmplog
./stop.sh > tmplog
./stop.sh > tmplog
rm -rf tmplog
