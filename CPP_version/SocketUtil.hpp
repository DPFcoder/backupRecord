/** $Id: //depot/NVS/v2.1/bsrDirectory/common/SocketUtil.hpp#4 $ $DateTime: 2008/12/05 16:22:45 $
 *  @file SocketUtil.hpp
 *  @brief socketº¯ÊýµÄ·â×°.
 *  @version 1.0.0
 *  @since 1.0.0
 *  @author yulichun<yulc@bstar.com.cn> 
 *  @date 2007-11-13    Created it
 */

/************************************************************ 
 *  @note
 *   Copyright 2005, BeiJing Bluestar Corporation, Limited  
 *   ALL RIGHTS RESERVED      
 *   Permission is hereby granted to licensees of BeiJing Bluestar, Inc.
 *   products to use or abstract this computer program for the sole purpose
 *   of implementing a product based on BeiJing Bluestar, Inc. products.
 *   No other rights to reproduce, use, or disseminate this computer program,
 *   whether in part or in whole, are granted. BeiJing Bluestar, Inc. 
 *   makes no representation or warranties with respect to the performance of
 *   this computer program, and specifically disclaims any responsibility for 
 *   any damages, special or consequential,connected with the use of this program.
 *   For details, see http://www.bstar.com.cn/ 
 ***********************************************************/


#ifndef SOCKETUTIL_HPP_
#define SOCKETUTIL_HPP_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
using namespace std;


class SocketUtil
{
public:
  static unsigned int getNetAddr(const char* ipAddr);
  static const char* getHostAddr(unsigned int in);
  static unsigned int getNetAddrByDev(const char* dev);

  static bool isIpStr(const char* ipStr);
  static string getHostByName(const string& hostname);
  static bool getPeerName(const int sock, char * ip, unsigned short int * port);
  
  static int createPasv(const char* ipAddr, int port, int type, int tcpMaxQueue);
  static int createConn(const char* ipAddr, int port, int type, unsigned int seconds=5);
  static int epollCreatePasv(const char* ipAddr, int port, int type, int tcpMaxQueue);
  static int epollCreateConn(const char* ipAddr, int port, int type, unsigned int seconds=5);
  static ssize_t sendBuffer(int sockfd, const char* buffer, size_t bufferLen, unsigned int ms=0); 
  static ssize_t receiveBuffer(int sockfd, char* buffer, size_t bufferLen, unsigned int ms=0);

  static int createUnixPasv(const char* strAddr, int type);
  static int createUnixConn(const char* serverAddr, int type, const char* srcAddr=NULL);

  static int setNonblocking(int sockfd);
  static int detectSrcAddr(const char* destAddr, char* srcAddr);  
  static int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen, unsigned int ms=0);

  static int setKeepAlive(int sockfd, bool on, int intvl=0, int probes=0, int time=0);

  static void safeClose(int sockfd);

private:
  SocketUtil();
  ~SocketUtil();


};




#endif


