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
    if (ptime->tm_hour <= RecordStopTime.time_hour)			//��������ʱ���ѯ
    {
      startTime -= 3600 * 12;
      stopTime -= 3600 * 12;
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
  char sql[50] = { '\0' };
  sprintf(sql, "SELECT recordDir from Camera where id = %d", cameraId);

  for (int i = 0; ; ++i)
  {
    if (bsrsql.query(sql, res, nRow) || bsrsql.Ping() == 0)					//���ݿ�ִ�гɹ�  ��  ִ��ʧ�������ݿ�����û����
      break;

    LOG_DEBUG(NOTHREAD, "check failed, %s", sql);
    sleep(1);
    if (i == 3)	return false;
  }
  if (nRow == 1)
  {
    mysqlpp::Row row = res.fetch_row();
    params["recordDir"] = (string)row[(unsigned int)0];											//cameraId��Ӧ��Ƶ����Ŀ¼
  }

  for (; ;)
  {
    if (!rpc->execute("Streamer.Stream.History.list", params, resultRecords) || rpc->isFault())
    {
      if (rpc->isConnected())
        return false;

      LOG_DEBUG(NOTHREAD, "Streamer was not connected�� try execute again");
      sleep(3);
      if (isOutTime())		//��鵱ǰʱ���Ƿ񳬹�Ǩ��ʱ�䷶Χ			 
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
      //�˴����ҵ�������ͷ���������¼���ļ�
      XmlRpcValue resultRecords;
      if (!gainCameraRecords(CameraId, resultRecords))
      {
        LOG_DEBUG(NOTHREAD, "Streamer.Stream.History.list  failed!  Need Backup Camera Info:cameraId:%d ", CameraId);			//��ʾ���ݵ�������ͷһ�����¼���ļ�
        break;
      }
      int cameraRecord = resultRecords["records"].size();

      if (!checkDiskSpace(saveFilePath))
      {
        LOG_DEBUG(NOTHREAD, "The store disk space is full or the path does not exist!!  Need Backup Camera Info�� cameraId:%d ", CameraId);
        break;
      }
      int starttime = (int)resultRecords["records"][Result]["startTime"];
      int stoptime = (int)resultRecords["records"][Result]["endTime"];

      if (isOutTime())
      {
        LOG_DEBUG(NOTHREAD, "Its out of Backup time!  Need Backup Video Info�� cameraId:%d  starttime:%d  stoptime:%d", CameraId, starttime, stoptime);
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

      //���һ��Ŀ¼�Ƿ���ڣ��������򴴽�
      string firstSaveDir = string(saveFilePath) + "/" + filename + "_" + toStr(CameraId);
      struct stat64 stbuf1, stbuf2;
      if (stat64(firstSaveDir.c_str(), &stbuf1) == -1)
      {
        if (mkdir(firstSaveDir.c_str(), 0700))
          LOG_DEBUG(NOTHREAD, "=======================mkdir %s failed======errno:%d =============", firstSaveDir.c_str(), errno);
      }

      //������Ŀ¼�Ƿ���ڣ��������򴴽�
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

      //�ļ���
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
        params["detailType"] = (string)row[(unsigned int)0];												// ����ͷ���ͣ����������ֺ�����8000����9000ϵ��    һ��ΪNULL
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
        params["recordDir"] = (string)row[(unsigned int)0];											//cameraId��Ӧ��Ƶ����Ŀ¼
      }
      //*************************************************************************


      bool exec_flag = true;
      for (; ;)
      {
        if (!rpc->execute("Streamer.Stream.History.dump", params, result) || rpc->isFault())	//ֱ��streamer��������Ƶ	//��������ӶϿ�,�Զ�����		//XmlRpcClient��������¼�񷽷���execute�����Զ����ӻ��ƣ�
        {
          if (rpc->isConnected())
            break;

          //������Ͽ����Żش�����ѭ������ֱ��������
          LOG_DEBUG(NOTHREAD, "Streamer was not connected�� try execute again");
          sleep(5);
          if (isOutTime())		//��鵱ǰʱ���Ƿ񳬹�Ǩ��ʱ�䷶Χ			 
          {
            LOG_DEBUG(NOTHREAD, "Its out of Backup time! Need Backup Video Info�� cameraId:%d  starttime:%d  stoptime:%d", CameraId, starttime, stoptime);
            break;
          }
        }
        else
        {
          exec_flag = false;
          break;
        }
      }
      if (exec_flag)		//��־��ʾ����ʧ�ܵ��ļ���Ϣ
      {
        LOG_DEBUG(NOTHREAD, "Video.Archive.startDump  failed!  Need Backup Video Info�� cameraId:%d  starttime:%d  stoptime:%d", CameraId, starttime, stoptime);
        break;
      }

      result["rxAddr"] = string(VirtualStreamerIP);
      result["rxPort"] = result["transPort"];

      //****************************************************
      //************������Ƶ��ת��Ϊmp4��ʽ*****************
      //****************************************************
      int sockFd = SocketUtil::createConn(((string)result["rxAddr"]).c_str(), (int)result["rxPort"], SOCK_STREAM, 2);
      if (sockFd < 0)
      {
        LOG_DEBUG(NOTHREAD, "Connect %s:%d failed, err num:%d", ((string)result["rxAddr"]).c_str(), (int)result["rxPort"], errno);
        break;
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
      int iWidth = 0, iHeight = 0, iFps = 0;
      int iAudioSampleRate = 8000, iAudioBitrate = 32000;//

      float size_K = 0;				//����Ƶ�ļ���С����λKB
      time_t storeTimeStamp = 0;
      while (1)
      {
        if (first)
        {
          first = false;

          // �� 256�ֽ��ļ�ͷ������
          char fileHeader[256];
          ssize_t rlen = SocketUtil::receiveBuffer(sockFd, fileHeader, 256, 2000);
          if (rlen <= 0)
          {
            break;
          }
        }
        memset(buffer, 0x00, sizeof(buffer));
        ssize_t rlen = SocketUtil::receiveBuffer(sockFd, buffer, sizeof(BSTPHeader), 3000);			// �Ƚ���BSTPͷ���ٽ�����Ч����
        BSTPHeader* pBstpHeader = (BSTPHeader*)buffer;
        if (rlen == sizeof(BSTPHeader) && pBstpHeader->mark == BSTP_HEADER_MARK
          && pBstpHeader->length < 0x001F3000) // ��ЧBSTPͷ���ҳ���ֵ����
        {
          if ((rlen = SocketUtil::receiveBuffer(sockFd, buffer + sizeof(BSTPHeader), pBstpHeader->length, 2000)) > 0)
            size_K += (float)rlen / 1024.0;						//�ۼƽ�������---ת��ǰ��¼���С

          if (rlen == pBstpHeader->length)
          {
            if (needWrite == -1)
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
                    LOG_DEBUG(NOTHREAD, "Open_BsrFrame2Mp4 failed, can't create %s, record number %d, Please Check your Network,or Videos, or RecordDB files", saveFileName.c_str(), RecordId);
                    break;
                  }

                  needWrite = 1;
                  Convert_BsrFrame2Mp4(pConvertHandle, iFramebuffer);//�����I֡
                  Convert_BsrFrame2Mp4(pConvertHandle, buffer);
                }
              }
              else if (IS_BSTP_IFRAME(*pBstpHeader))	// ��һ��д�ļ�ʱ����������ƵI֡��		
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
              if (pBstpHeader->type == BSTP_TYPE_AUDIO && hasAudio == 0)												//������Open_BsrFrame2Mp4ʱ�����hasAudioΪ0����������Ե����յ������д�����Ƶ֡ʱ������������
              {
                continue;
              }

              // ��һ��BSFP֡ת����MP4֡����д�뵽�ļ���. �ļ�����Open_BsrFrame2Mp4()����ʱָ���á�
              // buffer��һ��BSFP֡�����ȼ�¼��֡ͷ�е�length
              int rs = Convert_BsrFrame2Mp4(pConvertHandle, buffer);
              if (rs != 0)
              {
              }
            }
          }

          storeTimeStamp = pBstpHeader->timeStamp;				//����socket�쳣�Ͽ����
          if (rlen <= 0 || pBstpHeader->timeStamp > (int)stoptime)
          {
            LOG_DEBUG(NOTHREAD, "GoonBackup::Stop dumping %s, video size %f MB", rlen, saveFileName.c_str(), size_K / 1024);
            break;
          }
        }
        else if (rlen == -1)			// ͨ����ʾstreamer�ѶϿ��ûỰ:
        {
          LOG_DEBUG(NOTHREAD, "GoonBackup::Receiving %s data end, video size %f MB", rlen, saveFileName.c_str(), size_K / 1024);
          break;
        }
        else
        {
          //��ȡBSTPHeaderͷ������
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

    //update BackupResult���еĸü�¼���ֶ�times +1��result 1��0         result: 1 �����ɹ�   0 ����ʧ��
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