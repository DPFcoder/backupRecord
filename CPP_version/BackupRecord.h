#ifndef BACKUPRECORD_H
#define BACKUPRECORD_H
#include <sys/time.h>
#include "SocketUtil.hpp"
#include "SQL.hpp"				
#include "Util.hpp"
#include <mysql++.h>
#include <map>
#include "XmlRpc.h"
#include "XmlRpcValue.h"
#include "ThreadPool.hpp"
#include "Alarm.hpp"
#include "BSFPDefined.hpp"
#include "mp4aac/convert.h"

#define THREAD 1
#define NOTHREAD 0

using namespace XmlRpc;
using namespace std;

class BackupRecord
{
public:
  static BackupRecord* getinstance();
  bool initBackupRecord(LOG* _log, int* _nasMountStatus, char* _saveFilePath, char* _VirtualStreamerIP, char* _relIp);
  bool loginDB(const MysqlParams& sqlparas);
  bool getCamerasInStreamer(const char * streamerIp, vector<int> &CameraIds, map<int, XmlRpcClient*> porttoxmlrpc);
  static void GetTransRecordTime(time_t& begin, time_t& end);
  static void GetWorkTimeScope(time_t& begin, time_t& end);
  static bool isOutTime();
  static bool gainCameraRecords(const int& cameraId, XmlRpcValue &resultRecords);
  static int gainDBRecordsToCameras();
  static void *startTransRecord(void* cameraId);     //threads safe
  bool registCurDayBackupInfoToResult(vector<int> &CameraIds);
  static void cancelRegistCurDayBackupInfo(int cameraId, int recordId);
  virtual void work();
  void sendBackupAlarm(unsigned long alarmType);
  static void getVideoResolution(int bsfptab, int* pWidth, int* pHeight);

protected:
  BackupRecord();
  virtual ~BackupRecord();

protected:
  char relIp[16];
  MysqlParams sqlparas;
  static int *nasMountStatus;
  XmlRpcClient* globalXmlRpcClient[10];		//ÿ��Streamer��������Ӧһ��XmlRpcClient
  static map<int, XmlRpcClient*> CameraIdToStreamerMap;
  static BsrSQL bsrsql;
  mysqlpp::Connection con;
  LOG* log;
  static char saveFilePath[100];
  static char VirtualStreamerIP[20];					//ֱ��streamer�������󣬼������뱸���ĸ���IP  VirtualStreamerIP
  static int transRecordStartTime;			//��ʼǨ����Ƶʱ��
  static int transRecordStopTime;				//ֹͣǨ����Ƶʱ��
  static transRecordTimeInfo	RecordStartTime;		//��Ƶ���ʱ��
  static transRecordTimeInfo RecordStopTime;		//��Ƶ����ʱ��
  static time_t WorkStartTime;      //��ǰ��ʼ���ݱ�׼ʱ��
  static time_t WorkStopTime;       //��ǰ�������ݱ�׼ʱ��
  static int videoRecordCounts;		//ÿ�α��ݵ���Ƶ�ļ�����
  static pthread_mutex_t globalXmlRpcMutex;
  PthreadPool threadpool;
  int threadcounts;
  Alarm backupAlarm;
};

#endif