#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include "SocketUtil.hpp"

#include <netinet/tcp.h>  // setsockopt() SOL_TCP
#include "Util.hpp"

//进程线程日志flag
#define THREAD 1
#define NOTHREAD 0


/**
 * @fn SocketUtil::getNetAddr(const char* ipAddr)
 * @brief 将以点号分隔的ip地址转换为big-endian字节序的整型.
 * @param[in] ipaddr  ip地址
 * @return   转换后的整型ip地址.
 * @retval >0: 转换后的整型ip地址.
 * @retval 0l: 如传递的ipaddr为空指针或空串,则返回htonl(INADDR_ANY)
 * @retval -1: 转换失败,请参考inet_addr(const char*);
 */
unsigned int SocketUtil::getNetAddr(const char* ipAddr)
{
  //c++test建议用ipAddr的第一个字符是否为'\0'来检验空串
  if(ipAddr== NULL || strlen(ipAddr) == 0)
    return htonl(INADDR_ANY);
  else
    //返回值 in_addr_t是 uint32_t
    return inet_addr(ipAddr);
}


/**
 * @fn SocketUtil::getHostAddr(unsigned long in)
 * @brief 将big-endian字节序的整型转换为以点号分隔的ip地址串.
 * @param[in] in  整型的ip地址
 * @return   请参考inet_ntoa(unsigned long);
 */
const char * SocketUtil::getHostAddr(unsigned int in)
{
  struct in_addr inaddr;
  inaddr.s_addr = in;
  return inet_ntoa(inaddr);
}



/**
 * @fn SocketUtil::getNetAddrByDev(const char *dev)
 * @brief 获取指定网卡设备的整型ip地址,网络字节序.
 * @param[in] dev  指定网卡设备名
 * @return   获得的整型ip地址
 * @retval -1: 没有找到指定的网卡设备
 * @retval >0: 整型ip地址
 */
//c++ test检测返回值有int类型,return host_addr.sin_addr.s_addr; return htonl(INADDR_ANY);没有问题
unsigned int SocketUtil::getNetAddrByDev(const char *dev)
{
  struct ifreq ifr;
  struct sockaddr_in host_addr;
  int tmpsock;

  if(dev == NULL || strlen(dev) == 0)
    return htonl(INADDR_ANY);

  strcpy(ifr.ifr_name, dev);    // dev is "eth0" or "eth1"...
  if((tmpsock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    return 0;

  if (ioctl(tmpsock, SIOCGIFADDR, &ifr) == -1)
  {
    close(tmpsock);  
    return 0;
  }
  else
  {
    host_addr.sin_addr = ((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr;
    close(tmpsock);
    return host_addr.sin_addr.s_addr;
  }

}

/**
 * @fn SocketUtil::isIpStr(const char* ipStr)
 * @brief 判断一个字符串是否是ip
 * @param[in] ipStr ip字符串
 * @return 一个bool
 */
 
bool SocketUtil::isIpStr(const char* ipStr)
{
  if(INADDR_NONE != inet_addr(ipStr))
    return true;
  else
    return false; 
}

string SocketUtil::getHostByName(const string& hostname)
{
  if(hostname.length() == 0)
    return string("");
  
  struct hostent *phost = gethostbyname(hostname.c_str());

  if(phost != NULL)
    return string( inet_ntoa(*(struct in_addr*)phost->h_addr_list[0]) );
  else
    return string("0.0.0.0");
}

bool SocketUtil::getPeerName(const int sock, char * ip, unsigned short int * port)
{
  struct sockaddr_in sin;
  int len = sizeof(struct sockaddr_in);

  memset(&sin, 0, sizeof(struct sockaddr_in));
  if(getpeername(sock, (struct sockaddr *)&sin, (socklen_t *)&len) != 0)
    return false;
  strcpy(ip, inet_ntoa(sin.sin_addr));
  *port = ntohs(sin.sin_port);
  return true;
}


/**
 * @fn SocketUtil::createPasv(const char* ipAddr, int port, int type, int tcpMaxQueue)
 * @brief 创建一个被动的socket,监听本地端口.
 * @param[in] ipAddr  指明监听本机的ip地址,如监听所有地址,置该值为NULL
 * @param[in] port  监听的端口
 * @param[in] type  协议类型,tcp传递SOCK_STREAM; udp传递SOCK_DGRAM
 * @param[in] tcpMaxQueue  如指明是tcp协议,则此值表示listen时的队列大小;udp协议忽略该值.
 * @return   监听的socket句柄
 * @retval -1: 创建失败,同时会设置errno值.
 * @retval >=0: 监听的socket句柄
 */
int SocketUtil::createPasv(const char* ipAddr, int port, int type, int tcpMaxQueue)
{
  int sockfd;
  struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));

  if((sockfd = socket(AF_INET, type, 0)) == -1)
    return -1;

  // socket句柄不能超过1024,防止上层select出错
  if(sockfd >= 1024)
  {
    close(sockfd);
    // bsprintf(LOG_LOCAL5|LOG_NOTICE, "socket fd limit 1024");
    return -1;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  

  if(ipAddr == NULL)
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
  else
    server_addr.sin_addr.s_addr = SocketUtil::getNetAddr(ipAddr);

  // 设置允许socket立即重用,忽略返回值.
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&sockfd, sizeof(sockfd));

  if(bind(sockfd,(struct sockaddr *)(&server_addr), sizeof(struct sockaddr)) == -1)
  {
    close(sockfd);  
    return -1;
  }

  // tcp only
  if(type == SOCK_STREAM && listen(sockfd, tcpMaxQueue) == -1)
  {
    close(sockfd);  
    return -1;
  }

  return sockfd;
}

/**
 * @fn SocketUtil::createConn(const char* ipAddr, int port, int type, unsigned int seconds)
 * @brief 连接指定的socket地址
 * @param[in] ipAddr  指明连接的ip地址
 * @param[in] port  连接的端口
 * @param[in] type  协议类型,tcp传递SOCK_STREAM; udp传递SOCK_DGRAM\n
 *                  请注意如果是udp协议,实际不会向目的地址发起连接,而只是将目标地址设置到该socket句柄上,
 *                  以后调用时通过send调用即可,不需要再使用sendmsg
 * @param[in] seconds 连接超时秒
 * @return   连接的socket句柄
 * @retval -1: 连接失败,同时会设置errno值.
 * @retval >=0: 已连接的socket句柄
 */
int SocketUtil::createConn(const char* ipAddr, int port, int type, unsigned int seconds)
{
  int sockfd;
  struct sockaddr_in server_addr;
  int status;
  
  //given particular situation,it will not be break
  //the loop until successfully connect to the server 
  if((sockfd = socket(AF_INET, type, 0))==-1)
	{
		LOG_DEBUG(THREAD,"create socket failed");
    return -1;
	}
  
  //socket句柄不能超过1024,防止上层select出错
  if(sockfd >= 1024)
  {
    close(sockfd);
		LOG_DEBUG(THREAD, "socket fd limit 1024");
    return -1;
  }
  
  bzero(&server_addr,sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = inet_addr(ipAddr);

  int non_blocking = 1;
  ioctl(sockfd, FIONBIO, &non_blocking); 

  do
  {
    // For asynch operation, this will return EWOULDBLOCK (windows) or
    // EINPROGRESS (linux) and we just need to wait for the socket to be writable...
    status = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    
    // Connection established OK
    if(status == 0 || errno != EINPROGRESS)
      break;

    struct timeval tv;
    tv.tv_sec = seconds; 
    tv.tv_usec = 0; 
    fd_set fdset; 
    FD_ZERO(&fdset); 
    FD_SET(sockfd, &fdset);
    if(select(sockfd+1, NULL, &fdset, NULL, &tv) != 0)
    {
      if(FD_ISSET(sockfd, &fdset))
      {
        int sockopt;
        socklen_t socklen = sizeof(socklen_t); 
        if(getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char*)&sockopt, &socklen) < 0)
          break;
        if(sockopt != 0)
          break;
      }
      else
        break;
      status = 0;
    }

  }while(false);

  int blocking = 0;
  ioctl(sockfd, FIONBIO, &blocking);    

  if(status == 0)
    return sockfd;
  else
  {
    close(sockfd);  
    return -1;
  }
}
 



/**
 * @fn SocketUtil::createUnixPasv(const char* strAddr, int type)
 * @brief 创建一个被动的unix socket.
 * @param[in] strAddr  指明监听本机监听的地址,在unix socket中此值通常是临时文件.
 * @param[in] type  协议类型,tcp传递SOCK_STREAM; udp传递SOCK_DGRAM
 * @return   监听的socket句柄
 * @retval -1: 创建失败,同时会设置errno值.
 * @retval >=0: 监听的socket句柄
 */
int SocketUtil::createUnixPasv(const char* strAddr, int type)
{
  int sockfd;
  struct sockaddr_un server_addr;
  struct stat stbuf;

  if(stat(strAddr, &stbuf) != -1)
  {
    // 如果此文件已存在,且是一个SOCKET文件,则删除
    if(S_ISSOCK(stbuf.st_mode))
      unlink(strAddr);
  }


  if((sockfd=socket(PF_UNIX, type, 0)) == -1)
    return -1;

  // socket句柄不能超过1024,防止上层select出错
  if(sockfd >= 1024)
  {
    close(sockfd);
    //(LOG_LOCAL5|LOG_NOTICE, "socket fd limit 1024");
    return -1;
  }

  server_addr.sun_family = AF_UNIX;
  strcpy(server_addr.sun_path, strAddr);

  if(bind(sockfd,(struct sockaddr *)(&server_addr),sizeof(struct sockaddr_un)) == -1)
  {
    close(sockfd);  
    return -1;
  }

  // tcp only
  if(type == SOCK_STREAM && listen(sockfd, 4)==-1)
  {
    close(sockfd);  
    return -1;
  }

  return sockfd;

}

/**
 * @fn SocketUtil::createUnixConn(const char* serverAddr, int type, const char* srcAddr)
 * @brief 连接指定的socket地址
 * @param[in] serverAddr  指明连接的地址
 * @param[in] type  协议类型,tcp传递SOCK_STREAM; udp传递SOCK_DGRAM\n
 *                  请注意如果是udp协议,实际不会向目的地址发起连接,而只是将目标地址设置到该socket句柄上,
 *                  以后调用时通过send调用即可,不需要再使用sendto
 * @param[in] srcAddr 在unix socket中,当使用udp协议时,必须客户端也绑定一个地址,否则在server端的
                    recvfrom()中得不到客户端地址. 此值不为NULL, 则表示需要绑定客户端地址
 * @return   连接的socket句柄
 * @retval -1: 连接失败,同时会设置errno值.
 * @retval >=0: 已连接的socket句柄
 */
int SocketUtil::createUnixConn(const char* serverAddr, int type, const char* srcAddr)
{
  int sockfd;
  struct sockaddr_un server_addr;

  if((sockfd=socket(PF_UNIX, type, 0)) == -1)
    return -1;

  // socket句柄不能超过1024,防止上层select出错
  if(sockfd >= 1024)
  {
    close(sockfd);
    //(LOG_LOCAL5|LOG_NOTICE, "socket fd limit 1024");
    return -1;
  }

  if(srcAddr != NULL)
  {
    struct stat stbuf;
    if(stat(srcAddr, &stbuf) != -1)
    {
      // 如果此文件已存在,且是一个SOCKET文件,则删除
      if(S_ISSOCK(stbuf.st_mode))
        unlink(srcAddr);
    }

    struct sockaddr_un client_addr;
    client_addr.sun_family = AF_UNIX;
    strcpy(client_addr.sun_path, srcAddr);

    if(bind(sockfd,(struct sockaddr *)&client_addr,sizeof(struct sockaddr_un)) < 0)
    {
      close(sockfd);
      return -1;
    }
  }

  server_addr.sun_family = AF_UNIX;
  strcpy(server_addr.sun_path, serverAddr);

  int non_blocking = 1;
  ioctl(sockfd, FIONBIO, &non_blocking); 

  // CentOS5 Linux测试:
  // 在unix域socket中,connect表现为当服务端没有在listen时,返回111(Connection refused);
  // 而当服务端的listen队列已满(没有accept时),表现为:
  // 非阻塞时,返回11(Resource temporarily unavailable);
  // 阻塞环境下,一直阻塞直到服务端进行accept或listen队列有空闲的.
  int status = connect(sockfd,(struct sockaddr *)(&server_addr),sizeof(struct sockaddr_un));

  int blocking = 0;
  ioctl(sockfd, FIONBIO, &blocking);

  if(status == -1)
  {
    close(sockfd);  
    return -1;
  }

  return sockfd;

}


/**
 * @fn SocketUtil::sendBuffer(int sockfd, const char* buffer, size_t bufferLen, unsigned int ms)
 * @brief 发送数据到socket.该函数会尽量发送完bufferLen长度再返回.仅支持tcp.
 * @param[in] sockfd  socket句柄
 * @param[in] buffer  发送的数据
 * @param[in] bufferLen  buffer数据长度
 * @param[in] ms 超时毫秒数,0则会即返回.
 * @return
 * @retval -1: 发送失败,同时会设置errno值.errno为EAGAIN时,是返回实际发送的数.
 * @retval >=0:  已发送完的数据长度
 *              发送完: 返回bufferLen;
 *              发送部分(需要阻塞): 返回已发数据长
 *              出错:          -1
 */
ssize_t SocketUtil::sendBuffer(int sockfd, const char* buffer, size_t bufferLen, unsigned int ms)
{
  ssize_t tmp;

  do
  {
    tmp = send(sockfd, buffer, bufferLen, MSG_NOSIGNAL | MSG_DONTWAIT);    
  }while(tmp == -1 && errno == EINTR);

  if(ms == 0)
  {
    // 当socket需要阻塞时,不认为是错误产生.
    if(tmp == -1 && errno == EAGAIN)
      return 0;
    else
      return tmp;
  }
  else
  {
    //强转溢出
    if(bufferLen == (size_t)tmp)
      //返回无符号,可能溢出
      return bufferLen; 
    else if(tmp == -1)
    {
      if(errno == EAGAIN)
        tmp = 0;
      else
        return -1;
    }

    size_t total = bufferLen-tmp;
    const char *p = buffer+tmp;

    struct timeval mtime;
    if(ms<1000)
    {
      mtime.tv_sec = 0;
      mtime.tv_usec = ms*1000;
    }
    else
    {
      mtime.tv_sec = ms/1000;
      mtime.tv_usec = (ms%1000)*1000;
    }

    fd_set fds;
    while(mtime.tv_sec > 0 || mtime.tv_usec > 0)
    {
      // printf("sendBuffer:%lu.%lu, bufferLen:%d, total:%d.\n",mtime.tv_sec, mtime.tv_usec, bufferLen, total);

      FD_ZERO(&fds);
      FD_SET(sockfd, &fds);
      int res = select(sockfd+1, NULL, &fds, NULL, &mtime);
      if(res == -1)
      {
        if(errno == EINTR)
          continue;
        else
          return -1;
      }
      else if(FD_ISSET(sockfd, &fds))
      {
        tmp = send(sockfd, p, total, MSG_NOSIGNAL | MSG_DONTWAIT);
        if(tmp == -1)
          return -1;
        else
        {
          // printf("sendBuffer: tmp:%d\n", tmp);
          if((size_t)tmp == total)
            return bufferLen;

          total -= tmp;
          p += tmp;
        }
      }
    }

    return bufferLen - total;
  }
}


/**
 * @fn SocketUtil::receiveBuffer(int sockfd, char* buffer, size_t bufferLen, unsigned int ms=0)
 * @brief 从socket中读取数据.该函数会尽量读取bufferLen长度再返回.仅支持tcp.
 * @param[in] sockfd  socket句柄
 * @param[in] buffer  读取的数据
 * @param[in] bufferLen  buffer数据长度
 * @param[in] ms 超时毫秒数,0则会即返回.
 * @return
 * @retval -1:  读取失败,同时会设置errno值.errno为EAGAIN时,是返回实际发送的数.
 * @retval >=0: 已读取完的数据长度.如socket正常关闭,则返回-1.
 *              接收完: 返回bufferLen;
 *              接收部分(需要阻塞): 返回已收数据长
 *              出错:          -1
 *              对端socket关闭: -1
 */
ssize_t SocketUtil::receiveBuffer(int sockfd, char* buffer, size_t bufferLen, unsigned int ms)
{
  ssize_t tmp;

  do
  {
    tmp = recv(sockfd, buffer, bufferLen, MSG_DONTWAIT);    
  }while(tmp == -1 && errno == EINTR);

  if(ms == 0)
  {
    // 当socket需要阻塞时,不认为是错误产生.
    if(tmp == -1 && errno == EAGAIN)
      return 0;
    else if(tmp == 0)
      return -1;
    else
      return tmp;
  }
  else
  {
    if(tmp == 0)
      return -1; // socket结束
    else if(bufferLen == (size_t)tmp)
      return bufferLen; 
    else if(tmp == -1)
    {
      if(errno == EAGAIN)
        tmp = 0;
      else
        return -1;
    }
    
    size_t total = bufferLen-tmp;
    char *p = buffer+tmp;

    struct timeval mtime;
    if(ms<1000)
    {
      mtime.tv_sec = 0;
      mtime.tv_usec = ms*1000;
    }
    else
    {
      mtime.tv_sec = ms/1000;
      mtime.tv_usec = (ms%1000)*1000;
    }

    fd_set fds;
    while(mtime.tv_sec > 0 || mtime.tv_usec > 0)
    {
      // printf("receiveBuffer:%lu.%lu, bufferLen:%d, total:%d.\n",mtime.tv_sec, mtime.tv_usec, bufferLen, total);
      FD_ZERO(&fds);
      FD_SET(sockfd, &fds);
      int res = select(sockfd+1, &fds, NULL, NULL, &mtime);
      if(res == -1)
      {
        if(errno == EINTR)
          continue;
        else
          return -1;
      }
      else if(FD_ISSET(sockfd, &fds))
      {
        tmp = recv(sockfd, p, total, MSG_DONTWAIT);
        if(tmp <= 0)
          return -1;
        else
        {
          // printf("receiveBuffer:tmp:%d\n\n", tmp);
          if((size_t)tmp == total)
            return bufferLen;

          total -= tmp;
          p += tmp;
        }
      }
    }

    return bufferLen - total;
  }
 
}



/**
 * @fn SocketUtil::setNonblocking(int sockfd)
 * @brief 将socket设为非阻塞状态,一般用在accpet()或recv()之前.
 * @param[in] sockfd  socket句柄
 * @return
 * @retval 0:  设置成功
 * @retval -1: fcntl()函数调用失败.
 */
int SocketUtil::setNonblocking(int sockfd)
{
  int opts;
  opts=fcntl(sockfd, F_GETFL);
  if (opts < 0)
    return -1;

  opts = opts | O_NONBLOCK;
  if(fcntl(sockfd, F_SETFL, opts) < 0)
    return -1;

  return 0;
}


/**
 * @fn SocketUtil::detectSrcAddr(const char* destAddr, char* srcAddr)
 * @brief     检测通过本地哪个IP地址向destAddr发起连接,即源路由.
 * @param[in] destAddr 目标IP地址
 * @param[out] srcAddr  本地IP地址
 * @return
 * @retval 0:  检测完成.
 * @retval -1: socket出错.
*/
int SocketUtil::detectSrcAddr(const char* destAddr, char* srcAddr)
{
  int sockfd = SocketUtil::createConn(destAddr, 80 /* 该端口无任何意义 */, SOCK_DGRAM, 0);
  if(sockfd == -1)
    return sockfd;

  struct sockaddr_in localsocket;
  memset(&localsocket, 0, sizeof(localsocket));
  socklen_t localsocketlen = sizeof(struct sockaddr_in);
  getsockname(sockfd, (struct sockaddr*)&localsocket, &localsocketlen);
  strcpy(srcAddr, SocketUtil::getHostAddr(localsocket.sin_addr.s_addr));

  // printf("From %s to %s\n", srcAddr, destAddr);

  safeClose(sockfd);
  return 0;
}


/**
 * @fn SocketUtil::accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen, unsigned int ms)
 * @brief        前三个参数参考系统accept().
 * @param[in] ms 超时毫秒.如为0则直接调用系统accept,是否非阻塞取决于句柄属性.
 * @return
 * @retval >=0: 返回新连接的句柄  
 * @retval -1:  socket出错或超时
*/
int SocketUtil::accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen, unsigned int ms/* =0 */)
{
  if(ms == 0)
    return ::accept(sockfd, addr, addrlen);

  struct timeval tv = {0,0};
  if(ms>=1000)
    tv.tv_sec = ms/1000;
  else
    tv.tv_usec = ms*1000;

  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(sockfd, &fds);

  while(1)
  {
    int res = select(sockfd+1, &fds, NULL, NULL, &tv);
    if(res > 0 && FD_ISSET(sockfd, &fds))
      return ::accept(sockfd, addr, addrlen);
    else if(res == -1 && errno == EINTR)
      continue;
    else
      break;
  }

  return -1;
}


/**
 * @fn           SocketUtil::setKeepAlive(int sockfd, bool on, int intvl=0, int probes=0, int time=0)
 * @brief        设置TCP的keepalive属性
 * @param[in] sockfd  被设置的socket句柄
 * @param[in] on      开关项
 * @param[in] intvl   对应net.ipv4.tcp_keepalive_intvl, 0取系统值
 * @param[in] probes  对应net.ipv4.tcp_keepalive_probes, 0取系统值
 * @param[in] time    对应net.ipv4.tcp_keepalive_time, 0取系统值
 * @return
 * @retval 0:     成功
 * @retval -1:  setsockopt出错
*/
int SocketUtil::setKeepAlive(int sockfd, bool on, int intvl, int probes, int time)
{
  int keepAlive = on ? 1 : 0;
  int rs = 0;

  do 
  {
    if(intvl > 0)
    {
      rs = ::setsockopt(sockfd, SOL_TCP, TCP_KEEPINTVL, (void *)&intvl, sizeof(intvl));
      if(rs == -1)
        break;
    }

    if(probes > 0)
    {
      rs = ::setsockopt(sockfd, SOL_TCP, TCP_KEEPCNT, (void *)&probes, sizeof(probes));
      if(rs == -1)
        break;
    }

    if(time > 0)
    {
      rs = ::setsockopt(sockfd, SOL_TCP, TCP_KEEPIDLE, (void *)&time, sizeof(time));
      if(rs == -1)
        break;
    }

    rs = ::setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepAlive, sizeof(keepAlive));
    if(rs == -1)
      break;

  } while(0);
  
  return rs;
}
/**
 * @fn SocketUtil::epollCreatePasv(const char* ipAddr, int port, int type, int tcpMaxQueue)
 * @brief 创建一个被动的socket,监听本地端口.
 * @param[in] ipAddr  指明监听本机的ip地址,如监听所有地址,置该值为NULL
 * @param[in] port  监听的端口
 * @param[in] type  协议类型,tcp传递SOCK_STREAM; udp传递SOCK_DGRAM
 * @param[in] tcpMaxQueue  如指明是tcp协议,则此值表示listen时的队列大小;udp协议忽略该值.
 * @return   监听的socket句柄
 * @retval -1: 创建失败,同时会设置errno值.
 * @retval >=0: 监听的socket句柄
 */
int SocketUtil::epollCreatePasv(const char* ipAddr, int port, int type, int tcpMaxQueue)
{
  int sockfd;
  struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));

  if((sockfd = socket(AF_INET, type, 0)) == -1)
    return -1;
  
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  
  if(ipAddr == NULL)
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
  else
    server_addr.sin_addr.s_addr = SocketUtil::getNetAddr(ipAddr);

  // 设置允许socket立即重用,忽略返回值.
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&sockfd, sizeof(sockfd));

  if(bind(sockfd,(struct sockaddr *)(&server_addr), sizeof(struct sockaddr)) == -1)
  {
    close(sockfd);  
    return -1;
  }

  // tcp only
  if(type == SOCK_STREAM && listen(sockfd, tcpMaxQueue) == -1)
  {
    close(sockfd);  
    return -1;
  }

  return sockfd;
}

/**
 * @fn SocketUtil::epollCreateConn(const char* ipAddr, int port, int type, unsigned int seconds)
 * @brief 连接指定的socket地址
 * @param[in] ipAddr  指明连接的ip地址
 * @param[in] port  连接的端口
 * @param[in] type  协议类型,tcp传递SOCK_STREAM; udp传递SOCK_DGRAM\n
 *                  请注意如果是udp协议,实际不会向目的地址发起连接,而只是将目标地址设置到该socket句柄上,
 *                  以后调用时通过send调用即可,不需要再使用sendmsg
 * @param[in] seconds 连接超时秒
 * @return   连接的socket句柄
 * @retval -1: 连接失败,同时会设置errno值.
 * @retval >=0: 已连接的socket句柄
 */
int SocketUtil::epollCreateConn(const char* ipAddr, int port, int type, unsigned int seconds)
{
  int sockfd,i,nfds,epfd=-1;
  struct sockaddr_in server_addr;
  int status;
  
  //given particular situation,it will not be break
  //the loop until successfully connect to the server 
  if((sockfd = socket(AF_INET, type, 0))==-1)
    return -1;
  
  bzero(&server_addr,sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = inet_addr(ipAddr);

  int non_blocking = 1;
  ioctl(sockfd, FIONBIO, &non_blocking); 

  do
  {
    // For asynch operation, this will return EWOULDBLOCK (windows) or
    // EINPROGRESS (linux) and we just need to wait for the socket to be writable...
    status = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    
    // Connection established OK
    if(status == 0 || errno != EINPROGRESS)
      break;

    struct timeval tv;
    tv.tv_sec = seconds; 
    tv.tv_usec = 0; 
   
    struct epoll_event ev,events[50];
    epfd=epoll_create(5000);
    //设置与要处理的事件相关的文件描述符
    ev.data.fd=sockfd;
    //设置要处理的事件类型
    ev.events=EPOLLOUT|EPOLLET;
    //注册epoll事件
    epoll_ctl(epfd,EPOLL_CTL_ADD,sockfd,&ev);
    nfds=epoll_wait(epfd,events,1000,seconds*1000);
    for(i=0;i<nfds;++i)
    {
      if(events[i].data.fd==sockfd)
      { 
        if(events[i].events & EPOLLOUT)
        {
          int sockopt;
          socklen_t socklen = sizeof(socklen_t); 
          if(getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char*)&sockopt, &socklen) < 0)
            break;
          if(sockopt != 0)
            break;
        }
    else
      break;
      }
      else
        break;
      status = 0;
    }     
  }while(false);

  if (epfd > 0)
    close(epfd);
  int blocking = 0;
  ioctl(sockfd, FIONBIO, &blocking);    

  if(status == 0)
    return sockfd;
  else
  {
    close(sockfd);  
    return -1;
  }
  
}

/**
 * @fn           safeClose(int sockfd);
 * @brief        关闭一个由SocketUtil类创建的socket句柄.
 * @param[in] sockfd  socket句柄
 * @return
*/
void SocketUtil::safeClose(int sockfd)
{
  if(sockfd >= 0)
    close(sockfd);
}
