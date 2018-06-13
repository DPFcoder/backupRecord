#include "BackupRecord.h"


char BackupRecord::VirtualStreamerIP[20] = { '\0' };
char BackupRecord::saveFilePath[100] = { '\0' };
pthread_mutex_t BackupRecord::globalXmlRpcMutex = PTHREAD_MUTEX_INITIALIZER;
int *BackupRecord::nasMountStatus = NULL;
BsrSQL BackupRecord::bsrsql;
map<int, XmlRpcClient*> BackupRecord::CameraIdToStreamerMap;
int BackupRecord::transRecordStartTime = 0;
int BackupRecord::transRecordStopTime = 0;
transRecordTimeInfo	BackupRecord::RecordStartTime;
transRecordTimeInfo BackupRecord::RecordStopTime;
time_t BackupRecord::WorkStartTime = 0;
time_t BackupRecord::WorkStopTime = 0;
int BackupRecord::videoRecordCounts = 0;

BackupRecord::BackupRecord():log(NULL)
{
}


BackupRecord::~BackupRecord()
{
}

BackupRecord* BackupRecord::getinstance()
{
  static BackupRecord backupRecord;
  return &backupRecord;
}

bool BackupRecord::initBackupRecord(LOG* _log, int* _nasMountStatus, char* _saveFilePath, char* _VirtualStreamerIP, char* _relIp)
{
  log = _log;
  nasMountStatus = _nasMountStatus;
  memcpy(saveFilePath, _saveFilePath, sizeof(saveFilePath));
  memcpy(VirtualStreamerIP, _VirtualStreamerIP, sizeof(VirtualStreamerIP));
  memcpy(relIp, _relIp, sizeof(relIp));

  //定时迁移视频开始时间 
  char tmpRecordStartTime[10] = { '\0' };
  if (getFileCfg("transRecordStartTime", tmpRecordStartTime))
  {
    LOG_DEBUG(NOTHREAD, "get transRecordStartTime failed");
    return false;
  }
  transRecordStartTime = atoi(strtok(tmpRecordStartTime, ":"));

  //定时迁移视频结束时间
  char tmpRecordStopTime[10] = { '\0' };
  if (getFileCfg("transRecordStopTime", tmpRecordStopTime))
  {
    LOG_DEBUG(NOTHREAD, "get transRecordStopTime failed");
    return false;
  }
  transRecordStopTime = atoi(strtok(tmpRecordStopTime, ":"));

  //迁移视频的开始时间
  char tmptransRecordStartTime[10] = { '\0' };
  if (getFileCfg("RecordStartTime", tmptransRecordStartTime))
  {
    LOG_DEBUG(NOTHREAD, "get RecordStartTime failed");
    return false;
  }
  RecordStartTime.time_hour = atoi(strtok(tmptransRecordStartTime, ":"));
  RecordStartTime.time_min = atoi(strtok(NULL, ":"));
  RecordStartTime.time_sec = atoi(strtok(NULL, ":"));

  //迁移视频的结束时间
  char tmptransRecordStopTime[10] = { '\0' };
  if (getFileCfg("RecordStopTime", tmptransRecordStopTime))
  {
    LOG_DEBUG(NOTHREAD, "get RecordStopTime failed");
    return false;
  }
  RecordStopTime.time_hour = atoi(strtok(tmptransRecordStopTime, ":"));
  RecordStopTime.time_min = atoi(strtok(NULL, ":"));
  RecordStopTime.time_sec = atoi(strtok(NULL, ":"));

  //迁移视频线程数量
  char tmpthreadcounts[5] = { '\0' };
  if (getFileCfg("threadcounts", tmpthreadcounts))
  {
    LOG_DEBUG(NOTHREAD, "get threadcounts failed");
    return false;
  }
  threadcounts = atoi(tmpthreadcounts);

  //获取数据库连接信息
  if (!(getFileCfg("ServerAddr", sqlparas.serverAddr) == 0
    && getFileCfg("database", sqlparas.database) == 0
    && getFileCfg("use", sqlparas.username) == 0
    && getFileCfg("password", sqlparas.password) == 0))
  {
    LOG_DEBUG(NOTHREAD, "get db information failed");
    return false;
  }

  //对获取的配置文件做基本判断
  if (transRecordStartTime == transRecordStopTime
    || transRecordStartTime < 0
    || transRecordStartTime>23
    || transRecordStopTime < 0
    || transRecordStopTime>23
    || threadcounts < 0
    || threadcounts>50
    || RecordStartTime.time_hour < 0
    || RecordStartTime.time_hour>23
    || RecordStartTime.time_min < 0
    || RecordStartTime.time_min>59
    || RecordStartTime.time_sec < 0
    || RecordStartTime.time_sec>59
    || RecordStopTime.time_hour < 0
    || RecordStopTime.time_hour>23
    || RecordStopTime.time_min < 0
    || RecordStopTime.time_min>59
    || RecordStopTime.time_sec < 0
    || RecordStopTime.time_sec>59)
  {
    LOG_DEBUG(NOTHREAD, "==================get Config file information failed!====================");
    return false;
  }
  bsrsql.setCon(&con);
  loginDB(sqlparas);
  GetWorkTimeScope(WorkStartTime,WorkStopTime);
  LOG_DEBUG(NOTHREAD, "BackupRecord init over");
  return true;
}

bool BackupRecord::loginDB(const MysqlParams& sqlparas)
{
  for (int i = 0; i < 3; ++i)
  {
    if (bsrsql.connectToDb(sqlparas))
    {
      LOG_DEBUG(NOTHREAD, "Connect datebase successed");
      return true;
    }
    else
    {
      LOG_DEBUG(NOTHREAD, "Connect datebase failed");
      sleep(3);
    }
  }

  return false;
}

//获取本机streamer对应的cameraId,和cameraId与streamer连接映射   
bool BackupRecord::getCamerasInStreamer(const char * streamerIp, vector<int> &CameraIds, map<int, XmlRpcClient*> porttoxmlrpc)
{
  char sql[150];
  sprintf(sql, "SELECT c.id,st.port FROM Camera c, Site s, Streamer st WHERE c.site=s.id AND s.streamer=st.id AND st.ip='%s'", streamerIp);
  mysqlpp::Result result;
  int nRows = 0;
  for (int i = 0; ; ++i)
  {
    if (bsrsql.query(sql, result, nRows) || bsrsql.Ping() == 0)
      break;
    sleep(1);
    if (i == 3)
      return false;
  }

  mysqlpp::Row row;
  for (int i = 0; i < nRows; ++i)
  {
    row = result.fetch_row();
    int cameraId = (int)row[(unsigned int)0];
    CameraIds.push_back(cameraId);

    CameraIdToStreamerMap[cameraId] = porttoxmlrpc[(int)row[1]];
  }
  return true;
}

//获取备份视频开始和结束time_t类型时间
void BackupRecord::GetTransRecordTime(time_t& begin, time_t& end)
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

  if (transRecordStartTime > transRecordStopTime)					//备份开始和结束时间在24点两侧
  {
    if (ptr->tm_hour > begintime.tm_hour
      || ptr->tm_hour == begintime.tm_hour)			//备份当天时间
    {
      begin = mktime(&begintime);
      end = mktime(&endtime);
    }
    else
    {
      begin = mktime(&begintime) - 24 * 60 * 60;		 
      end = mktime(&endtime) - 24 * 60 * 60;
    }
  }
  else if (transRecordStartTime < transRecordStopTime)   //凌晨前备份完，或凌晨后开始备份
  {
    if (ptr->tm_hour > RecordStopTime.time_hour)									//当前时间大于当天录像结束时间 备份当天视频
    {
      begin = mktime(&begintime);
      end = mktime(&endtime);
    }
    else																			//备份前一天视频
    {
      begin = mktime(&begintime) - 24 * 60 * 60;
      end = mktime(&endtime) - 24 * 60 * 60;
    }
  }
}

//检查当前时间是否超过迁移时间范围			 
bool BackupRecord::isOutTime()
{
  time_t nowtimetmp = time(NULL);
  struct tm* ptime = localtime(&nowtimetmp);
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

//根据CamreaId,获取对应视频文件records
bool BackupRecord::gainCameraRecords(const int& cameraId, XmlRpcValue &resultRecords)
{
  time_t startTime, stopTime;
  GetTransRecordTime(startTime, stopTime);

  if (isOutTime())
  {
    time_t nowtime = time(NULL);
    struct tm* ptime = localtime(&nowtime);
    if (ptime->tm_hour <= RecordStopTime.time_hour)			//超过备份时间查询
    {
      startTime -= 3600 * 12;
      stopTime -= 3600 * 12;
    }
  }
  XmlRpcValue params;
  params["cameraId"] = cameraId;
  params["startTime"] = (int)startTime;
  params["endTime"] = (int)stopTime;
  params["eventType"] = 0; // 表示忽略此条件
  params["attachId"] = 0;  // 同上

  int nRow = 0;
  mysqlpp::Result res;
  char sql[50] = { '\0' };
  sprintf(sql, "SELECT recordDir from Camera where id = %d", cameraId);

  for (int i = 0; ; ++i)
  {
    if (bsrsql.query(sql, res, nRow) || bsrsql.Ping() == 0)					//数据库执行成功  或  执行失败且数据库连接没问题
      break;

    LOG_DEBUG(THREAD, "check failed, %s", sql);
    sleep(1);
    if (i == 3)	return false;
  }
  if (nRow == 1)
  {
    mysqlpp::Row row = res.fetch_row();
    params["recordDir"] = (string)row[(unsigned int)0];											//cameraId对应视频保存目录
  }

  for (; ;)
  {
    pthread_mutex_lock(&globalXmlRpcMutex);
    if (!CameraIdToStreamerMap[cameraId]->execute("Streamer.Stream.History.list", params, resultRecords) || CameraIdToStreamerMap[cameraId]->isFault())							//返回值多组数据  起止时间最大24小时  当CameraId没有对应的返回值时也返回错误
    {
      if (CameraIdToStreamerMap[cameraId]->isConnected())
      {
        pthread_mutex_unlock(&globalXmlRpcMutex);
        return false;
      }

      pthread_mutex_unlock(&globalXmlRpcMutex);
      LOG_DEBUG(THREAD, "Streamer was not connected， try execute again");
      sleep(5);
      if (isOutTime())		//检查当前时间是否超过迁移时间范围			 
      {
        LOG_DEBUG(THREAD, "Its out of Backup time!");
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

//统计本服务器在BackupInfo 表中对应的视频文件数量
int BackupRecord::gainDBRecordsToCameras()
{
  int RecordsCounts = 0;

  char sql[200];
  sprintf(sql, "select count(cameraId) from BackupInfo where cameraId in (SELECT c.id FROM Camera c, Site s, Streamer st WHERE c.site=s.id AND s.streamer=st.id AND st.ip='%s')", VirtualStreamerIP);

  int nRows;
  mysqlpp::Result res;
  if (!bsrsql.query(sql, res, nRows))
  {
    LOG_DEBUG(THREAD, "select count(cameraId) from BackupInfo where cameraId in (SELECT c.id FROM Camera c, Site s, Streamer st WHERE c.site=s.id AND s.streamer=st.id AND st.ip='%s')", VirtualStreamerIP);
    return -1;
  }
  mysqlpp::Row row = res.fetch_row();
  nRows = (int)row[(unsigned int)0];

  return nRows;
}

//下载视频并转换视频
void* BackupRecord::startTransRecord(void* cameraId)
{
  int CameraId = *(int*)cameraId;
  delete (int*)cameraId;
  char sql[50] = { '\0' };
  sprintf(sql, "select name from Camera where id = %d", CameraId);
  mysqlpp::Result res;
  int rows;
  for (int i = 0; ; ++i)
  {
    if (bsrsql.query(sql, res, rows) || bsrsql.Ping() == 0)
      break;
    sleep(3);
    LOG_DEBUG(THREAD, "get name of CameraId %d failed", CameraId);
    if (i == 3)
    {
      LOG_DEBUG(THREAD, "backup Videos of CameraId %d failed", CameraId);
      return NULL;
    }
  }

  mysqlpp::Row row = res.fetch_row();
  string filename = (string)row[(unsigned int)0];

  XmlRpcValue resultRecords;
  if (!gainCameraRecords(CameraId, resultRecords))
  {
    LOG_DEBUG(THREAD, "Streamer.Stream.History.list  failed!  Need Backup Camera Info:cameraId:%d ", CameraId);			//提示备份单个摄像头一整天的录像文件
    return NULL;
  }

  int cameraRecord = resultRecords["records"].size();

  LOG_DEBUG(THREAD, "CameraId %d  match videoRecord number is %d", CameraId, cameraRecord);
  //获取每个CameraId对应的一个或多个视频文件信息	
  for (int k = 0; k < cameraRecord; ++k)
  {
    //检查磁盘空间，小于100M就返回	  //在nas挂载失败或网络原因断开的情况下检查出错
    if (!checkDiskSpace(saveFilePath))
    {
      LOG_DEBUG(THREAD, "The store disk space is full or the path does not exist!!  Need Backup Camera Info： cameraId:%d ", CameraId);
      return NULL;
    }
    int rebackCounts = 3;	 //对于单个录像迁移后异常文件，重新迁移的最大次数
  reback:										//视频备份过程中streamer服务中断，连接streamer后重新备份

    int starttime = (int)resultRecords["records"][k]["startTime"];
    int stoptime = (int)resultRecords["records"][k]["endTime"];

    //检查数据库表BackupInfo中该视频文件是否已经备份
    int nRow = 0;
    mysqlpp::Result res;
    char sql[150] = { '\0' };
    sprintf(sql, "select count(cameraId) from BackupInfo where cameraId = %d and recordId = %d", CameraId, k);
    if (!bsrsql.query(sql, res, nRow))
    {
      LOG_DEBUG(THREAD, "check failed, %s", sql);
    }
    else
    {
      mysqlpp::Row row = res.fetch_row();
      if ((int)row[(unsigned int)0] >= 1)		//已备份 
      {
        LOG_DEBUG(THREAD, "cameraId %d recordId %d has backuped", CameraId, k);
        continue;
      }
    }

    //检查当前时间是否超过迁移时间范围			 
    if (isOutTime())
    {
      LOG_DEBUG(THREAD, "Its out of Backup time!  Need Backup Video Info： cameraId:%d  starttime:%d  stoptime:%d", CameraId, starttime, stoptime);
      continue;
    }

    //检查挂载目录是否存在
    if (access(saveFilePath, F_OK))
    {
      LOG_DEBUG(THREAD, "saveFilePath is not exist");
      return NULL;
    }
    //检查nas挂载
    if (*nasMountStatus == 0)
    {
      LOG_DEBUG(THREAD, "NAS no mount,goto reback:cameraId:%d  starttime:%d  stoptime:%d", CameraId, starttime, stoptime);
      sleep(100);
      goto reback;
    }

    //检查一级目录是否存在，不存在则创建
    string firstSaveDir = string(saveFilePath) + "/" + filename + "_" + toStr(CameraId);
    struct stat64 stbuf1, stbuf2;
    if (stat64(firstSaveDir.c_str(), &stbuf1) == -1)
    {
      if (mkdir(firstSaveDir.c_str(), 0700))
        LOG_DEBUG(THREAD, "=======================mkdir %s failed======errno:%d =============", firstSaveDir.c_str(), errno);
    }

    //检查二级目录是否存在，不存在则创建
    time_t nowTime = time(NULL);
    struct tm *p = localtime(&nowTime);
    char filepath[50] = { '\0' };
    string secondSaveDir = firstSaveDir + "/" + toStr(p->tm_year + 1900) + "_";
    sprintf(filepath, "%s/%d-%02d", firstSaveDir.c_str(), p->tm_year + 1900, p->tm_mon + 1);
    if (stat64(filepath, &stbuf2) == -1)
    {
      if (mkdir(filepath, 0700))
        LOG_DEBUG(THREAD, "=======================mkdir %s failed======errno:%d=============", filepath, errno);
    }

    //文件名
    time_t tmp = (time_t)starttime;
    struct tm *pstart = localtime(&tmp);
    char filename[50] = { '\0' };
    sprintf(filename, "%02d%02d%02d-%d%02d%02d", pstart->tm_hour, pstart->tm_min, pstart->tm_sec, pstart->tm_year + 1900, pstart->tm_mon + 1, pstart->tm_mday);
    string saveFileName = string(filepath) + "/" + string(filename) + ".mp4";
    if (access(saveFileName.c_str(), F_OK))
      remove(saveFileName.c_str());

    XmlRpcValue params, result;
    params["cameraId"] = CameraId;
    params["startTime"] = starttime;
    params["endTime"] = stoptime;
    //*************************************************************************
    nRow = 0;
    mysqlpp::Result res1;
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "select type from Camera where id = %d", CameraId);
    if (!bsrsql.query(sql, res1, nRow))
    {
      LOG_DEBUG(THREAD, "check failed, %s", sql);
      continue;
    }

    if (nRow == 1)
    {
      mysqlpp::Row row = res1.fetch_row();
      params["detailType"] = (string)row[(unsigned int)0];												// 摄像头类型，仅用来区分海康的8000还是9000系列    一般为NULL
    }
    //*************************************************************************
    nRow = 0;
    mysqlpp::Result res2;
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "SELECT recordDir from Camera where id = %d", CameraId);
    if (!bsrsql.query(sql, res2, nRow))
    {
      LOG_DEBUG(THREAD, "check failed, %s", sql);
      continue;
    }
    if (nRow == 1)
    {
      mysqlpp::Row row = res2.fetch_row();
      params["recordDir"] = (string)row[(unsigned int)0];											//cameraId对应视频保存目录
    }
    //*************************************************************************

    bool exec_flag = true;
    for (; ;)
    {
      pthread_mutex_lock(&globalXmlRpcMutex);
      if (!CameraIdToStreamerMap[CameraId]->execute("Streamer.Stream.History.dump", params, result) || CameraIdToStreamerMap[CameraId]->isFault())	//直连streamer，下载视频	//如果是连接断开,自动连接		//XmlRpcClient调用下载录像方法（execute中有自动连接机制）
      {
        if (CameraIdToStreamerMap[CameraId]->isConnected())
        {
          pthread_mutex_unlock(&globalXmlRpcMutex);
          break;
        }
        pthread_mutex_unlock(&globalXmlRpcMutex);

        //因网络断开而放回错误则循环调用直到连接上
        LOG_DEBUG(THREAD, "Streamer was not connected， try execute again");
        sleep(5);
        if (isOutTime())		//检查当前时间是否超过迁移时间范围			 
        {
          LOG_DEBUG(THREAD, "Its out of Backup time! Need Backup Video Info： cameraId:%d  starttime:%d  stoptime:%d", CameraId, starttime, stoptime);
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
    if (exec_flag)		//日志提示备份失败的文件信息
    {
      LOG_DEBUG(THREAD, "Video.Archive.startDump  failed!  Need Backup Video Info： cameraId:%d  starttime:%d  stoptime:%d", CameraId, starttime, stoptime);
      continue;
    }

    result["rxAddr"] = string(VirtualStreamerIP);
    result["rxPort"] = result["transPort"];

    //****************************************************
    //************下载视频并转换为mp4格式*****************
    //****************************************************
    int sockFd = SocketUtil::createConn(((string)result["rxAddr"]).c_str(), (int)result["rxPort"], SOCK_STREAM, 2);
    if (sockFd < 0)
    {
      LOG_DEBUG(THREAD, "Connect %s:%d failed, err num:%d", ((string)result["rxAddr"]).c_str(), (int)result["rxPort"], errno);
      continue;
    }

    // 发送命令给流媒体服务器的流端口
    BSTPHeader bstpHeader;
    bstpHeader.mark = BSTP_HEADER_MARK;
    bstpHeader.type = BSTP_TYPE_COMMAND;
    int cmd = 0x01;
    SocketUtil::sendBuffer(sockFd, (char*)&bstpHeader, sizeof(BSTPHeader), 0);
    SocketUtil::sendBuffer(sockFd, (char*)&cmd, sizeof(int), 0);
    BsrFrame2Mp4_Handle* pConvertHandle = NULL;

    bool first = true;
    int needWrite = -1; // -1是初始状态，0表示已收到第一个视频I帧，1表示可以写mp4文件了。
    char buffer[2048000];
    char iFramebuffer[2048000];
    bool hasIFrame = false;
    int hasAudio = 0;
    int iWidth = 0, iHeight = 0, iFps = 0;
    int iAudioSampleRate = 8000, iAudioBitrate = 32000;//

    float size_K = 0;				//该视频文件大小，单位KB
    time_t storeTimeStamp = 0;
    while (1)
    {
      if (*nasMountStatus == 0)			//挂载失败
      {
        sleep(100);
        LOG_DEBUG(THREAD, "NAS no mount,goto reback:cameraId:%d  starttime:%d  stoptime:%d", CameraId, starttime, stoptime);
        SocketUtil::safeClose(sockFd);
        goto reback;
      }

      if (first)
      {
        first = false;

        // 读 256字节文件头，丢弃
        char fileHeader[256];
        ssize_t rlen = SocketUtil::receiveBuffer(sockFd, fileHeader, 256, 2000);
        if (rlen <= 0)
        {
          break;
        }
      }
      memset(buffer, 0x00, sizeof(buffer));
      ssize_t rlen = SocketUtil::receiveBuffer(sockFd, buffer, sizeof(BSTPHeader), 3000);			// 先接收BSTP头，再接收有效负载
      BSTPHeader* pBstpHeader = (BSTPHeader*)buffer;
      if (rlen == sizeof(BSTPHeader) && pBstpHeader->mark == BSTP_HEADER_MARK
        && pBstpHeader->length < 0x001F3000) // 有效BSTP头，且长度值合理
      {
        if ((rlen = SocketUtil::receiveBuffer(sockFd, buffer + sizeof(BSTPHeader), pBstpHeader->length, 2000)) > 0)
          size_K += (float)rlen / 1024.0;						//累计接收数据---转换前的录像大小

        if (rlen == pBstpHeader->length)
        {
          if (needWrite == -1)
          {
            if (pBstpHeader->type == BSTP_TYPE_AUDIO)  // 表示有音频					音频帧数据都很小 324Byte
            {
              hasAudio = 1;
              iAudioSampleRate = pBstpHeader->format[2];//音频采样率
              iAudioBitrate = (pBstpHeader->format[3] & 0xF0) >> 4; // 获取音频码率
              if (hasIFrame)
              {
                pConvertHandle = Open_BsrFrame2Mp4(iWidth, iHeight, iFps, hasAudio,
                  const_cast<char*>(saveFileName.c_str()), 300, iAudioSampleRate, iAudioBitrate); // 暂时写成300秒
                if (pConvertHandle == NULL)
                {
                  LOG_DEBUG(THREAD, "Open_BsrFrame2Mp4 failed, can't create %s, record number %d, Please Check your Network,or Videos, or RecordDB files", saveFileName.c_str(), k);
                  break;
                }

                needWrite = 1;
                Convert_BsrFrame2Mp4(pConvertHandle, iFramebuffer);//保存的I帧
                Convert_BsrFrame2Mp4(pConvertHandle, buffer);
              }
            }
            else if (IS_BSTP_IFRAME(*pBstpHeader))	// 第一次写文件时，必须是视频I帧，		
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
            // 表示有音频帧出现，但在初始化库时，没有加入音频标志，所以只能丢弃音频帧了。		
            if (pBstpHeader->type == BSTP_TYPE_AUDIO && hasAudio == 0)												//对于在Open_BsrFrame2Mp4时传入的hasAudio为0的情况，所以当接收的数据中存在音频帧时，丢掉该数据
            {
              //LOG_DEBUG(THREAD,"first data  Audio = 0 %d",pBstpHeader->timeStamp);
              continue;
            }

            // 将一个BSFP帧转换成MP4帧，并写入到文件中. 文件已在Open_BsrFrame2Mp4()调用时指定好。
            // buffer是一个BSFP帧，长度记录在帧头中的length
            int rs = Convert_BsrFrame2Mp4(pConvertHandle, buffer);
            if (rs != 0)
            {
              //LOG_DEBUG(THREAD, "Convert_BsrFrame2Mp4 failed. type:%d", pBstpHeader->type);
            }
          }
        }

        storeTimeStamp = pBstpHeader->timeStamp;				//用于socket异常断开检测
        if (rlen <= 0 || pBstpHeader->timeStamp > (int)stoptime)
        {
          LOG_DEBUG(THREAD, "rlen:%d  Stop dumping %s, video size %f MB", rlen, saveFileName.c_str(), size_K / 1024);
          break;
        }
      }
      else if (rlen == -1)			// 通常表示streamer已断开该会话:
      {
        if (pBstpHeader->timeStamp > 0																																							// 异常情况没有进入goto 
          && pBstpHeader->timeStamp < (int)stoptime - 120
          && rebackCounts-- >0)			//差异范围2分钟视为streamer连接异常，接收数据失败，重新连接streamer,并重新备份当前文件,单个文件最多rebackup3次
        {
          SocketUtil::safeClose(sockFd);
          if (pConvertHandle != NULL)
          {
            Close_BsrFrame2Mp4(pConvertHandle);
            pConvertHandle = NULL;
          }
          LOG_DEBUG(THREAD, "backup Video size except,goto reback:cameraId:%d  starttime:%d  stoptime:%d", CameraId, starttime, stoptime);
          goto reback;
        }
        else
        {
          LOG_DEBUG(THREAD, "rlen:%d  Receiving %s data end, video size %f MB", rlen, saveFileName.c_str(), size_K / 1024);
          break;
        }
      }
      else
      {
        //读取BSTPHeader头出问题
      }
    }
    SocketUtil::safeClose(sockFd); // 关闭与streamer录像下载会话
    if (pConvertHandle == NULL)		//20171115 出现bug pConvertHangdle为空   因为对于录像异常产生的空文件读取0个字符
    {
      LOG_DEBUG(THREAD, "pConvertHandle is NULL! video backup failed!  CameraId:%d,recordId:%d, time period:%d--%d", CameraId, k, starttime, stoptime);
      continue;
    }
    Close_BsrFrame2Mp4(pConvertHandle);

    nRow = 0;
    mysqlpp::Result res3;
    sprintf(sql, "select count(*) from BackupInfo where cameraId=%d and recordId=%d", CameraId, k);
    if (!bsrsql.query(sql, res3, nRow))
      LOG_DEBUG(THREAD, "record backup video failed:%s", sql);

    mysqlpp::Row rowsnum = res3.fetch_row();
    if ((int)rowsnum[(unsigned int)0] == 0)		//表中没有该迁移文件信息时才写入数据库表BackupInfo
    {
      memset(sql, 0, sizeof(sql));
      sprintf(sql, "insert into BackupInfo values(%d,%d)", CameraId, k);
      if (!bsrsql.execute(string(sql), &nRow))
        LOG_DEBUG(THREAD, "record backup video failed:%s", sql);

      cancelRegistCurDayBackupInfo(CameraId, k);
    }

    //检查文件数量，检查本次迁移工作是否完成
    if (gainDBRecordsToCameras() == videoRecordCounts)
      LOG_DEBUG(THREAD, "===================backupVideo work is over!!===================");

  }
  return NULL;
}

//登记当天要备份的录像文件信息到BackupResult表中
bool BackupRecord::registCurDayBackupInfoToResult(vector<int> &CameraIds)
{
  int nRow = 0;
  char sql[200] = { '\0' };
  mysqlpp::Result result;
  struct tm *ptr = localtime(&WorkStartTime);
  sprintf(sql, "select count(*) from BackupResult where datediff(curdate(),'%d-%2d-%2d') = 0",ptr->tm_year+1900,ptr->tm_mon+1,ptr->tm_mday);
  if (!bsrsql.query(sql, result, nRow))
  {
    LOG_DEBUG(NOTHREAD, "select count(*) from BackupResult where datediff(curdate(),'%d-%2d-%2d') = 0", ptr->tm_year + 1900, ptr->tm_mon + 1, ptr->tm_mday);
    return false;
  }

  mysqlpp::Row row = result.fetch_row();
  int recordsnum = (int)row[(unsigned int)0];
  if (recordsnum == 0)   
  {
    for (int i = 0; i < CameraIds.size(); ++i)
    {
      XmlRpcValue resultRecords;
      int cameraId = CameraIds[i];
      if (!gainCameraRecords(cameraId, resultRecords))
      {
        LOG_DEBUG(NOTHREAD, "gainCameraRecords failed!");
        continue;
      }

      int recordsnum = resultRecords["records"].size();
      for (int k = 0; k < recordsnum; ++k)
      {
        memset(sql, 0, sizeof(sql));
        sprintf(sql, "insert into BackupResult values(curdate(),%d,%d,0,0)", cameraId, k);
        if (!bsrsql.execute(string(sql), &nRow))
        {
          LOG_DEBUG(NOTHREAD, "insert into BackupResult values(curdate(),%d,%d,0,0)", cameraId, k);
        }
      }
    }
  }
  return true;
}

//从BackupResult中删除当天备份成功的录像文件信息
void BackupRecord::cancelRegistCurDayBackupInfo(int cameraId, int recordId)
{
  int nRow = 0;
  char sql[100] = { 0, };
  sprintf(sql, "delete from BackupResult where to_days(curdate()) = to_days(backDate) and cameraId = %d and recordId = %d", cameraId, recordId);
  if (!bsrsql.execute(string(sql), &nRow))
  {
    LOG_DEBUG(NOTHREAD, "", cameraId, recordId);
  }
}

void BackupRecord::work()
{
  char sql[150];
  sprintf(sql, "select port from Streamer where ip='%s' order by id", VirtualStreamerIP);
  mysqlpp::Result result;
  int nRows = 0;

  for (; ;)
  {
    if (bsrsql.query(sql, result, nRows))  break;

    if (bsrsql.Ping() == 0)
      LOG_DEBUG(NOTHREAD, "sql bsrsql.query failed:%s", sql);
    else
      LOG_DEBUG(NOTHREAD, "sql connection with mysql  is error");
    sleep(5);
  }
  map<int, XmlRpcClient*> PortToXmlRpcMap;		//端口和streamer连接指针映射
  for (int i = 0; i < nRows; ++i)
  {
    mysqlpp::Row row = result.fetch_row();
    int port = (int)row[(unsigned int)0];
    globalXmlRpcClient[i] = new XmlRpcClient(VirtualStreamerIP, port);
    globalXmlRpcClient[i]->setWaitTimeout(5);

    PortToXmlRpcMap[port] = globalXmlRpcClient[i];
  }

  threadpool.SetTaskFuntion(BackupRecord::startTransRecord);
  threadpool.Pthreadinit(threadcounts);
  for (; ;)
  {
    sleep(3);
    if (isOutTime())
    {
      int nRow = 0;
      char sql[200] = { '\0' };
      sprintf(sql, "delete from BackupInfo where cameraId in (SELECT c.id FROM Camera c, Site s, Streamer st WHERE c.site=s.id AND s.streamer=st.id AND st.ip='%s')", VirtualStreamerIP);			//清空本服务器对应的Camera信息
      if (!bsrsql.execute(string(sql), &nRow))
        LOG_DEBUG(NOTHREAD, "delete from BackupInfo failed,%s", sql);
      continue;
    }

    if (strlen(VirtualStreamerIP) < 8)
      continue;

    vector<int> CameraIds;					//获取备份所有CameraId
    for (; ;)
    {
      if (getCamerasInStreamer(VirtualStreamerIP, CameraIds, PortToXmlRpcMap) || isOutTime())
        break;

      sleep(10);
      LOG_DEBUG(NOTHREAD, "getCamerasInstreamer failed");
    }

    //将当天要备份的录像文件写进BackupResult表中，每天只写一次
    if (time(NULL) >= WorkStartTime && time(NULL) <=WorkStopTime)         
    {
      if (!registCurDayBackupInfoToResult(CameraIds))
        continue;
      WorkStartTime += 24 * 60 * 60;
      WorkStopTime += 24 * 60 * 60;
    }
    
    //获取当前要备份的视频文件总数
    videoRecordCounts = 0;
    for (int i = 0; i < CameraIds.size(); ++i)
    {
      XmlRpcValue resultRecords;
      if (gainCameraRecords(CameraIds[i], resultRecords))
        videoRecordCounts += resultRecords["records"].size();
    }

    if (videoRecordCounts == gainDBRecordsToCameras() || videoRecordCounts == 0)				//备份完毕或没有需要备份的文件
      continue;

    LOG_DEBUG(NOTHREAD, "=========================================BackupVideo time");
    LOG_DEBUG(NOTHREAD, "CameraId size is %d  in streamer  %s-------------backupRecords number is %d", CameraIds.size(), VirtualStreamerIP, videoRecordCounts);

    //将迁移任务添加到线程工作队列
    vector<int*> taskQueue;
    for (int i = 0; i < CameraIds.size(); ++i)
      taskQueue.push_back(new int(CameraIds[i]));
    threadpool.Addwork(taskQueue);

    for (; ;)
    {
      sleep(300);
      if (isOutTime())
      {
        LOG_DEBUG(NOTHREAD, "it's out of time");
        break;
      }

      if (threadpool.GetTaskQueueNum() == 0)			//备机录像回迁之前的本机录像备份完毕  且所有线程没有其他任务
        break;
    }
  }

  delete[] globalXmlRpcClient;
}

void BackupRecord::sendBackupAlarm(unsigned long alarmType)
{
  ALARM_INFO alarmInfo;
  memset(&alarmInfo, 0, sizeof(alarmInfo));
  alarmInfo.myip = (unsigned long)inet_addr(VirtualStreamerIP);
  alarmInfo.event = alarmType;
  alarmInfo.which = 0;
  alarmInfo.time = time(NULL);
  backupAlarm.sendAlarm(alarmInfo, relIp, 2134);
  LOG_DEBUG(THREAD, "Send Alarm!!!");
}

// 通过BSFP中的标志位，计算视频分辨率的宽高
void BackupRecord::getVideoResolution(int bsfptab, int* pWidth, int* pHeight)
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

//获取正在备份的时间范围标准时间
void BackupRecord::GetWorkTimeScope(time_t& begin, time_t& end)
{
  time_t nowtime = time(NULL);
  struct tm *ptr = localtime(&nowtime);
  struct tm begintime(*localtime(&nowtime));
  struct tm	endtime(*localtime(&nowtime));
  begintime.tm_hour = transRecordStartTime;
  begintime.tm_min = 0;
  begintime.tm_sec = 0;
  endtime.tm_hour = transRecordStopTime;
  endtime.tm_min = 0;
  endtime.tm_sec = 0;

  if (transRecordStartTime > transRecordStopTime)					//备份开始时间和结束时间在24点两侧
  {
    if (ptr->tm_hour > begintime.tm_hour
      || ptr->tm_hour == begintime.tm_hour)			//备份当天时间
    {
      begin = mktime(&begintime);
      end = mktime(&endtime) + 24 * 60 * 60;
    }
    else
    {
      begin = mktime(&begintime) - 24 * 60 * 60;
      end = mktime(&endtime);
    }
  }
  else if (transRecordStartTime < transRecordStopTime)   //凌晨前备份完，或凌晨后开始备份
  {
      begin = mktime(&begintime);
      end = mktime(&endtime);
  }
}