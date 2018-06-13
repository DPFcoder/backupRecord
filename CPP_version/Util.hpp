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
  //http Э�������һ���100~500,Ӧ�ò������֮���ֿ���800��ʼ
  FAILED = -1,
  //�ɹ�
  RET_SUCCESS =0,
  //����ֵ����
  ERR_PARAM_VALUE = 800,
  //������ʽ����
  ERR_PARAM_FORMAT,
  //ʱ�䷶Χ����ȷ
  ERR_TIME_RANGE,
  //�洢Ŀ¼������
  ERR_STOREDIR_NOT_EXIST,
  //�洢�ļ��Ѿ�����
  ERR_STORE_FILE_ALREADY_EXIST,
  //�ڲ�����ִ��ʧ��
  ERR_INNER_FUC_EXECUTE,
  //�ļ�ϵͳ�ռ䲻��
  ERR_DISK_SPACE,
  //�洢Ŀ¼����д
  ERR_STORE_FILE_NO_WRITE,
  //���Ͷ�������
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
  int threadFlag;//��ʼֵĬ�ϴ�ӡ����
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
  char serverAddr[16];  // mysql��ַ
  char username[16];    // �����˺�
  char password[16];    // ��������
  char database[16];    // ���ݿ���
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

