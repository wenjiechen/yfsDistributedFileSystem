// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <rpc/slock.h>

using namespace std;

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }

	pthread_mutex_init(&m_cache,NULL);
}

extent_client::~extent_client()
{
	for(caches_iterator it = caches.begin(); it != caches.end(); it++ )
	{
		delete it->second;
	}
	pthread_mutex_destroy(&m_cache);
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  //check caches
	extent_protocol::status ret = extent_protocol::OK;
	ScopedLock sl(&m_cache);  
  caches_iterator it = caches.find(eid);
	bool hit_cache = (it != caches.end());
	if(hit_cache)
	{ //1 hit cache, return cache
	    cout<<"++ec:get(): hit cache +thread id ="<<pthread_self()<<", eid = "<< eid << endl;
		if(it->second->removed == true)
		{
			cout<<"++removed error 1 +"<<endl;
			return extent_protocol::NOENT;
		} 
	  buf = it->second->content;
	  it->second->attribute.atime = time(NULL);
	}
	else
	{ //else, get from server, save to cache, return
	    cout<<"++ec:get(): miss cache ++thread id ="<<pthread_self()<<", eid = "<< eid << endl;
		string t_content = "";
		extent_protocol::attr t_attribute;
  		int ret1 = cl->call(extent_protocol::get, eid, t_content);
		int ret2 = cl->call(extent_protocol::getattr,eid, t_attribute);
		if(	ret1 != extent_protocol::OK || ret2 != extent_protocol::OK)
		{
	    	cout<<"++ec:get(): IOERR +"<<pthread_self() << endl;
			return extent_protocol::IOERR;
		}
		pExtent_cache pec= new extent_cache();
		pec->dirty = false;
		pec->removed = false;
		pec->content = t_content;
		pec->attribute = t_attribute; //structer assignment
		buf = t_content; 
		caches.insert(map<extent_protocol::extentid_t, pExtent_cache>::value_type(eid,pec));
	}
 	return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  ScopedLock sl(&m_cache);
	caches_iterator it = caches.find(eid);
	bool hit_cache = (it != caches.end());
	if(hit_cache)
	{
	    cout<<"++ec: getattr(): hit cache +thread id ="<<pthread_self()<<", eid = "<< eid << endl;
		if(it->second->removed == true)
		{
			cout<<"++removed error 2 +"<<endl;
			return extent_protocol::NOENT;
		}
		attr = it->second->attribute;
	}
	else
	{// miss cache, get from server and save to cache
	    cout<<"++ec: getattr(): miss cache +thread id ="<<pthread_self()<<", eid = "<< eid << endl;
		string t_content = "";
		extent_protocol::attr t_attribute;
  	int ret1 = cl->call(extent_protocol::get, eid, t_content);
		int ret2 = cl->call(extent_protocol::getattr,eid, t_attribute);
		if(	ret1 != extent_protocol::OK || ret2 != extent_protocol::OK)
		{
			return extent_protocol::IOERR;
		}
		pExtent_cache pec= new extent_cache();
		pec->dirty = false;
		pec->removed = false;
		pec->content = t_content;
		pec->attribute = t_attribute; //structer assignment
		attr = t_attribute;
	//	cout <<"-----------------------attr.size = "<<attr.size<<", atime = "<<attr.atime<< endl;
		caches.insert(map<extent_protocol::extentid_t, pExtent_cache>::value_type(eid,pec));
	}
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
	cout<<"++ec:put(): start, eid = "<< eid << endl;
  extent_protocol::status ret = extent_protocol::OK;
	ScopedLock sl(&m_cache);
	time_t ct = time(NULL);
	caches_iterator it = caches.find(eid);
	bool hit_cache = (it != caches.end());
  if(hit_cache)
	{//modify cache
		cout<<"++ec:put(): hit cache +thread id ="<<pthread_self()<<", eid = "<< eid << endl;
		pExtent_cache pec = it->second;
		
		if(pec->removed == true)
		{
			cout<<"++removed error 3 +"<<endl;
			return extent_protocol::IOERR;
		}
	
		pec->dirty = true;
		pec->content = buf;
    	pec->attribute.ctime = ct;
		pec->attribute.mtime = ct;
		pec->attribute.size = buf.size();
	}
	else
	{//add content to caches
		cout<<"++ec: put(): miss cache ++thread id ="<<pthread_self()<<", eid = "<< eid << endl;
		pExtent_cache pec = new extent_cache();
		pec->dirty = true;
		pec->removed = false;
		pec->content = buf;
		pec->attribute.ctime = ct;
		pec->attribute.mtime = ct;
		pec->attribute.size = buf.size();
		caches.insert(map<extent_protocol::extentid_t, pExtent_cache>::value_type(eid,pec));
		cout<<"++ec: put(): miss cache,insert = "<< (caches.find(eid) !=caches.end()) <<", eid"<< eid << endl;
		
	}
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
	ScopedLock sl(&m_cache);
	if(caches.find(eid)== caches.end())
	{
		cout<<"++ec:remove(): miss cache ++thread id ="<<pthread_self()<<", eid = "<< eid << endl;
		pExtent_cache pec = new extent_cache();
		pec->dirty = false;
		pec->content = "";
		caches.insert(map<extent_protocol::extentid_t, pExtent_cache>::value_type(eid,pec));
	}
	caches[eid]->removed = true;
  return ret;
}

int extent_client::flush(extent_protocol::extentid_t eid){
	ScopedLock sl(&m_cache);
	caches_iterator it = caches.find(eid);
	if(it == caches.end())
	{
		cout<<"++ec:flush(): IOERR, thread ="<<pthread_self()<<", eid = "<< eid << endl;
		return extent_protocol::IOERR;
	}
	pExtent_cache pec = it->second;
	if( pec->removed == true)
	{	
		cout<<"++ec:flush(): removed, thread ="<<pthread_self()<<", eid = "<< eid << endl;
		int r = 0;
		cl->call(extent_protocol::remove, eid, r);
	}
	else if( pec->dirty == true )
	{
		cout<<"++ec:flush(): dirty, thread ="<<pthread_self()<<", eid = "<< eid << endl;
		int r = 0;
		cl->call(extent_protocol::put,eid,pec->content,r);
	}
	else
	{
		cout<<"++ec:flush(): clean, thread ="<<pthread_self()<<", eid = "<< eid << endl;
	}
	caches.erase(it);
	cout <<"++ec:flush(): delete cache: "<<(caches.find(eid) == caches.end())<< endl;
	delete pec;
//	pec = NULL;
	return extent_protocol::OK; 
}













