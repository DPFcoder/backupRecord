#include<execinfo.h>   
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <sys/time.h>
#include "XmlRpc.h"
#include "XmlRpcValue.h"
#include "SocketUtil.hpp"
#include "SQL.hpp"				//	����ͷ�ļ�"Util.hpp"֮ǰ
#include "Util.hpp"
#include "mp4aac/convert.h"
#include "BSFPDefined.hpp"
#include <sys/types.h>
#include <sys/wait.h>
#include <mysql++.h>
#include <map>
#include <sys/shm.h>
#include "ThreadPool.hpp"
#include "Alarm.hpp"
//�����߳���־flag
#define THREAD 1
#define NOTHREAD 0


using namespace XmlRpc;
using namespace std;

char relIp[16] ={'\0'};
XmlRpcClient* globalXmlRpcClient[10] = {'\0'};		//ÿ��Streamer��������Ӧһ��XmlRpcClient
map<int,XmlRpcClient*> CameraIdToStreamerMap;
BsrSQL bsrsql;
mysqlpp::Connection con;
int backupPid;				    //ֻ����������ʹ��   
char saveFilePath[100] = {'\0'};
char VirtualStreamerIP[20] = {'\0'};					//ֱ��streamer�������󣬼������뱸���ĸ���IP  VirtualStreamerIP
int transRecordStartTime;			//��ʼǨ����Ƶʱ��
int transRecordStopTime;				//ֹͣǨ����Ƶʱ��
transRecordTimeInfo	RecordStartTime;		//��Ƶ���ʱ��
transRecordTimeInfo RecordStopTime;		//��Ƶ����ʱ��
int threadcounts;
int videoRecordCounts;		//ÿ�α��ݵ���Ƶ�ļ�����
pthread_mutex_t globalXmlRpcMutex = PTHREAD_MUTEX_INITIALIZER;				 
int*  nasMountStatus;	  //0 ��ʶnas ����ʧ��  1 ��ʶnas ���سɹ�
Alarm backupAlarm;
PthreadPool threadpool;



// ͨ��BSFP�еı�־λ��������Ƶ�ֱ��ʵĿ��
void getVideoResolution(int bsfptab, int* pWidth, int* pHeight)
{
	// default 720P  
	*pWidth = 1080;
	*pHeight = 720;

	switch (bsfptab)
	{
	case BSTP_FORMAT_DCIF:
		*pWidth = 176;
		*pHeight = 144;
		break;

	case BSTP_FORMAT_CIF:
		*pWidth = 352;
		*pHeight = 288;
		break;

	case BSTP_FORMAT_2CIF:
		*pWidth = 704;
		*pHeight = 288;
		break;

	case BSTP_FORMAT_D1:
		*pWidth = 704;
		*pHeight = 576;
		break;

	case BSTP_FORMAT_HALFD1:
		*pWidth = 352;
		*pHeight = 576;
		break;

	case BSTP_FORMAT_RCIF:
		*pWidth = 288;
		*pHeight = 352;
		break;

	case BSTP_FORMAT_RD1:
		*pWidth = 576;
		*pHeight = 704;
		break;

	case BSTP_FORMAT_720I:
		*pWidth = 1280;
		*pHeight = 720;
		break;

	case BSTP_FORMAT_720P:
		*pWidth = 1280;
		*pHeight = 720;
		break;

	case BSTP_FORMAT_1080I:
		*pWidth = 1920;
		*pHeight = 1080;
		break;

	case BSTP_FORMAT_1080P:
		*pWidth = 1920;
		*pHeight = 1080;
		break;

	case BSTP_FORMAT_960H:
		*pWidth = 960;
		*pHeight = 576;
		break;

	case BSTP_FORMAT_R720P:
		*pWidth = 720;
		*pHeight = 1080;
		break;

	case BSTP_FORMAT_R1080P:
		*pWidth = 1080;
		*pHeight = 1920;
		break;

	default:
		break;
	} 
	return;
}

void SystemErrorHandler(int signum)  
{  
	const int len=1024;  
	void *func[len];  
	size_t size;  
	int i;  
	char **funs;  

	signal(signum,SIG_DFL);  
	size=backtrace(func,len);  
	funs=(char**)backtrace_symbols(func,size);  
 	LOG_DEBUG(NOTHREAD,"System error,Stack trace:----------"); 
	for(i=0;i<size;++i) 
		LOG_DEBUG(NOTHREAD,"---------%d %s \n",i,funs[i]);  
	free(funs);  
}  

void sendBackupAlarm(unsigned long alarmType)
{
	ALARM_INFO alarmInfo;
	memset(&alarmInfo,0,sizeof(alarmInfo));
	alarmInfo.myip = (unsigned long)inet_addr(VirtualStreamerIP);
	alarmInfo.event = alarmType;
	alarmInfo.which = 0;
	alarmInfo.time = time(NULL);
	backupAlarm.sendAlarm(alarmInfo,relIp,2134);
	LOG_DEBUG(THREAD,"Send Alarm!!!");
	sleep(1);
}

bool loginDB(const MysqlParams& sqlparas)
{
	bsrsql.setCon(&con);
	for (int i=0; i<3; ++i)
	{
		if(bsrsql.connectToDb(sqlparas))
		{
			LOG_DEBUG(NOTHREAD,"Connect datebase successed");
			return true;
		}
		else 
		{
			LOG_DEBUG(NOTHREAD,"Connect datebase failed");
			sleep(3);
		}
	}

	return false;
}

//��ȡ����streamer��Ӧ��cameraId,��cameraId��streamer����ӳ��   
bool getCamerasInStreamer(const char * streamerIp,vector<int> &CameraIds,map<int,XmlRpcClient*> porttoxmlrpc)
{
	char sql[150];
	sprintf(sql,"SELECT c.id,st.port FROM Camera c, Site s, Streamer st WHERE c.site=s.id AND s.streamer=st.id AND st.ip='%s'",streamerIp);					 
	mysqlpp::Result result; 
	int nRows = 0;
	for (int i=0; ; ++i)
	{
		if (bsrsql.query(sql,result,nRows) || bsrsql.Ping() == 0)
			break;
		sleep(1);
		if (i == 3)		
			return false;
	}

	mysqlpp::Row row;
	for( int i=0; i<nRows; ++i)
	{
		row = result.fetch_row();
		int cameraId = (int)row[(unsigned int)0];
		CameraIds.push_back(cameraId);

		CameraIdToStreamerMap[cameraId] = porttoxmlrpc[(int)row[1] ];
	}
	return true;
}

//��ȡ������Ƶ��ʼ�ͽ���time_t����ʱ��
static void GetTransRecordTime(time_t& begin, time_t& end)		  			 
{
	time_t nowtime = time(NULL);
	struct tm *ptr = localtime(&nowtime);
	struct tm begintime(*localtime(&nowtime));				 
	struct tm	endtime(*localtime(&nowtime));
	begintime.tm_hour = RecordStartTime.time_hour;
	begintime.tm_min = RecordStartTime.time_min;
	begintime.tm_sec = RecordStartTime.time_sec;
	endtime.tm_hour = RecordStopTime.time_hour;
	endtime.tm_min = RecordStopTime.time_min;
	endtime.tm_sec = RecordStopTime.time_sec;

	if (transRecordStartTime > transRecordStopTime)					//���ݿ�ʼ�ͽ���ʱ����24������
	{
		if (ptr->tm_hour > begintime.tm_hour
			|| ptr->tm_hour == begintime.tm_hour)			//���ݵ���ʱ��
		{
			begin = mktime(&begintime);
			end = mktime(&endtime);
		}
		else	 																			 
		{
			begin = mktime(&begintime) - 24*60*60;		//����
			end = mktime(&endtime) - 24*60*60;
		}
	}
	else if (transRecordStartTime < transRecordStopTime)   //�賿ǰ�����꣬���賿��ʼ����
	{
		if (ptr->tm_hour > RecordStopTime.time_hour)									//��ǰʱ����ڵ���¼�����ʱ�� ���ݵ�����Ƶ
		{
			begin = mktime(&begintime);
			end = mktime(&endtime);
		}
		else																			//����ǰһ����Ƶ
		{
			begin = mktime(&begintime) - 24*60*60;  
			end = mktime(&endtime) - 24*60*60;
		}
	}
}

//��鵱ǰʱ���Ƿ񳬹�Ǩ��ʱ�䷶Χ			 
bool isOutBackupTime()
{
	time_t nowtimetmp = time(NULL);
	struct tm* ptime =  localtime(&nowtimetmp);
	if (transRecordStartTime > transRecordStopTime				 
		&& ptime->tm_hour > transRecordStopTime
		&& ptime->tm_hour < transRecordStartTime)					 
		return true;

	else if (transRecordStartTime < transRecordStopTime				 
		&& (ptime->tm_hour > transRecordStopTime 
		|| ptime->tm_hour < transRecordStartTime))					 
		return true;

	return false;
}

//����CamreaId,��ȡ��Ӧ��Ƶ�ļ�records
bool gainCameraRecords(const int& cameraId, XmlRpcValue &resultRecords)
{
	time_t startTime, stopTime;
	GetTransRecordTime(startTime,stopTime);

	if (isOutBackupTime())
	{
		time_t nowtime = time(NULL);
		struct tm* ptime =  localtime(&nowtime);
		if (ptime->tm_hour <= RecordStopTime.time_hour)			//��������ʱ���ѯ
		{
			startTime -= 3600*12;
			stopTime  -= 3600*12;
		}
	}
	XmlRpcValue params;
	params["cameraId"] = cameraId;
	params["startTime"] = (int)startTime;
	params["endTime"] = (int)stopTime;
	params["eventType"] = 0; // ��ʾ���Դ�����
	params["attachId"] = 0;  // ͬ��

	int nRow = 0;
	mysqlpp::Result res;
	char sql[50] ={'\0'};
	sprintf(sql,"SELECT recordDir from Camera where id = %d",cameraId);

	for(int i=0; ; ++i)
	{
		if (bsrsql.query(sql,res,nRow) || bsrsql.Ping() == 0)					//���ݿ�ִ�гɹ�  ��  ִ��ʧ�������ݿ�����û����
			break;

		LOG_DEBUG(THREAD,"check failed, %s",sql);
		sleep(1);
		if (i == 3)	return false;
	}
	if (nRow == 1)
	{
		mysqlpp::Row row = res.fetch_row();
		params["recordDir"] = (string)row[(unsigned int)0];											//cameraId��Ӧ��Ƶ����Ŀ¼
	}



	for(; ;)				 
	{
		pthread_mutex_lock(&globalXmlRpcMutex);
		if (!CameraIdToStreamerMap[cameraId]->execute("Streamer.Stream.History.list",params,resultRecords) || CameraIdToStreamerMap[cameraId]->isFault())							//����ֵ��������  ��ֹʱ�����24Сʱ
		{
			if (CameraIdToStreamerMap[cameraId]->isConnected())			//�����ӶϿ�ִ��ʧ��������ִ��  ��ִ֤�г���ԭ����xmlrpc��������
			{
				pthread_mutex_unlock(&globalXmlRpcMutex);
				//LOG_DEBUG(THREAD,"CameraId:%d  Streamer.Stream.History.list  failed",cameraId);
				return false;
			}

			pthread_mutex_unlock(&globalXmlRpcMutex);
			LOG_DEBUG(THREAD,"Streamer was not connected�� try execute again");
			sleep(5);
			if (isOutBackupTime())		//��鵱ǰʱ���Ƿ񳬹�Ǩ��ʱ�䷶Χ			 
			{
				LOG_DEBUG(THREAD,"Its out of Backup time!");
				return false;
			}
		}
		else
		{
			pthread_mutex_unlock(&globalXmlRpcMutex);
			return true;
		}
	}
}


//�жϱ���Ŀ¼�Ƿ�����ڸ�Ŀ¼
bool IsMountOnRootDev(void)
{
	FILE *p = NULL;
	string rootDev;
	string mountDev;
	bool checkRootDev = false;
	bool checkMountDev = false;
	char buf[256] = {0,};
	string s = "/etc/mtab";  

	do{
		p = fopen(s.c_str(),"r");
		if(p == NULL) {
			break;
		}
		while(fgets(buf,sizeof(buf), p) != NULL) {
			char bufdev[128]={0,};
			char bufkey[128]={0,};
			char bufothrer[128] = {0,};
			sscanf(buf,"%s %s %s",(char*)&bufdev, (char*)&bufkey,(char*)bufothrer);
			if(('0' == *bufkey ) || ('0' == *bufdev)){
				continue; 
			}
			if(!strcmp(bufkey, "/"))
			{
				rootDev = bufdev;
				checkRootDev = true;
			}
			else if (!strcmp(bufkey,saveFilePath))
			{
				mountDev = bufdev;
				checkMountDev = true;

			}

			if (checkRootDev && checkMountDev)
				break;
		}
	}while(0);

	if(p){
		fclose(p);
		p = NULL;
	}

	if (!rootDev.empty() && !strcmp(rootDev.c_str(),mountDev.c_str()))
		return true;
	else
		return false;
}

//�����̼��nas����
bool checkNasMount()			 
{
	string checknas = "df -h | grep " + string(saveFilePath);
	int status = -1;										 
	for(int i=0; ; ++i)
	{
		if ((status = system(checknas.c_str()) ) != -1)									
			break;

		LOG_DEBUG(NOTHREAD,"check nas mount failed!");
		sleep(1);

		if (i == 3)
		{
			*nasMountStatus = 0;
			return false;
		}
	}

	do 
	{
		if (status != 0)
		{
			*nasMountStatus = 0;
			LOG_DEBUG(NOTHREAD,"============Nas mounted failed!! Please check Nas mount status===============");
			break;
		}
		else if (status == 0)
		{
			if (IsMountOnRootDev())
			{
				LOG_DEBUG(NOTHREAD,"============Mount on RootDev !! Please check Nas mount status===============");
				*nasMountStatus = 0;
				break;
			}
			else
			{
				*nasMountStatus = 1;
				return true;
			}
		}
	}while(0);

	return false;
}

//ͳ�Ʊ���������BackupInfo ���ж�Ӧ����Ƶ�ļ�����
int gainDBRecordsToCameras()
{
	int RecordsCounts = 0;

	char sql[200];
	sprintf(sql,"select count(cameraId) from BackupInfo where cameraId in (SELECT c.id FROM Camera c, Site s, Streamer st WHERE c.site=s.id AND s.streamer=st.id AND st.ip='%s')",VirtualStreamerIP);			

	int nRows;
	mysqlpp::Result res;
	if (!bsrsql.query(sql,res,nRows))
	{
		LOG_DEBUG(THREAD,"select count(cameraId) from BackupInfo where cameraId in (SELECT c.id FROM Camera c, Site s, Streamer st WHERE c.site=s.id AND s.streamer=st.id AND st.ip='%s')",VirtualStreamerIP);
		return -1;
	}
	mysqlpp::Row row = res.fetch_row();
	nRows = (int)row[(unsigned int)0];

	return nRows;
}

//������Ƶ��ת����Ƶ
static void *startTransRecord(void* cameraId)
{
	int CameraId = *(int*)cameraId;
	delete (int*)cameraId;
	char sql[50] = {'\0'};
	sprintf(sql,"select name from Camera where id = %d",CameraId);	
	mysqlpp::Result res;
	int rows;
	for(int i=0; ;++i)
	{
		if (bsrsql.query(sql,res,rows) || bsrsql.Ping() == 0)
			break;
		sleep(3);
		LOG_DEBUG(THREAD,"get name of CameraId %d failed",CameraId);
		if (i==3)
		{
			LOG_DEBUG(THREAD,"backup Videos of CameraId %d failed",CameraId);
			return NULL;
		}
	}

	mysqlpp::Row row = res.fetch_row();
	string filename = (string)row[(unsigned int)0];

	XmlRpcValue resultRecords;
	if (!gainCameraRecords(CameraId, resultRecords))			
	{
		LOG_DEBUG(THREAD,"Streamer.Stream.History.list  failed!  Need Backup Camera Info:cameraId:%d ",CameraId);			//��ʾ���ݵ�������ͷһ�����¼���ļ�
		return NULL;
	}

	int cameraRecord =  resultRecords["records"].size();  

	LOG_DEBUG(THREAD,"CameraId %d  match videoRecord number is %d",CameraId,cameraRecord);
	//��ȡÿ��CameraId��Ӧ��һ��������Ƶ�ļ���Ϣ	
	for (int k=0; k<cameraRecord; ++k)										 
	{
		//�����̿ռ䣬С��100M�ͷ���	  //��nas����ʧ�ܻ�����ԭ��Ͽ�������¼�����
		if (!checkDiskSpace(saveFilePath))
		{
			LOG_DEBUG(THREAD,"The store disk space is full or the path does not exist!!  Need Backup Camera Info�� cameraId:%d ",CameraId);
			return NULL;
		}
		int rebackCounts = 3;	 //���ڵ���¼��Ǩ�ƺ��쳣�ļ�������Ǩ�Ƶ�������
reback:										//��Ƶ���ݹ�����streamer�����жϣ�����streamer�����±���

		int starttime = (int)resultRecords["records"][k]["startTime"];
		int stoptime = (int)resultRecords["records"][k]["endTime"];

		//������ݿ��BackupInfo�и���Ƶ�ļ��Ƿ��Ѿ�����
		int nRow = 0;
		mysqlpp::Result res;
		char sql[150] = {'\0'};
		sprintf(sql,"select count(cameraId) from BackupInfo where cameraId = %d and recordId = %d",CameraId,k);
		if (!bsrsql.query(sql,res,nRow))
		{
			LOG_DEBUG(THREAD,"check failed, %s",sql);
		}
		else
		{
			mysqlpp::Row row = res.fetch_row();
			if ((int)row[(unsigned int)0] >= 1)		//�ѱ��� 
			{
				LOG_DEBUG(THREAD,"cameraId %d recordId %d has backuped",CameraId,k);
				continue;
			}
		}

		//��鵱ǰʱ���Ƿ񳬹�Ǩ��ʱ�䷶Χ			 
		if (isOutBackupTime())
		{
			LOG_DEBUG(THREAD,"Its out of Backup time!  Need Backup Video Info�� cameraId:%d  starttime:%d  stoptime:%d",CameraId,starttime,stoptime);
			continue;		 
		}

		//������Ŀ¼�Ƿ����
		if (access(saveFilePath,F_OK))          
		{
			LOG_DEBUG(THREAD,"saveFilePath is not exist");
			return NULL;
		}
		//���nas����
		if (*nasMountStatus == 0)
		{
			LOG_DEBUG(THREAD,"NAS no mount,goto reback:cameraId:%d  starttime:%d  stoptime:%d",CameraId,starttime,stoptime);
			sleep(100);
			goto reback;
		}

		//���һ��Ŀ¼�Ƿ���ڣ��������򴴽�
		string firstSaveDir = string(saveFilePath)+ "/"+filename+"_"+toStr(CameraId);
		struct stat64 stbuf1,stbuf2;
		if(stat64(firstSaveDir.c_str(), &stbuf1) == -1)					
		{
			if(mkdir(firstSaveDir.c_str(), 0700))
				LOG_DEBUG(THREAD,"=======================mkdir %s failed======errno:%d =============",firstSaveDir.c_str(),errno);
		}

		//������Ŀ¼�Ƿ���ڣ��������򴴽�
		time_t nowTime = time(NULL);
		struct tm *p = localtime(&nowTime);
		char filepath[50] = {'\0'};
		string secondSaveDir = firstSaveDir+"/"+toStr(p->tm_year+1900)+"_";
		sprintf(filepath,"%s/%d-%02d",firstSaveDir.c_str(),p->tm_year+1900,p->tm_mon+1);
		if (stat64(filepath, &stbuf2) == -1)
		{
			if (mkdir(filepath, 0700))
				LOG_DEBUG(THREAD,"=======================mkdir %s failed======errno:%d=============",filepath,errno);
		}

		//�ļ���
		time_t tmp = (time_t)starttime;
		struct tm *pstart = localtime(&tmp); 
		char filename[50] = {'\0'};
		sprintf(filename,"%02d%02d%02d-%d%02d%02d",pstart->tm_hour,pstart->tm_min,pstart->tm_sec,pstart->tm_year+1900,pstart->tm_mon+1,pstart->tm_mday);
		string saveFileName = string(filepath)+"/"+string(filename)+".mp4";
		if (access(saveFileName.c_str(), F_OK))					 
			remove(saveFileName.c_str());

		XmlRpcValue params,result;		
		params["cameraId"] = CameraId;
		params["startTime"] = starttime;
		params["endTime"] = stoptime;    
		//*************************************************************************
		nRow = 0;
		mysqlpp::Result res1;
		memset(sql,0,sizeof(sql));
		sprintf(sql,"select type from Camera where id = %d",CameraId);
		if (!bsrsql.query(sql,res1,nRow))
		{
			LOG_DEBUG(THREAD,"check failed, %s",sql);
			continue;
		}

		if (nRow == 1)
		{
			mysqlpp::Row row = res1.fetch_row();
			params["detailType"] = (string)row[(unsigned int)0];												// ����ͷ���ͣ����������ֺ�����8000����9000ϵ��    һ��ΪNULL
		}
		//*************************************************************************
		nRow = 0;
		mysqlpp::Result res2;
		memset(sql,0,sizeof(sql));
		sprintf(sql,"SELECT recordDir from Camera where id = %d",CameraId);
		if (!bsrsql.query(sql,res2,nRow))
		{
			LOG_DEBUG(THREAD,"check failed, %s",sql);
			continue;
		}
		if (nRow == 1)
		{
			mysqlpp::Row row = res2.fetch_row();
			params["recordDir"] = (string)row[(unsigned int)0];											//cameraId��Ӧ��Ƶ����Ŀ¼
		}
		//*************************************************************************

		bool exec_flag = true;
		for(; ;)
		{
			pthread_mutex_lock(&globalXmlRpcMutex);				
			if (!CameraIdToStreamerMap[CameraId]->execute("Streamer.Stream.History.dump",params,result) || CameraIdToStreamerMap[CameraId]->isFault())	//ֱ��streamer��������Ƶ	//��������ӶϿ�,�Զ�����		//XmlRpcClient��������¼�񷽷���execute�����Զ����ӻ��ƣ�
			{
				if (CameraIdToStreamerMap[CameraId]->isConnected())		
				{
					pthread_mutex_unlock(&globalXmlRpcMutex);
					break;
				}
				pthread_mutex_unlock(&globalXmlRpcMutex);

				//������Ͽ����Żش�����ѭ������ֱ��������
				LOG_DEBUG(THREAD,"Streamer was not connected�� try execute again");
				sleep(5);
				if (isOutBackupTime())		//��鵱ǰʱ���Ƿ񳬹�Ǩ��ʱ�䷶Χ			 
				{
					LOG_DEBUG(THREAD,"Its out of Backup time! Need Backup Video Info�� cameraId:%d  starttime:%d  stoptime:%d",CameraId,starttime,stoptime);
					return NULL;
				}
			}
			else
			{
				pthread_mutex_unlock(&globalXmlRpcMutex);
				exec_flag = false;
				break;
			}
		}
		if (exec_flag)		//��־��ʾ����ʧ�ܵ��ļ���Ϣ
		{
			LOG_DEBUG(THREAD,"Video.Archive.startDump  failed!  Need Backup Video Info�� cameraId:%d  starttime:%d  stoptime:%d",CameraId,starttime,stoptime);
			continue;
		}

		result["rxAddr"] = string(VirtualStreamerIP); 
		result["rxPort"] = result["transPort"];

		//****************************************************
		//************������Ƶ��ת��Ϊmp4��ʽ*****************
		//****************************************************
    int sockFd = SocketUtil::createConn(((string)result["rxAddr"]).c_str(),(int)result["rxPort"], SOCK_STREAM, 2);
		if(sockFd < 0)
		{
			LOG_DEBUG(THREAD, "Connect %s:%d failed, err num:%d", ((string)result["rxAddr"]).c_str(),(int)result["rxPort"], errno);
			continue;
		}

		// �����������ý������������˿�
		BSTPHeader bstpHeader;
		bstpHeader.mark = BSTP_HEADER_MARK;
		bstpHeader.type = BSTP_TYPE_COMMAND;
		int cmd = 0x01;
		SocketUtil::sendBuffer(sockFd, (char*)&bstpHeader, sizeof(BSTPHeader), 0);
		SocketUtil::sendBuffer(sockFd, (char*)&cmd, sizeof(int), 0);
		BsrFrame2Mp4_Handle* pConvertHandle = NULL;

		bool first = true;
		int needWrite = -1; // -1�ǳ�ʼ״̬��0��ʾ���յ���һ����ƵI֡��1��ʾ����дmp4�ļ��ˡ�
		char buffer[2048000];
		char iFramebuffer[2048000];
		bool hasIFrame = false;
		int hasAudio = 0;
		int iWidth = 0,iHeight = 0, iFps = 0;
		int iAudioSampleRate = 8000,iAudioBitrate= 32000;//

		float size_K = 0;				//����Ƶ�ļ���С����λKB
		time_t storeTimeStamp = 0;
		int *if_exit = nasMountStatus;
		++if_exit;
		while(1)
		{
			if (*nasMountStatus == 0)			//����ʧ��
			{
				sleep(100);
				LOG_DEBUG(THREAD,"NAS no mount,goto reback:cameraId:%d  starttime:%d  stoptime:%d",CameraId,starttime,stoptime);
				SocketUtil::safeClose(sockFd);
				goto reback;
			}

			if ((*if_exit) == 1)
			{
        threadpool.CleanTaskQueue();
        threadpool.setExitFlag(pthread_self());
        LOG_DEBUG(THREAD, "threadid Ox%x is exit \n", pthread_self());
        pthread_exit(NULL);
				//return NULL;
			}

			if(first)
			{
				first = false;

				// �� 256�ֽ��ļ�ͷ������
				char fileHeader[256];
				ssize_t rlen = SocketUtil::receiveBuffer(sockFd, fileHeader, 256, 2000);
				if(rlen <= 0)
				{
					break;
				}
			}
			memset(buffer,0x00,sizeof(buffer));
			ssize_t rlen = SocketUtil::receiveBuffer(sockFd, buffer, sizeof(BSTPHeader), 3000);			// �Ƚ���BSTPͷ���ٽ�����Ч����
			BSTPHeader* pBstpHeader = (BSTPHeader*)buffer;
			if(rlen == sizeof(BSTPHeader) && pBstpHeader->mark == BSTP_HEADER_MARK
				&& pBstpHeader->length < 0x001F3000) // ��ЧBSTPͷ���ҳ���ֵ����
			{
				if ((rlen = SocketUtil::receiveBuffer(sockFd, buffer+sizeof(BSTPHeader), pBstpHeader->length, 2000) ) > 0)
					size_K += (float)rlen /1024.0;						//�ۼƽ�������---ת��ǰ��¼���С

				if(rlen == pBstpHeader->length)
				{
					if(needWrite == -1)
					{
						if (pBstpHeader->type == BSTP_TYPE_AUDIO)  // ��ʾ����Ƶ					��Ƶ֡���ݶ���С 324Byte
						{
							hasAudio = 1;
							iAudioSampleRate = pBstpHeader->format[2];//��Ƶ������
							iAudioBitrate = (pBstpHeader->format[3] & 0xF0) >> 4; // ��ȡ��Ƶ����
							if (hasIFrame)
							{
								pConvertHandle = Open_BsrFrame2Mp4(iWidth, iHeight, iFps, hasAudio,
									const_cast<char*>(saveFileName.c_str()), 300, iAudioSampleRate, iAudioBitrate); // ��ʱд��300��
								if (pConvertHandle == NULL)
								{
									LOG_DEBUG(THREAD, "Open_BsrFrame2Mp4 failed, can't create %s, record number %d, Please Check your Network,or Videos, or RecordDB files", saveFileName.c_str(), k);
									break;
								}
							
								needWrite = 1;
								Convert_BsrFrame2Mp4(pConvertHandle, iFramebuffer);//�����I֡
								Convert_BsrFrame2Mp4(pConvertHandle, buffer);			
							}
						}
						else if(IS_BSTP_IFRAME(*pBstpHeader))	// ��һ��д�ļ�ʱ����������ƵI֡��		
						{
							getVideoResolution(pBstpHeader->format[1], &iWidth, &iHeight);
							iFps = pBstpHeader->format[2];
							hasIFrame = true;
							memset(iFramebuffer, 0, sizeof(iFramebuffer));
							memcpy(iFramebuffer, buffer, sizeof(buffer));
						}
					}
					else if (needWrite == 1)
					{
						// ��ʾ����Ƶ֡���֣����ڳ�ʼ����ʱ��û�м�����Ƶ��־������ֻ�ܶ�����Ƶ֡�ˡ�		
						if(pBstpHeader->type == BSTP_TYPE_AUDIO && hasAudio == 0)												//������Open_BsrFrame2Mp4ʱ�����hasAudioΪ0����������Ե����յ������д�����Ƶ֡ʱ������������
						{								
							//LOG_DEBUG(THREAD,"first data  Audio = 0 %d",pBstpHeader->timeStamp);
							continue;
						}

						// ��һ��BSFP֡ת����MP4֡����д�뵽�ļ���. �ļ�����Open_BsrFrame2Mp4()����ʱָ���á�
						// buffer��һ��BSFP֡�����ȼ�¼��֡ͷ�е�length
						int rs = Convert_BsrFrame2Mp4(pConvertHandle, buffer);																			
						if(rs != 0)
						{
							//LOG_DEBUG(THREAD, "Convert_BsrFrame2Mp4 failed. type:%d", pBstpHeader->type);
						}
					}
				}

				storeTimeStamp = pBstpHeader->timeStamp;				//����socket�쳣�Ͽ����
				if(rlen <= 0 || pBstpHeader->timeStamp > (int)stoptime)
				{
					LOG_DEBUG(THREAD, "rlen:%d  Stop dumping %s, video size %f MB", rlen,saveFileName.c_str(),size_K/1024);
					break;
				}
			}
			else if(rlen == -1)			// ͨ����ʾstreamer�ѶϿ��ûỰ:
			{
				if (pBstpHeader->timeStamp >0																																							// �쳣���û�н���goto 
					&& pBstpHeader->timeStamp < (int)stoptime -120  
					&& rebackCounts-- >0)			//���췶Χ2������Ϊstreamer�����쳣����������ʧ�ܣ���������streamer,�����±��ݵ�ǰ�ļ�,�����ļ����rebackup3��
				{
					SocketUtil::safeClose(sockFd);
					if (pConvertHandle != NULL)
					{
						Close_BsrFrame2Mp4(pConvertHandle);
						pConvertHandle = NULL;
					}
					LOG_DEBUG(THREAD,"backup Video size except,goto reback:cameraId:%d  starttime:%d  stoptime:%d",CameraId,starttime,stoptime);
					goto reback;				
				}
				else
				{
					LOG_DEBUG(THREAD, "rlen:%d  Receiving %s data end, video size %f MB", rlen, saveFileName.c_str(),size_K/1024);
					break;
				}
			}
			else
			{
				//��ȡBSTPHeaderͷ������
			}
		}
		SocketUtil::safeClose(sockFd); // �ر���streamer¼�����ػỰ
		if(pConvertHandle == NULL)		//20171115 ����bug pConvertHangdleΪ��   ��Ϊ����¼���쳣�����Ŀ��ļ���ȡ0���ַ�
		{
			LOG_DEBUG(THREAD,"pConvertHandle is NULL! video backup failed!  CameraId:%d,recordId:%d, time period:%d--%d",CameraId,k+1,starttime,stoptime);
			continue;
		}
		Close_BsrFrame2Mp4(pConvertHandle);


		nRow = 0;
		mysqlpp::Result res3;
		sprintf(sql, "select count(*) from BackupInfo where cameraId=%d and recordId=%d", CameraId, k);
		if (!bsrsql.query(sql,res3,nRow))
			LOG_DEBUG(THREAD, "record backup video failed:%s", sql);
		
		mysqlpp::Row rowsnum = res3.fetch_row();
		if ((int)rowsnum[(unsigned int)0] == 0)		//����û�и�Ǩ���ļ���Ϣʱ��д�����ݿ��BackupInfo
		{
			memset(sql, 0, sizeof(sql));
			sprintf(sql, "insert into BackupInfo values(%d,%d)", CameraId, k);
			if (!bsrsql.execute(string(sql), &nRow))					
				LOG_DEBUG(THREAD, "record backup video failed:%s", sql);
		}

		//����ļ���������鱾��Ǩ�ƹ����Ƿ����
		if (gainDBRecordsToCameras() == videoRecordCounts)						
			LOG_DEBUG(THREAD,"===================backupVideo work is over!!===================");

	}
	return NULL;
}

//Ǩ����Ƶ����
static void* backupProcess()
{
	char sql[150];
	sprintf(sql,"select port from Streamer where ip='%s' order by id",VirtualStreamerIP);					 
	mysqlpp::Result result; 
	int nRows = 0;

	for(; ;)
  {
		if (bsrsql.query(sql,result,nRows))  break;

		if (bsrsql.Ping() == 0)																							 
			LOG_DEBUG(NOTHREAD,"sql bsrsql.query failed:%s",sql); 
		else
			LOG_DEBUG(NOTHREAD,"sql connection with mysql  is error"); 
		sleep(5);
	}
	map<int,XmlRpcClient*> PortToXmlRpcMap;		//�˿ں�streamer����ָ��ӳ��
	for( int i=0; i<nRows; ++i)
	{
		mysqlpp::Row row = result.fetch_row();
		int port = (int)row[(unsigned int)0];
		globalXmlRpcClient[i] = new XmlRpcClient(VirtualStreamerIP, port);
		globalXmlRpcClient[i]->setWaitTimeout(5);

		PortToXmlRpcMap[port] = globalXmlRpcClient[i];
	}

	threadpool.SetTaskFuntion(startTransRecord);
	threadpool.Pthreadinit(threadcounts);	                  

	for (; ;)	 
	{
		int times = 30;		
		//while (times-- >0)	//ÿ5���Ӽ��һ��  ��֤����Ǩ��ʱ��Сʱ��ǰ5���ӾͿ�ʼ����
			sleep(10);								

		if (isOutBackupTime())					
		{
			int nRow = 0;
			char sql[200] = {'\0'};
			sprintf(sql,"delete from BackupInfo where cameraId in (SELECT c.id FROM Camera c, Site s, Streamer st WHERE c.site=s.id AND s.streamer=st.id AND st.ip='%s')",VirtualStreamerIP);			//��ձ���������Ӧ��Camera��Ϣ
			if (!bsrsql.execute(string(sql),&nRow))
				LOG_DEBUG(NOTHREAD,"delete from BackupInfo failed,%s",sql);
			continue;
		}

		if(strlen(VirtualStreamerIP) < 8)
			continue;
	
		vector<int> CameraIds;					//��ȡ��������CameraId
		for(; ;)		
		{
			if (getCamerasInStreamer(VirtualStreamerIP, CameraIds,PortToXmlRpcMap) || isOutBackupTime())				
				break;

			sleep(10);
			LOG_DEBUG(NOTHREAD,"getCamerasInstreamer failed");
		}

		//��ȡ��ǰҪ���ݵ���Ƶ�ļ�����
		videoRecordCounts = 0;
		for (int i=0; i<CameraIds.size(); ++i)
		{
			XmlRpcValue resultRecords;
			if (gainCameraRecords(CameraIds[i],resultRecords))
				videoRecordCounts += resultRecords["records"].size();
		}

		if (videoRecordCounts == gainDBRecordsToCameras() || videoRecordCounts == 0)				//������ϻ�û����Ҫ���ݵ��ļ�
			continue;

		LOG_DEBUG(NOTHREAD, "=========================================BackupVideo time");
		LOG_DEBUG(NOTHREAD, "CameraId size is %d  in streamer  %s-------------backupRecords number is %d", CameraIds.size(), VirtualStreamerIP, videoRecordCounts);

		//��Ǩ��������ӵ��̹߳�������
		vector<int*> taskQueue;
		for (int i=0; i<CameraIds.size(); ++i)							
			taskQueue.push_back(new int(CameraIds[i]));
		threadpool.Addwork(taskQueue);			

		for(; ;)
		{
			sleep(5*60);
			if (isOutBackupTime())
			{
				LOG_DEBUG(NOTHREAD,"it's out of time");
				break;
			}

			if (threadpool.GetTaskQueueNum() == 0)			//����¼���Ǩ֮ǰ�ı���¼�񱸷����  �������߳�û����������
				break;				
		}
	}

	delete[] globalXmlRpcClient;
	return NULL;
} 

static void waitChildPid(int sig)
{
	static int seconds = 0;
	pid_t childpid;
	while( (childpid = waitpid(-1, NULL, WNOHANG)) > 0)
	{  
		LOG_DEBUG(NOTHREAD,"exit pid is %d",childpid);
		int *tmp = nasMountStatus;
		if (*(++tmp) == 1)
		{
			LOG_DEBUG(NOTHREAD,"main process exit");
			exit(0);
		}

		if (childpid == backupPid)
		{
			pid_t pid = fork();
			backupPid = pid;
			switch (pid)
			{
			case -1:
				LOG_DEBUG(NOTHREAD,"The fork create backupPid process failed");
				_exit(-1);
			case 0:
				backupProcess();      
				_exit(-1);
			default:
				LOG_DEBUG(NOTHREAD,"The fork create backupPid process pid = %d",backupPid);
				break;
			}
		}

		sleep(1); 
	}
}

static void sigKillHandler(int sig)
{
	LOG_DEBUG(NOTHREAD,"process %d recive killall signal",getpid());

	if (backupPid == 0)					
	{
    int* tmp = nasMountStatus;
    *(++tmp) = 1;
		threadpool.DestroyPthread();
    while (!threadpool.IsDestroied())
      usleep(100000);

		LOG_DEBUG(NOTHREAD,"child process %d exit",getpid());
		exit(-1);
	}
	else
	{
		LOG_DEBUG(NOTHREAD,"main process %d wait",getpid());
		return;
	}
}

int main(int argc, char* argv[])
{
	//��־Ŀ¼���ļ���ǰ׺
	char loginfo[50] = {'\0'};
	if(getFileCfg("LogDirAndName",loginfo))			//����������־
		return -1;
	LOG::get_instance()->init(loginfo, 512,1024*1024*2*2);  

	//admind �����ӵ�ַ
	if(getFileCfg("relIp",relIp))
	{
		LOG_DEBUG(NOTHREAD,"get realIp failed");
		return -1;
	} 

	//����Ǩ����ƵĿ¼
	if (getFileCfg("saveFilePath",saveFilePath))
	{
		LOG_DEBUG(NOTHREAD,"get saveFilePath failed");
		return -1;
	}

	//��������IP����ȡ������Ӧ��CameraId�����ݿ��Streamer�д��Streamer����IP
	if (getFileCfg("VirtualStreamerIp", VirtualStreamerIP))			
	{
		LOG_DEBUG(NOTHREAD,"get saveFilePath failed");
		return -1;
	} 

	//��ʱǨ����Ƶ��ʼʱ�� 
	char tmpRecordStartTime[10] = {'\0'};
	if (getFileCfg("transRecordStartTime",tmpRecordStartTime))
	{
		LOG_DEBUG(NOTHREAD,"get transRecordStartTime failed");
		return -1;
	}
	transRecordStartTime = atoi(strtok(tmpRecordStartTime,":"));	

	//��ʱǨ����Ƶ����ʱ��
	char tmpRecordStopTime[10] = {'\0'};
	if (getFileCfg("transRecordStopTime",tmpRecordStopTime))
	{
		LOG_DEBUG(NOTHREAD,"get transRecordStopTime failed");
		return -1;
	}
	transRecordStopTime = atoi(strtok(tmpRecordStopTime,":"));			 

	//Ǩ����Ƶ�Ŀ�ʼʱ��
	char tmptransRecordStartTime[10] = {'\0'};
	if (getFileCfg("RecordStartTime",tmptransRecordStartTime))
	{
		LOG_DEBUG(NOTHREAD,"get RecordStartTime failed");
		return -1;
	}
	RecordStartTime.time_hour = atoi(strtok(tmptransRecordStartTime,":"));
	RecordStartTime.time_min = atoi(strtok(NULL,":"));
	RecordStartTime.time_sec = atoi(strtok(NULL,":"));

	//Ǩ����Ƶ�Ľ���ʱ��
	char tmptransRecordStopTime[10] = {'\0'};
	if (getFileCfg("RecordStopTime",tmptransRecordStopTime))
	{
		LOG_DEBUG(NOTHREAD,"get RecordStopTime failed");
		return -1;
	}
	RecordStopTime.time_hour = atoi(strtok(tmptransRecordStopTime,":"));
	RecordStopTime.time_min = atoi(strtok(NULL,":"));
	RecordStopTime.time_sec = atoi(strtok(NULL,":"));

	//Ǩ����Ƶ�߳�����
	char tmpthreadcounts[5]={'\0'};
	if(getFileCfg("threadcounts",tmpthreadcounts))
	{
		LOG_DEBUG(NOTHREAD,"get threadcounts failed");
		return -1;
	}
	threadcounts = atoi(tmpthreadcounts);

	//��ȡ���ݿ�������Ϣ
	MysqlParams sqlparas;			 
	if (!(getFileCfg("ServerAddr",sqlparas.serverAddr) == 0 
		&& getFileCfg("database",sqlparas.database) == 0 
		&& getFileCfg("use",sqlparas.username) == 0 
		&& getFileCfg("password",sqlparas.password) == 0))
	{
		LOG_DEBUG(NOTHREAD,"get db information failed");
		return false;
	}

	//������Ŀ¼�Ƿ����
	if (access(saveFilePath,F_OK))				
	{
		LOG_DEBUG(NOTHREAD,"saveFilePath is not exist");
		return -1;	
	}
	//������Ŀ¼�Ƿ����д
	if (access(saveFilePath,W_OK))
	{
		LOG_DEBUG(NOTHREAD,"saveFilePath cannot write in");
		return -1;
	}

	//�Ի�ȡ�������ļ��������ж�
	if (transRecordStartTime==transRecordStopTime
		|| transRecordStartTime<0
		|| transRecordStartTime>23
		|| transRecordStopTime<0
		|| transRecordStopTime>23
		|| threadcounts<0
		|| threadcounts>50
		|| RecordStartTime.time_hour<0
		|| RecordStartTime.time_hour>23
		|| RecordStartTime.time_min<0
		|| RecordStartTime.time_min>59
		|| RecordStartTime.time_sec<0
		|| RecordStartTime.time_sec>59
		|| RecordStopTime.time_hour<0
		|| RecordStopTime.time_hour>23
		|| RecordStopTime.time_min<0
		|| RecordStopTime.time_min>59
		|| RecordStopTime.time_sec<0
		|| RecordStopTime.time_sec>59)
	{
		LOG_DEBUG(NOTHREAD,"==================get Config file information failed!====================");
		return -1;
	}

	//�������nas���صĹ����ڴ�
	key_t kid;
	if((kid = ftok(saveFilePath, 1)) == -1)
	{
		LOG_DEBUG(NOTHREAD,"ftok kid failed");
		return -1;
	}
	int sharedMemId = -1;
	if((sharedMemId = shmget(sharedMemId,sizeof(int)*2,0666|IPC_CREAT) ) < 0)
	{
		LOG_DEBUG(NOTHREAD, "creat shared memory fail");
		return -1;
	}
	else
		LOG_DEBUG(NOTHREAD, "create share memory id:%d",sharedMemId);

	nasMountStatus = (int*)shmat(sharedMemId,(const void*)0,0);		
	int *tmp = nasMountStatus;
	*tmp = 0;
	*(++tmp) = 0;

	//���nas����
	if (!checkNasMount()) 
	{
		sendBackupAlarm(MOUNT_DISK_ERR);
		return -1;
	}
	signal(SIGCHLD,waitChildPid);
	signal(SIGSEGV,SystemErrorHandler); //Invaild memory address   
	signal(SIGABRT,SystemErrorHandler); // Abort signal   SystemErrorHandler
	signal(SIGTERM,sigKillHandler);		 

	if (!loginDB(sqlparas))						
		return -1;

	pid_t pid = fork();   
	backupPid = pid;
	switch (pid)
	{
	case -1:
		LOG_DEBUG(NOTHREAD,"The fork create backupPid process failed");
		_exit(-1);
	case 0:

		backupProcess();
		_exit(-1);
	default:
		LOG_DEBUG(NOTHREAD,"The fork create backupPid process pid = %d",backupPid);
		break;
	}

	for(; ;)
	{
		sleep(5*60);					 
		if (!checkNasMount()) 
			sendBackupAlarm(MOUNT_DISK_ERR);
	}
  sleep(300);
	shmctl(sharedMemId,IPC_RMID,0);
	return 0;
}

