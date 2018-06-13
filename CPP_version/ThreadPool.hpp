#ifndef PTHREADPOOL_HPP_
#define PTHREADPOOL_HPP_
#include <vector>
#include <iostream>
#include <mysql++.h>
#include "SQL.hpp"
#include "Util.hpp"
using namespace std;


class PthreadPool{
public:
	PthreadPool();
	~PthreadPool();
	void Pthreadinit(int _capacity);
	void Addwork(const vector<int*> &tasks);
	int  DestroyPthread();
	static void *func(void* arg);
	static void SetTaskFuntion(void* (*_taskfunction)(void*));
	static int GetTaskQueueNum() {return m_vec.size();};		
	static void CleanTaskQueue();
	static pthreadInfo* GetPthreadInfos() {return threadInfos;};
	bool IsIdl();
	int GetCapacity() {return capacity;};

private:
	static pthread_mutex_t queue_lock;
	static pthread_cond_t  queue_cond;
	static vector<int*> m_vec;
	static pthreadInfo* threadInfos;//线程句柄
	static int isshutdown;//线程是否销毁描述符，0 正常，1 销毁
	int capacity;	
	static void* (*taskfunction)(void*);
};
#endif