#include "Util.hpp"


void getTimeYYYYMMDD(char *timep)
{
  time_t  clock;
  struct tm *tm1;
  char	 tmp[8+1];

  time(&clock);
  tm1=localtime(&clock);

  memset(tmp, 0, sizeof(tmp));
  sprintf(tmp,"%04d%02d%02d",tm1->tm_year+1900,tm1->tm_mon+1,tm1->tm_mday);

  strncpy(timep,tmp,8);
}
/*
* @fn  Log(const char *logFileNamePrefix，...
* @brief 日志输出模块
* @param[in] logFileNamePrefix 文件名前缀.（实际输出文件名为 logFileNamePrefix.YYYYMMDD）
* @param[in] threadFlag 打印进程线程id标识.（0 进程，1 线程+进程）
* @return   int
* @retval 0: 成功.
* @retval !0: 失败. 
*/
int Log(const char *logFileNamePrefix,int threadFlag,char *fmt,...)
{
  time_t		now_time;/*记录系统的当前时间*/
  struct tm		*clock;
  char 			logfile_pathName[90];
  char 			tmp_pathName[90];
  char	 		time_str[18];
  char			time_tmp[8+1];
  struct stat 	fileStat;

  FILE 			*fp;
  va_list 		varArg;
  char 			buf[81920];

  int 			rt_value=0;

  memset(time_tmp,0,sizeof(time_tmp));
  memset(logfile_pathName,0,sizeof(logfile_pathName));
  memset(tmp_pathName,0,sizeof(tmp_pathName));
  memset(time_str,0,sizeof(time_str));
  memset(buf,0,sizeof(buf));
  memset(&fileStat,0,sizeof(struct stat));

  /*get  filename & filepath*/
  sprintf(logfile_pathName, "/tmp/%s",logFileNamePrefix);
  getTimeYYYYMMDD(time_tmp);

  sprintf(tmp_pathName,"%s.%s",logfile_pathName,time_tmp);

  fp = fopen(tmp_pathName,"a+");

  /*get the time now*/
  time(&now_time);
  clock = localtime(&now_time);	
  sprintf(time_str, "%02d/%02d/%02d %02d:%02d:%02d",
    clock->tm_year + 1900 - 2000, clock->tm_mon + 1, clock->tm_mday,
    clock->tm_hour, clock->tm_min, clock->tm_sec);

  va_start(varArg,fmt);
  //vsprintf(buf, fmt, varArg);
  vsnprintf(buf, sizeof(buf), fmt, varArg);
  va_end(varArg);
  switch (threadFlag)
  { 
  case 0:
    fprintf(fp,"%s pid %d: %s\n",time_str,getpid(),buf);
    break;
  case 1:
    fprintf(fp,"%s pid %d threadid %lu: %s\n",time_str,getpid(),pthread_self(),buf);
    break;
  default:
    break;
  }
  fclose(fp);

  return 0;
}

LOG::LOG() :
  m_split_lines(0),
  m_log_buf_size(0),
  threadFlag(0),
  m_today(0),
  m_fp(NULL),
  m_buf(NULL)
{
  m_count = 0;
  m_mutex = new pthread_mutex_t;
  pthread_mutex_init(m_mutex, NULL);
}

LOG::~LOG()  
{  
  if(m_fp != NULL)  
  {  
    fclose(m_fp);  
  }  
  pthread_mutex_destroy(m_mutex);  

  if(m_mutex != NULL)  
  {  
    delete m_mutex;  
  }  
}  
/*
* @fn  LOG::init(const char* file_name, int log_buf_size, int split_lines)  
* @brief 日志输出模块init方法说明
* @param[in] file_name 路径+文件名前缀.（实际输出文件名为 文件名前缀.YYYYMMDD）
* @param[in] log_buf_size 日志输出buff大小
* @param[in] split_lines 日志文件的多大行数
* @return   int
* @retval true: 成功.
* @retval !0: 失败. 
*/
bool LOG::init(const char* file_name, int log_buf_size, int split_lines)  
{  
  threadFlag = 0;
  m_log_buf_size = log_buf_size;  
  m_buf = new char[m_log_buf_size];  
	memset(m_buf, '\0', m_log_buf_size);  
  m_split_lines = split_lines;  

  time_t t = time(NULL);  
  struct tm* sys_tm = localtime(&t);  
  struct tm my_tm = *sys_tm;  
  char *p = strrchr(file_name, '/');  
  char log_full_name[256] = {0};  
  if(p == NULL)  
  {  
    snprintf(log_full_name, 255, "%d_%02d_%02d_%s",my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday, file_name);     
  }  
  else  
  {  
    strcpy(log_name, p+1);  
    strncpy(dir_name, file_name, p - file_name + 1);  
    snprintf(log_full_name, 255, "%s%s.%d%02d%02d",dir_name,log_name,my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday  );   
  }  

  m_today = my_tm.tm_mday;  

  m_fp = fopen(log_full_name, "a");  
  if(m_fp == NULL)  
  {  
    return false;  
  }  

  return true;  
}  

void LOG::writeLog(int level,int ithreadFlag, const char* format, ...)  
{  
  int threadFlag_t = ithreadFlag;
  struct timeval now = {0,0};  
  gettimeofday(&now, NULL);  
  time_t t = now.tv_sec;  
  struct tm* sys_tm = localtime(&t);  
  struct tm my_tm = *sys_tm;  
  char s[16] = {0};  
  switch(level)  
  {  
  case 0 : strcpy(s, "[debug]:"); break;  
  case 1 : strcpy(s, "[info]:"); break;  
  case 2 : strcpy(s, "[warn]:"); break;  
  case 3 : strcpy(s, "[erro]:"); break;  
  default:  
    strcpy(s, "[info]:"); break;  
  }  
  int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06d %s ",  
    my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday,  
    my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);  

  pthread_mutex_lock(m_mutex);  
  m_count++;			//没有同步
  if(m_today != my_tm.tm_mday || m_count > m_split_lines) //everyday log    大于指定行则重写该文件
  {  
    char new_log[256] = {0};  
    fflush(m_fp);  
    fclose(m_fp);  
    char tail[16] = {0};  
    snprintf(tail, 16,  "%d%02d%02d", my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday);  
    if(m_today != my_tm.tm_mday)  
    {  
      snprintf(new_log, 255, "%s%s.%s", dir_name,log_name,tail);  
      m_today = my_tm.tm_mday;  
      m_count = 0;  
    }  
    else  
    {  
      snprintf(new_log, 255, "%s%s%s.%d", dir_name,log_name,tail,m_count/m_split_lines);  
    }  
    m_fp = fopen(new_log, "a");  
  }  

  va_list valst;  
  va_start(valst, format);  
  
  int m =vsnprintf(m_buf + n, m_log_buf_size-1, format, valst);  
  m_buf[n + m + 1] = '\n';

  switch (threadFlag_t)
  { 
  case 0:
    fprintf(m_fp,"pid %d: %s\n",getpid(),m_buf);
    break;
  case 1:
    fprintf(m_fp,"pid %d threadid %lu: %s\n",getpid(),pthread_self(),m_buf);
    break;
  default:
    break;
  }  
  pthread_mutex_unlock(m_mutex);  
  va_end(valst);
  fflush(m_fp);  
    
}  

void LOG::flush(void)  
{  
  pthread_mutex_lock(m_mutex);  
  fflush(m_fp);  
  pthread_mutex_unlock(m_mutex);  
}  

/**
 * @fn getFileCfg(char *keyName,char *value)
 * @brief 获取配置文件信息
 * @param[in] keyName 配置文件的key.
 * @param[in] value   返回取到的keyName对应的值buff
 * @return    是否获取成功
 * @retval 0: 成功.
 * @retval !0: 失败. 
 */
int getFileCfg(char *keyName,char *value,char* filePath)
{
  FILE *pFile = NULL;
  char *key=NULL,*valuep = NULL;
  char *p = NULL;
  char lineBuff[256];
	char fileName[256];
	if (filePath == NULL)
	{
		strcpy(fileName, "./backupRecord.ini");
	}
	else
	{
		strcpy(fileName,filePath);
	}
  
  pFile = fopen(fileName,"r");
  if ( pFile == NULL )
  {
    fprintf(stderr,"the %s open fail\n",fileName);
    return -1;
  }
  int keylen = strlen(keyName);
  while (fgets(lineBuff,256,pFile)!=NULL)
  {    
    if (*lineBuff == '#' || *lineBuff == ';' || *lineBuff == '['|| *lineBuff == '\n' || *lineBuff == '\r')
      continue;
    for (key = lineBuff;*key == ' ' || *key == '\t'; ++key)
      ;
    if ( 0 == strncmp(key, keyName,keylen))
    {
      for (valuep = key + keylen; *valuep == ' ' || *valuep == '\t';++valuep)
        ;
      if(*valuep == '=')
      {
        for(valuep = valuep + 1; *valuep == ' ' || *valuep == '\t'; ++ valuep) //去掉=与value间的空格或tab
          ;
        for (p = lineBuff + strlen(lineBuff) - 1;p >= valuep && (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t');--p)
          ;
        if (p - valuep >= 0 && p - valuep < 255)
        {
          memcpy(value, valuep, p - valuep + 1);
          value[p - valuep + 1] = '\0';
        }
      }
    }
  }
  fclose(pFile); 
  return 0;
}

/*
检查磁盘是否空间<100M
param [in] : fspath （检测的磁盘路径）
return: (true or false)
*/
bool checkDiskSpace(const char *fspath)
{
  struct statvfs64 fsbuf;
  if(statvfs64(fspath, &fsbuf) == -1)
  {
	 fprintf(stderr,"statvfs64() failed \n");
	 return false;
  }
  //(块大小*块)转换为兆字节
  unsigned long long freeSpaceMb = (fsbuf.f_bsize>>10)*(fsbuf.f_bavail>>10);
  fprintf(stderr, "Partition %s freespace: %lld MB \n", fspath, freeSpaceMb);
  if( freeSpaceMb < 100)
	 return false;
  else
	 return true;	  
}
