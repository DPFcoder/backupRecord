#ifndef GOONBACKUP_H
#define GOONBACKUP_H
//not threads safe   no mutex

#include <vector>
#include <string>
#include "BackupRecord.h"

typedef struct backupInfo
{
  string date;
  int cameraId;
  int recordId;
  int times;
  int result;
}backupInfo;
typedef vector<backupInfo> BackupTask;
typedef vector<backupInfo>::iterator task_it;

class GoonBackup:public BackupRecord
{
public:
  bool initGoonBackup(LOG* _log, int* _nasMountStatus, char* _saveFilePath, char* _VirtualStreamerIP, char* _relIp);
  bool isOutTime();
  void addTask(BackupTask & task);
  int GetGoonBackTimes();
  bool queryGoonBackupTasks(BackupTask & task, int times);
  void *startTransRecord(backupInfo& info);
  bool gainCameraRecords(const int& cameraId, XmlRpcValue &resultRecords);
  task_it begin();
  task_it end();
  void clear();
  virtual void work();

  XmlRpcClient* rpc;
  static GoonBackup* getinstance();
private:
  GoonBackup();
  virtual ~GoonBackup();

private:
  int GoonBackStartTime;		    	//go on backup start time
  int GoonBackStopTime;				    //go on backup stop time
  int GoonBackTimes;              //go on backup times
  BackupTask backupTask;          //go on backup task information
};

#endif