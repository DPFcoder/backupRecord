#ifndef UTIL_SOAP_HPP
#define UTIL_SOAP_HPP
#define maxLen 1024

#include <sys/ipc.h>
#include <sys/msg.h>
#include <iconv.h>
#include <string.h>
#include <sys/statvfs.h>  // statfs()
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/time.h>
enum AgentErrEnum
{
  //http 协议错误码一般从100~500,应用层错误与之区分开从800开始
  FAILED = -1,
  //成功
  RET_SUCCESS =0,
  //参数值错误
  ERR_PARAM_VALUE = 800,
  //参数格式错误
  ERR_PARAM_FORMAT,
  //时间范围不正确
  ERR_TIME_RANGE,
  //存储目录不存在
  ERR_STOREDIR_NOT_EXIST,
  //存储文件已经存在
  ERR_STORE_FILE_ALREADY_EXIST,
  //内部函数执行失败
  ERR_INNER_FUC_EXECUTE,
  //文件系统空间不足
  ERR_DISK_SPACE,
  //存储目录不可写
  ERR_STORE_FILE_NO_WRITE,
  //发送队列已满
  ERR_SENDMSGQ_IS_FULL
};
int Log(const char *logFileNamePrefix,int threadFlag,char *fmt,...);
#define debugPrint(fileNamePrefix,threadFlag,fmt,...) { \
  char fmt_buff[2048]; \
  sprintf(fmt_buff,"%s LINE:%d %s", __FILE__,__LINE__,fmt);\
  Log(fileNamePrefix,threadFlag,fmt_buff,##__VA_ARGS__);}
class LOG
{
public:
  static LOG* get_instance()
  {
    static LOG instance;
    return &instance;
  }
  bool init(const char*file_name_prefix,int log_buff_size = 8192,int m_split_lines = 5000000);
  void writeLog(int level,int ithreadFlag,const char *format,...);
  void flush(void);
private:
  LOG();
  virtual ~LOG();
private:
  pthread_mutex_t *m_mutex;
  char dir_name[128];
  char log_name[128];
  int m_split_lines;
  int m_log_buf_size;
  long long m_count;
  int threadFlag;//初始值默认打印进程
  int m_today;
  FILE *m_fp;
  char *m_buf;
};
#define LOG_DEBUG(threadFlag,format, ...) LOG::get_instance()->writeLog(0,threadFlag,format,##__VA_ARGS__)   
#define LOG_INFO(threadFlag,format, ...) LOG::get_instance()->writeLog(1,threadFlag,format,##__VA_ARGS__)   
#define LOG_WARN(threadFlag,format, ...) LOG::get_instance()->writeLog(2,threadFlag,format,##__VA_ARGS__)   
#define LOG_ERROR(threadFlag,format, ...) LOG::get_instance()->writeLog(3,threadFlag,format,##__VA_ARGS__) 
  


void getTimeYYYYMMDD(char *timep);

bool checkDiskSpace(const char *fspath);
int getFileCfg(char *keyName,char *value,char* filePath = NULL);

struct MysqlParams
{
  char serverAddr[16];  // mysql地址
  char username[16];    // 访问账号
  char password[16];    // 访问密码
  char database[16];    // 数据库名
};

struct transRecordTimeInfo
{
	int time_hour;
	int time_min;
	int time_sec;
	transRecordTimeInfo():time_hour(0),time_min(0),time_sec(0) {}
};

#include <sstream>
template<typename T> 
std::string toStr(T var) 
{
  std::stringstream ss; 
  ss << var; 
  return ss.str();
};

typedef struct pthreadInfo
{
	pthread_t pthreadid;
	int status;
}pthreadInfo;
#endif

