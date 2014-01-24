Introduction
---
There are 7 labs in different branchs, which builds a multi-server file system called Yet Another File System (yfs) in the spirit of [Frangipani](http://www.news.cs.nyu.edu/~jinyang/fa08/papers/frangipani.pdf). In the end, the system looks like

![alt text](http://www.news.cs.nyu.edu/~jinyang/fa12/labs/yfs.jpg)

The client side process is call "yfs" using [FUSE](http://fuse.sourceforge.net/), which provides a fully functional filesystem operations.Each client host will run a copy of yfs. Each yfs will create a file system visible to applications on the same machine, and FUSE will forward application file system operations to yfs. All the yfs instances will store file system data in a single shared "extent" server, so that all client machines will see a single shared file system.

Set up on Ubuntu
---
To install FUSE, type "sudo aptitude install libfuse2 fuse-utils libfuse-dev ". Then add yourself to the FUSE group by typing "sudo adduser {your_user_name} fuse". 

