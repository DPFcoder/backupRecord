#ifndef ALARM_HPP
#define ALARM_HPP

#include <pthread.h>
#include <string>

//��ƵǨ�Ʊ��ݱ�������
#define MOUNT_DISK_ERR						0x7200	//���ش��̳���
#define MOUNT_DISK_FULL						0x7201	//���ش��̿ռ䲻��
#define BACKUP_TIMEOUT						0x7202	//����Ǩ��ʱ��
#define BACKUP_VIDEO_EXCEPT				0x7203	//����¼��Ǩ���쳣

using namespace std;

typedef struct ALARM_INFO 
{
	unsigned long event;   // �¼����ͣ���AlarmType.hpp�еĶ���
	unsigned long which;   // �����ţ�������޹صı�����Ϊ0. ע�����ֵ�п����ǰ�λ��ȡ��ȡ���ڷ����ߡ�
	char slot;             // ����������ۺţ�ͨ��Ϊ0
	char res[2];
	unsigned char product; // �豸���̱�ʶ ��ALARM_PRODUCT

	unsigned char macid[8]; // ������ʶ��ǰ2 �ֽ�Ϊ0����6 �ֽ�Ϊ��һ��������MAC ��ַ 
	unsigned long myip;     // ���豸��һ��������IP ��ַ 
	unsigned long serial;   // ����ע�������������豸id ��ţ�û�з���ʱΪ0, �������ذ����豸ID 
	unsigned long sequence; // ϵͳ����������ۼƱ�������
	time_t time;            // ��������ʱ��ϵͳʱ��
	unsigned short res2;
	unsigned short plen;    // ������Ϣ���ȣ���λΪ�ֽ�
  // ������Ϣ����Ϊ 40 �ֽڡ�
	char payload[472];      // ������Ϣ��ʵ����Ч����Ϊplen
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


//����
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
