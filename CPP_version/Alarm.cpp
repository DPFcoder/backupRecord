#include "Alarm.hpp"
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


int Alarm::sendAlarm(const ALARM_INFO& _alarmInfo,char* _dstAddr,int _dstPort)
{
	pthread_mutex_lock(&mutex);			
	alarmInfo = _alarmInfo;

	struct sockaddr_in addr;
	int sockfd = -1;
	while (1)
	{
		if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) != -1)
			break;

		sleep(1);
	}

	memset(&addr, 0, sizeof(addr));    
	addr.sin_family = AF_INET;    
	addr.sin_addr.s_addr = inet_addr(_dstAddr);    
	addr.sin_port = htons(_dstPort);

	while(1)
	{
		if(sendto(sockfd, &alarmInfo, sizeof(struct ALARM_INFO), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) > 0)
			break;

		sleep(1);
	}

	::close(sockfd);

	pthread_mutex_unlock(&mutex);
	return 0;
}

