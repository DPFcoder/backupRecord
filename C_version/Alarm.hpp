#ifndef ALARM_HPP
#define ALARM_HPP

#include <pthread.h>
#include <string>

//视频迁移备份报警类型
#define MOUNT_DISK_ERR						0x7200	//挂载磁盘出错
#define MOUNT_DISK_FULL						0x7201	//挂载磁盘空间不足
#define BACKUP_TIMEOUT						0x7202	//超过迁移时间
#define BACKUP_VIDEO_EXCEPT				0x7203	//单个录像迁移异常

using namespace std;

typedef struct ALARM_INFO 
{
	unsigned long event;   // 事件类型，见AlarmType.hpp中的定义
	unsigned long which;   // 防区号，如防区无关的报警则为0. 注意这个值有可能是按位存取，取决于发送者。
	char slot;             // 报警主机插槽号，通常为0
	char res[2];
	unsigned char product; // 设备厂商标识 见ALARM_PRODUCT

	unsigned char macid[8]; // 机器标识，前2 字节为0，后6 字节为第一块网卡的MAC 地址 
	unsigned long myip;     // 本设备第一块网卡的IP 地址 
	unsigned long serial;   // 主动注册服务器分配的设备id 编号，没有分配时为0, 对于迈特安是设备ID 
	unsigned long sequence; // 系统从启动后的累计报警计数
	time_t time;            // 发生报警时的系统时间
	unsigned short res2;
	unsigned short plen;    // 负载信息长度：单位为字节
  // 以上信息长度为 40 字节。
	char payload[472];      // 附加信息，实际有效长度为plen
	ALARM_INFO():
    event(0),
		which(0),
		slot(0),
		myip(0),
		serial(0),
		sequence(0),
		time(0),
		res2(0),
		plen(0),
		product('\0')
	{
		memset(res,0,2);
		memset(macid,0,8);
	}


}ALARM_INFO;


//报警
class Alarm
{
public:
	Alarm() {};
	~Alarm(void) {};

	int sendAlarm(const ALARM_INFO& _alarmInfo,char* _dstAddr,int _dstPort); 

private:
	ALARM_INFO alarmInfo;
	pthread_mutex_t mutex;
};

#endif
