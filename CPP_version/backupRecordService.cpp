#include<execinfo.h>   
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include "GoonBackup.h"
#include "BackupRecord.h"
//�����߳���־flag
#define THREAD 1
#define NOTHREAD 0


int backupPid;
int goonBackupPid;
int*  nasMountStatus;	  //0 ��ʶnas ����ʧ��  1 ��ʶnas ���سɹ�     
char saveFilePath[100] = { '\0' };
char VirtualStreamerIP[20] = { '\0' };					//ֱ��streamer�������󣬼������뱸���ĸ���IP  VirtualStreamerIP
char relIp[16];

void SystemErrorHandler(int signum)
{
  const int len = 1024;
  void *func[len];
  size_t size;
  int i;
  char **funs;

  signal(signum, SIG_DFL);
  size = backtrace(func, len);
  funs = (char**)backtrace_symbols(func, size);
  LOG_DEBUG(NOTHREAD, "System error,Stack trace:----------");
  for (i = 0; i < size; ++i)
    LOG_DEBUG(NOTHREAD, "---------%d %s \n", i, funs[i]);
  free(funs);
}

void sendBackupAlarm(unsigned long alarmType)
{
  ALARM_INFO alarmInfo;
  memset(&alarmInfo, 0, sizeof(alarmInfo));
  alarmInfo.myip = (unsigned long)inet_addr(VirtualStreamerIP);
  alarmInfo.event = alarmType;
  alarmInfo.which = 0;
  alarmInfo.time = time(NULL);
  Alarm backupAlarm;
  backupAlarm.sendAlarm(alarmInfo, relIp, 2134);
  LOG_DEBUG(THREAD, "Send Alarm!!!");
  sleep(1);
}

//�жϱ���Ŀ¼�Ƿ�����ڸ�Ŀ¼
bool IsMountOnRootDev(void)
{
  FILE *p = NULL;
  string rootDev;
  string mountDev;
  bool checkRootDev = false;
  bool checkMountDev = false;
  char buf[256] = { 0, };
  string s = "/etc/mtab";

  do {
    p = fopen(s.c_str(), "r");
    if (p == NULL) {
      break;
    }
    while (fgets(buf, sizeof(buf), p) != NULL) {
      char bufdev[128] = { 0, };
      char bufkey[128] = { 0, };
      char bufothrer[128] = { 0, };
      sscanf(buf, "%s %s %s", (char*)&bufdev, (char*)&bufkey, (char*)bufothrer);
      if (strcpy(bufkey, "") == 0 || strcpy(bufdev, "") == 0) {
        continue;
      }
      if (!strcmp(bufkey, "/"))
      {
        rootDev = bufdev;
        checkRootDev = true;
      }
      else if (!strcmp(bufkey, saveFilePath))
      {
        mountDev = bufdev;
        checkMountDev = true;

      }

      if (checkRootDev && checkMountDev)
        break;
    }
  } while (0);

  if (p) {
    fclose(p);
    p = NULL;
  }

  if (!rootDev.empty() && !strcmp(rootDev.c_str(), mountDev.c_str()))
    return true;
  else
    return false;
}

//���nas����
bool checkNasMount()
{
  string checknas = "df -h | grep " + string(saveFilePath);
  int status = -1;
  for (int i = 0; ; ++i)
  {
    if ((status = system(checknas.c_str())) != -1)
      break;

    LOG_DEBUG(NOTHREAD, "check nas mount failed!");
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
      LOG_DEBUG(NOTHREAD, "============Nas mounted failed!! Please check Nas mount status===============");
      break;
    }
    else if (status == 0)
    {
      if (IsMountOnRootDev())
      {
        LOG_DEBUG(NOTHREAD, "============Mount on RootDev !! Please check Nas mount status===============");
        *nasMountStatus = 0;
        break;
      }
      else
      {
        *nasMountStatus = 1;
        return true;
      }
    }
  } while (0);

  return false;
}

//����¼�����
static void* backupProcess()
{
  BackupRecord* backupRecord = BackupRecord::getinstance();
  if (!backupRecord->initBackupRecord(LOG::get_instance(), nasMountStatus, saveFilePath, VirtualStreamerIP, relIp))
    exit(-1);
  backupRecord->work();
}

//����¼�����
static void*goonBackupProcess()
{
  GoonBackup* goonBackup = GoonBackup::getinstance();
  if (!goonBackup->initGoonBackup(LOG::get_instance(), nasMountStatus, saveFilePath, VirtualStreamerIP, relIp))
    exit(-1);
  goonBackup->work();
}

static void waitChildPid(int sig)
{
  static int seconds = 0;
  pid_t childpid;
  while ((childpid = waitpid(-1, NULL, WNOHANG)) > 0)
  {
    LOG_DEBUG(NOTHREAD, "exit pid is %d", childpid);
    if (childpid == backupPid)
    {
      pid_t pid = fork();
      backupPid = pid;
      switch (pid)
      {
      case -1:
        LOG_DEBUG(NOTHREAD, "The fork create backupPid process failed");
        _exit(-1);
      case 0:
        backupProcess();
        _exit(-1);
      default:
        LOG_DEBUG(NOTHREAD, "The fork create backupPid process pid = %d", backupPid);
        break;
      }
    }

    if (childpid == goonBackupPid)
    {
      pid_t pid = fork();
      goonBackupPid = pid;
      switch (goonBackupPid)
      {
      case -1:
        LOG_DEBUG(NOTHREAD, "The fork create go on backupPid process failed");
        _exit(-1);
      case 0:
        goonBackupProcess();
        _exit(-1);
      default:
        LOG_DEBUG(NOTHREAD, "The fork create go on backupPid process pid = %d", goonBackupPid);
        break;
      }
    }
    sleep(1);
  }
}

int main(int argc, char* argv[])
{
  //��־Ŀ¼���ļ���ǰ׺
  char loginfo[50] = { '\0' };
  if (getFileCfg("LogDirAndName", loginfo))
    return -1;
  LOG::get_instance()->init(loginfo, 512, 1024 * 1024 * 2 * 2);

  //admind �����ӵ�ַ
  if (getFileCfg("relIp", relIp))
  {
    LOG_DEBUG(NOTHREAD, "get realIp failed");
    return -1;
  }
  //��������IP����ȡ������Ӧ��CameraId�����ݿ��Streamer�д��Streamer����IP
  if (getFileCfg("VirtualStreamerIp", VirtualStreamerIP))
  {
    LOG_DEBUG(NOTHREAD, "get saveFilePath failed");
    return -1;
  }
  //����Ǩ����ƵĿ¼
  if (getFileCfg("saveFilePath", saveFilePath))
  {
    LOG_DEBUG(NOTHREAD, "get saveFilePath failed");
    return -1;
  }
  //������Ŀ¼�Ƿ����
  if (access(saveFilePath, F_OK))
  {
    LOG_DEBUG(NOTHREAD, "saveFilePath is not exist");
    return -1;
  }
  //������Ŀ¼�Ƿ����д
  if (access(saveFilePath, W_OK))
  {
    LOG_DEBUG(NOTHREAD, "saveFilePath cannot write in");
    return -1;
  }
  //�������nas���صĹ����ڴ�
  key_t kid;
  if ((kid = ftok(saveFilePath, 1)) == -1)
  {
    LOG_DEBUG(NOTHREAD, "ftok kid failed");
    return -1;
  }
  int sharedMemId = -1;
  if ((sharedMemId = shmget(sharedMemId, sizeof(int) * 2, 0666 | IPC_CREAT)) < 0)
  {
    LOG_DEBUG(NOTHREAD, "creat shared memory fail");
    return -1;
  }
  else
    LOG_DEBUG(NOTHREAD, "create share memory id:%d", sharedMemId);

  nasMountStatus = (int*)shmat(sharedMemId, (const void*)0, 0);
  int *tmp = nasMountStatus;
  *tmp = 0;

  if (!checkNasMount())
  {
    sendBackupAlarm(MOUNT_DISK_ERR);
    return -1;
  }
  signal(SIGCHLD, waitChildPid);
  signal(SIGSEGV, SystemErrorHandler); //Invaild memory address   
  signal(SIGABRT, SystemErrorHandler); // Abort signal   SystemErrorHandler

  //���ݷ���
  pid_t pid = fork();
  backupPid = pid;
  switch (pid)
  {
  case -1:
    LOG_DEBUG(NOTHREAD, "The fork create backupPid process failed");
    _exit(-1);
  case 0:
    backupProcess();
    _exit(-1);
  default:
    LOG_DEBUG(NOTHREAD, "The fork create backupPid process pid = %d", backupPid);
    break;
  }

  //����
  pid_t pid1 = fork();
  goonBackupPid = pid1;
  switch (goonBackupPid)
  {
  case -1:
    LOG_DEBUG(NOTHREAD, "The fork create go on backupPid process failed");
    _exit(-1);
  case 0:
    goonBackupProcess();
    _exit(-1);
  default:
    LOG_DEBUG(NOTHREAD, "The fork create go on backupPid process pid = %d", goonBackupPid);
    break;
  }

  for (; ;)
  {
    sleep(5 * 60);
    if (!checkNasMount())
      sendBackupAlarm(MOUNT_DISK_ERR);
  }

  shmctl(sharedMemId, IPC_RMID, 0);
  return 0;
}

