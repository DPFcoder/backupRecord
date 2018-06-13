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
  XmlRpcClient* globalXmlRpcClient[10];		//每个Streamer服务器对应一个XmlRpcClient
  static map<int, XmlRpcClient*> CameraIdToStreamerMap;
  static BsrSQL bsrsql;
  mysqlpp::Connection con;
  LOG* log;
  static char saveFilePath[100];
  static char VirtualStreamerIP[20];					//直连streamer服务器后，即本机与备机的浮动IP  VirtualStreamerIP
  static int transRecordStartTime;			//开始迁移视频时间
  static int transRecordStopTime;				//停止迁移视频时间
  static transRecordTimeInfo	RecordStartTime;		//视频起点时间
  static transRecordTimeInfo RecordStopTime;		//视频结束时间
  static time_t WorkStartTime;      //当前开始备份标准时间
  static time_t WorkStopTime;       //当前结束备份标准时间
  static int videoRecordCounts;		//每次备份的视频文件数量
  static pthread_mutex_t globalXmlRpcMutex;
  PthreadPool threadpool;
  int threadcounts;
  Alarm backupAlarm;
};

#endif