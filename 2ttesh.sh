#########################################################################
# File Name: test.sh
# Author: Wenjie Chen
# mail: wenjie.chen@nyu.edu
# Created Time: Wed 13 Nov 2013 11:57:33 AM EST
#########################################################################
#!/bin/bash
./stop.sh
./stop.sh
./stop.sh
make
./start.sh
./test-lab-3-c ./yfs1 ./yfs2 
echo "========================================="
./stop.sh > tmplog
./stop.sh > tmplog
./stop.sh > tmplog
rm -rf tmplog
