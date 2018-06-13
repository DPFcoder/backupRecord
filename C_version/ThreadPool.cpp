#include <stdio.h>
#include "ThreadPool.hpp"



//进程线程日志flag
#define THREAD 1
#define NOTHREAD 0

pthread_cond_t PthreadPool::queue_cond;
pthread_mutex_t PthreadPool::queue_lock;
vector<int*> PthreadPool::m_vec;
pthreadInfo* PthreadPool::threadInfos = NULL;
int PthreadPool::isshutdown = 0;
void*(*PthreadPool::taskfunction)(void*) = NULL;

PthreadPool::PthreadPool()
	:capacity(0)
{
}

PthreadPool::~PthreadPool()
{
	DestroyPthread();
}

void PthreadPool::Pthreadinit(int _capacity)		
{
	capacity = _capacity;
	pthread_mutex_init(&queue_lock,NULL);
	pthread_cond_init(&queue_cond,NULL);
	m_vec.empty();
	if (capacity == 0)
		return;
	threadInfos = new pthreadInfo[capacity];				//线程ID和状态
	memset(threadInfos,0,sizeof(pthreadInfo)*capacity);
	for(int i=0; i<capacity; i++)//启动线程
	{
		pthread_create(&(threadInfos[i].pthreadid),NULL,&func,(void*)(&i));				 
		LOG_DEBUG(THREAD, "Create pthread %d, pthreadId:%lu",i,threadInfos[i].pthreadid);
	}

	LOG_DEBUG(THREAD, "Create pthread num is:%d", capacity);
}

void PthreadPool::SetTaskFuntion(void* (*_taskfunction)(void*))
{
	taskfunction = _taskfunction;
}

void* PthreadPool::func(void * arg)
{
	int pthreadid = *(int*)arg;
	while(1)
	{
		pthread_mutex_lock(&queue_lock);
		while(m_vec.size()==0 && isshutdown==0)//如果vec为空则说明没任务，需要继续等待，直到任务添加以后被唤醒
		{
			threadInfos[pthreadid].status = 1;
			LOG_DEBUG(THREAD, "pthread_wait Ox%x +++++++++++\n",pthread_self());
			pthread_cond_wait(&queue_cond,&queue_lock);               
		}
	
		if(isshutdown==1)//如果标志置为退出则结束当前线程		
		{	
			pthread_mutex_unlock(&queue_lock);
			LOG_DEBUG(THREAD, "threadid Ox%x is exit----------\n",pthread_self());
			pthread_exit(NULL);
		}
		//*********************************************************************

		//说明在线程池中有任务可执行了。
		vector<int*>::iterator it = m_vec.begin();
		void* cameraId = *it;
		m_vec.erase(it);
		pthread_mutex_unlock(&queue_lock);
		threadInfos[pthreadid].status = 0;
		(*taskfunction)(cameraId); 	
	}
}
void PthreadPool::Addwork(const vector<int*> &tasks)			
{
	pthread_mutex_lock(&queue_lock);			
	for (int i=0; i<tasks.size(); ++i)
		m_vec.push_back(tasks[i]);																							 
	pthread_mutex_unlock(&queue_lock);
	pthread_cond_broadcast(&queue_cond);
}


void PthreadPool::CleanTaskQueue()
{
	if (!m_vec.size())
		return;

	pthread_mutex_lock(&queue_lock);	
	for(int i=0; i<m_vec.size(); ++i)
	{
		int *tmp = m_vec[i];
		delete tmp;
	}
	m_vec.clear();
	pthread_mutex_unlock(&queue_lock);
	LOG_DEBUG(NOTHREAD,"clean task queue");
}

int PthreadPool::DestroyPthread()
{
	if(isshutdown==1)
		return -1;
	isshutdown=1;
	pthread_cond_broadcast(&queue_cond);//唤醒所有线程。主要是线程销毁的时候防止错误条件变量函数的阻塞
	for(int i=0;i<capacity;i++)
	{
		pthread_join(threadInfos[i].pthreadid,NULL);
	}
	delete[] threadInfos;
	threadInfos = NULL;
	pthread_mutex_destroy(&queue_lock);
	pthread_cond_destroy(&queue_cond);
	return 0;
}

bool PthreadPool::IsIdl()
{
	for(int i=0; i<capacity; ++i)
		if (!threadInfos[i].status)
			return false;

	return true;
}