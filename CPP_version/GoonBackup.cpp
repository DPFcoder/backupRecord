#include "GoonBackup.h"
#include <time.h>


GoonBackup::GoonBackup() :
  GoonBackStartTime(0),
  GoonBackStopTime(0),
  GoonBackTimes(0),
  rpc(NULL)
{
}

GoonBackup::~GoonBackup()
{

}

bool GoonBackup::initGoonBackup(LOG* _log, int* _nasMountStatus, char* _saveFilePath, char* _VirtualStreamerIP, char* _relIp)
{
  initBackupRecord(_log,_nasMountStatus,_saveFilePath,_VirtualStreamerIP,_relIp);
  char tmpGoonBackStartTime[10] = { '\0' };
  char tmpGoonBackStopTime[10] = { '\0' };
  char tmpGoonBackTimes[5] = { '\0' };
  if (getFileCfg("GoonBackStartTime", tmpGoonBackStartTime))
    return false;
  if (getFileCfg("GoonBackStopTime", tmpGoonBackStopTime))
    return false;
  if (getFileCfg("GoonBackTimes", tmpGoonBackTimes))
    return false;

  GoonBackStartTime = atoi(strtok(tmpGoonBackStartTime, ":"));
  GoonBackStopTime = atoi(strtok(tmpGoonBackStopTime, ":"));
  GoonBackTimes = atoi(tmpGoonBackTimes);

  rpc = new XmlRpcClient(VirtualStreamerIP, 8003);      

  LOG_DEBUG(NOTHREAD, "GoonBackup init over");
  return true;
}

bool GoonBackup::isOutTime()
{
  time_t nowtimetmp = time(NULL);
  struct tm* ptime = localtime(&nowtimetmp);
  if (GoonBackStartTime > GoonBackStopTime
    && ptime->tm_hour > GoonBackStopTime
    && ptime->tm_hour < GoonBackStartTime)
    return true;

  else if (GoonBackStartTime < GoonBackStopTime
    && (ptime->tm_hour > GoonBackStopTime
      || ptime->tm_hour < GoonBackStartTime))
    return true;

  return false;
}

void GoonBackup::addTask(vector<backupInfo> & task)
{
  backupTask.clear();
  backupTask.assign(task.begin(),task.end());
}


int GoonBackup::GetGoonBackTimes()
{
  return GoonBackTimes;
}

task_it GoonBackup::begin()
{
  return backupTask.begin();
}

task_it GoonBackup::end()
{
  return backupTask.end();
}

void GoonBackup::clear()
{
  backupTask.clear();
}

GoonBackup* GoonBackup::getinstance()
{
  static GoonBackup goonBackup;
  return &goonBackup;
}

bool GoonBackup::queryGoonBackupTasks(BackupTask & task, int times)
{
  char sql[200];
  sprintf(sql, "select * from BackupResult where cameraId in (SELECT c.id FROM Camera c, Site s, Streamer st WHERE c.site=s.id AND s.streamer=st.id AND st.ip='%s') and to_days(now())- to_days(backDate) > 0 and to_days(now())- to_days(backDate) < %d", VirtualStreamerIP, times);

  mysqlpp::Result result;
  int nRows = 0;
  for (int i = 0; ; ++i)
  {
    if (bsrsql.query(sql, result, nRows) || bsrsql.Ping() == 0)
    {
      break;
    }
    sleep(1);
    if (i == 3)
      return false;
  }
  
  mysqlpp::Row row;
  for (int i = 0; i < nRows; ++i)
  {
    row = result.fetch_row();
    backupInfo tmp;
    tmp.cameraId = (int)row[1];
    tmp.date = (string)row[(unsigned int)0];                
    tmp.recordId = (int)row[2];
    tmp.times = (int)row[3];
    tmp.result = (int)row[4];
    task.push_back(tmp);
  }
  return true;
}

bool GoonBackup::gainCameraRecords(const int& cameraId, XmlRpcValue &resultRecords)
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

    LOG_DEBUG(NOTHREAD, "check failed, %s", sql);
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
    if (!rpc->execute("Streamer.Stream.History.list", params, resultRecords) || rpc->isFault())
    {
      if (rpc->isConnected())
        return false;

      LOG_DEBUG(NOTHREAD, "Streamer was not connected， try execute again");
      sleep(3);
      if (isOutTime())		//检查当前时间是否超过迁移时间范围			 
      {
        LOG_DEBUG(NOTHREAD, "Its out of Backup time!");
        return false;
      }
    }
    else
    {
      return true;
    }
  }
}

void* GoonBackup::startTransRecord(backupInfo& info)
{
    int CameraId = info.cameraId;
    int RecordId = info.recordId;
    int Result = info.result;
    int Times = info.times;
    string date = info.date;
    LOG_DEBUG(NOTHREAD, "date:%s   cameraId:%d   recordId:%d",date.c_str(),CameraId,RecordId);

    bool result = false;
    do{ 
      int nRow = 0;
      char sql[50] = { '\0' };
      sprintf(sql, "select name from Camera where id = %d", CameraId);
      mysqlpp::Result res;
      int rows;
      if (!bsrsql.query(sql, res, rows))
        LOG_DEBUG(NOTHREAD, "get name of CameraId %d failed", CameraId);
      mysqlpp::Row row = res.fetch_row();
      string filename = (string)row[(unsigned int)0];
      //此处查找单个摄像头当天的所有录像文件
      XmlRpcValue resultRecords;
      if (!gainCameraRecords(CameraId, resultRecords))
      {
        LOG_DEBUG(NOTHREAD, "Streamer.Stream.History.list  failed!  Need Backup Camera Info:cameraId:%d ", CameraId);			//提示备份单个摄像头一整天的录像文件
        break;
      }
      int cameraRecord = resultRecords["records"].size();

      if (!checkDiskSpace(saveFilePath))
      {
        LOG_DEBUG(NOTHREAD, "The store disk space is full or the path does not exist!!  Need Backup Camera Info： cameraId:%d ", CameraId);
        break;
      }
      int starttime = (int)resultRecords["records"][Result]["startTime"];
      int stoptime = (int)resultRecords["records"][Result]["endTime"];

      if (isOutTime())
      {
        LOG_DEBUG(NOTHREAD, "Its out of Backup time!  Need Backup Video Info： cameraId:%d  starttime:%d  stoptime:%d", CameraId, starttime, stoptime);
        break;
      }

      if (access(saveFilePath, F_OK))
      {
        LOG_DEBUG(NOTHREAD, "saveFilePath is not exist");
        break;
      }

      if (*nasMountStatus == 0)
      {
        LOG_DEBUG(NOTHREAD, "NAS no mount,goto reback:cameraId:%d  starttime:%d  stoptime:%d", CameraId, starttime, stoptime);
        break;
      }

      //检查一级目录是否存在，不存在则创建
      string firstSaveDir = string(saveFilePath) + "/" + filename + "_" + toStr(CameraId);
      struct stat64 stbuf1, stbuf2;
      if (stat64(firstSaveDir.c_str(), &stbuf1) == -1)
      {
        if (mkdir(firstSaveDir.c_str(), 0700))
          LOG_DEBUG(NOTHREAD, "=======================mkdir %s failed======errno:%d =============", firstSaveDir.c_str(), errno);
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
          LOG_DEBUG(NOTHREAD, "=======================mkdir %s failed======errno:%d=============", filepath, errno);
      }

      //文件名
      time_t tmptime = (time_t)starttime;
      struct tm *pstart = localtime(&tmptime);
      char filenamechar[50] = { '\0' };
      sprintf(filenamechar, "%02d%02d%02d-%d%02d%02d", pstart->tm_hour, pstart->tm_min, pstart->tm_sec, pstart->tm_year + 1900, pstart->tm_mon + 1, pstart->tm_mday);
      string saveFileName = string(filepath) + "/" + string(filenamechar) + ".mp4";
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
        LOG_DEBUG(NOTHREAD, "check failed, %s", sql);
        break;
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
        LOG_DEBUG(NOTHREAD, "check failed, %s", sql);
        break;
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
        if (!rpc->execute("Streamer.Stream.History.dump", params, result) || rpc->isFault())	//直连streamer，下载视频	//如果是连接断开,自动连接		//XmlRpcClient调用下载录像方法（execute中有自动连接机制）
        {
          if (rpc->isConnected())
            break;

          //因网络断开而放回错误则循环调用直到连接上
          LOG_DEBUG(NOTHREAD, "Streamer was not connected， try execute again");
          sleep(5);
          if (isOutTime())		//检查当前时间是否超过迁移时间范围			 
          {
            LOG_DEBUG(NOTHREAD, "Its out of Backup time! Need Backup Video Info： cameraId:%d  starttime:%d  stoptime:%d", CameraId, starttime, stoptime);
            break;
          }
        }
        else
        {
          exec_flag = false;
          break;
        }
      }
      if (exec_flag)		//日志提示备份失败的文件信息
      {
        LOG_DEBUG(NOTHREAD, "Video.Archive.startDump  failed!  Need Backup Video Info： cameraId:%d  starttime:%d  stoptime:%d", CameraId, starttime, stoptime);
        break;
      }

      result["rxAddr"] = string(VirtualStreamerIP);
      result["rxPort"] = result["transPort"];

      //****************************************************
      //************下载视频并转换为mp4格式*****************
      //****************************************************
      int sockFd = SocketUtil::createConn(((string)result["rxAddr"]).c_str(), (int)result["rxPort"], SOCK_STREAM, 2);
      if (sockFd < 0)
      {
        LOG_DEBUG(NOTHREAD, "Connect %s:%d failed, err num:%d", ((string)result["rxAddr"]).c_str(), (int)result["rxPort"], errno);
        break;
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
                    LOG_DEBUG(NOTHREAD, "Open_BsrFrame2Mp4 failed, can't create %s, record number %d, Please Check your Network,or Videos, or RecordDB files", saveFileName.c_str(), RecordId);
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
                continue;
              }

              // 将一个BSFP帧转换成MP4帧，并写入到文件中. 文件已在Open_BsrFrame2Mp4()调用时指定好。
              // buffer是一个BSFP帧，长度记录在帧头中的length
              int rs = Convert_BsrFrame2Mp4(pConvertHandle, buffer);
              if (rs != 0)
              {
              }
            }
          }

          storeTimeStamp = pBstpHeader->timeStamp;				//用于socket异常断开检测
          if (rlen <= 0 || pBstpHeader->timeStamp > (int)stoptime)
          {
            LOG_DEBUG(NOTHREAD, "GoonBackup::Stop dumping %s, video size %f MB", rlen, saveFileName.c_str(), size_K / 1024);
            break;
          }
        }
        else if (rlen == -1)			// 通常表示streamer已断开该会话:
        {
          LOG_DEBUG(NOTHREAD, "GoonBackup::Receiving %s data end, video size %f MB", rlen, saveFileName.c_str(), size_K / 1024);
          break;
        }
        else
        {
          //读取BSTPHeader头出问题
        }
      }
      SocketUtil::safeClose(sockFd);
      if (pConvertHandle == NULL)
      {
        LOG_DEBUG(NOTHREAD, "pConvertHandle is NULL! video backup failed!  CameraId:%d,recordId:%d, time period:%d--%d", CameraId, RecordId, starttime, stoptime);
        break;
      }
      Close_BsrFrame2Mp4(pConvertHandle);

      result = true;
    }while (0);

    //update BackupResult表中的该记录的字段times +1和result 1或0         result: 1 补备成功   0 补备失败
    if (result)
    {
      int nRow = 0;
      char sql[100] = { 0, };
      sprintf(sql, "update BackupResult set times = times + 1, result = 1 where  datediff(backDate,'%s') = 0 and cameraId = %d and recordId = %d", date.c_str(), CameraId, RecordId);
      if (bsrsql.execute(string(sql), &nRow))
        LOG_DEBUG(NOTHREAD, "update BackupResult set times = times + 1, result = 1 failed");
    }
    else
    {
      int nRow = 0;
      char sql[100] = { 0, };
      sprintf(sql, "update BackupResult set times = times + 1, result = 0 where  datediff(backDate,'%s') = 0 and cameraId = %d and recordId = %d", date.c_str(), CameraId, RecordId);
      if (bsrsql.execute(string(sql), &nRow))
        LOG_DEBUG(NOTHREAD, "update BackupResult  set times = times + 1, result = 0 failed");
    }

  return NULL;
}

void GoonBackup::work()
{
  for (;;)
  {
    sleep(3);
    //LOG_DEBUG(NOTHREAD,"GoonStartTime:%d GoonStopTime:%d Times:%d",GoonBackStartTime,GoonBackStopTime,GoonBackTimes);
    if (isOutTime())
      continue;

    BackupTask task;
    if (!queryGoonBackupTasks(task, GetGoonBackTimes()))
      continue;

    addTask(task);
    task_it it = begin();
    for (; it != end(); ++it)
      startTransRecord(*it);

    while (!isOutTime())
      sleep(300);
  }
}