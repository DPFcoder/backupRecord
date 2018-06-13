#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include "SocketUtil.hpp"

#include <netinet/tcp.h>  // setsockopt() SOL_TCP
#include "Util.hpp"

//�����߳���־flag
#define THREAD 1
#define NOTHREAD 0


/**
 * @fn SocketUtil::getNetAddr(const char* ipAddr)
 * @brief ���Ե�ŷָ���ip��ַת��Ϊbig-endian�ֽ��������.
 * @param[in] ipaddr  ip��ַ
 * @return   ת���������ip��ַ.
 * @retval >0: ת���������ip��ַ.
 * @retval 0l: �紫�ݵ�ipaddrΪ��ָ���մ�,�򷵻�htonl(INADDR_ANY)
 * @retval -1: ת��ʧ��,��ο�inet_addr(const char*);
 */
unsigned int SocketUtil::getNetAddr(const char* ipAddr)
{
  //c++test������ipAddr�ĵ�һ���ַ��Ƿ�Ϊ'\0'������մ�
  if(ipAddr== NULL || strlen(ipAddr) == 0)
    return htonl(INADDR_ANY);
  else
    //����ֵ in_addr_t�� uint32_t
    return inet_addr(ipAddr);
}


/**
 * @fn SocketUtil::getHostAddr(unsigned long in)
 * @brief ��big-endian�ֽ��������ת��Ϊ�Ե�ŷָ���ip��ַ��.
 * @param[in] in  ���͵�ip��ַ
 * @return   ��ο�inet_ntoa(unsigned long);
 */
const char * SocketUtil::getHostAddr(unsigned int in)
{
  struct in_addr inaddr;
  inaddr.s_addr = in;
  return inet_ntoa(inaddr);
}



/**
 * @fn SocketUtil::getNetAddrByDev(const char *dev)
 * @brief ��ȡָ�������豸������ip��ַ,�����ֽ���.
 * @param[in] dev  ָ�������豸��
 * @return   ��õ�����ip��ַ
 * @retval -1: û���ҵ�ָ���������豸
 * @retval >0: ����ip��ַ
 */
//c++ test��ⷵ��ֵ��int����,return host_addr.sin_addr.s_addr; return htonl(INADDR_ANY);û������
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
 * @brief �ж�һ���ַ����Ƿ���ip
 * @param[in] ipStr ip�ַ���
 * @return һ��bool
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
 * @brief ����һ��������socket,�������ض˿�.
 * @param[in] ipAddr  ָ������������ip��ַ,��������е�ַ,�ø�ֵΪNULL
 * @param[in] port  �����Ķ˿�
 * @param[in] type  Э������,tcp����SOCK_STREAM; udp����SOCK_DGRAM
 * @param[in] tcpMaxQueue  ��ָ����tcpЭ��,���ֵ��ʾlistenʱ�Ķ��д�С;udpЭ����Ը�ֵ.
 * @return   ������socket���
 * @retval -1: ����ʧ��,ͬʱ������errnoֵ.
 * @retval >=0: ������socket���
 */
int SocketUtil::createPasv(const char* ipAddr, int port, int type, int tcpMaxQueue)
{
  int sockfd;
  struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));

  if((sockfd = socket(AF_INET, type, 0)) == -1)
    return -1;

  // socket������ܳ���1024,��ֹ�ϲ�select����
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

  // ��������socket��������,���Է���ֵ.
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
 * @brief ����ָ����socket��ַ
 * @param[in] ipAddr  ָ�����ӵ�ip��ַ
 * @param[in] port  ���ӵĶ˿�
 * @param[in] type  Э������,tcp����SOCK_STREAM; udp����SOCK_DGRAM\n
 *                  ��ע�������udpЭ��,ʵ�ʲ�����Ŀ�ĵ�ַ��������,��ֻ�ǽ�Ŀ���ַ���õ���socket�����,
 *                  �Ժ����ʱͨ��send���ü���,����Ҫ��ʹ��sendmsg
 * @param[in] seconds ���ӳ�ʱ��
 * @return   ���ӵ�socket���
 * @retval -1: ����ʧ��,ͬʱ������errnoֵ.
 * @retval >=0: �����ӵ�socket���
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
  
  //socket������ܳ���1024,��ֹ�ϲ�select����
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
 * @brief ����һ��������unix socket.
 * @param[in] strAddr  ָ���������������ĵ�ַ,��unix socket�д�ֵͨ������ʱ�ļ�.
 * @param[in] type  Э������,tcp����SOCK_STREAM; udp����SOCK_DGRAM
 * @return   ������socket���
 * @retval -1: ����ʧ��,ͬʱ������errnoֵ.
 * @retval >=0: ������socket���
 */
int SocketUtil::createUnixPasv(const char* strAddr, int type)
{
  int sockfd;
  struct sockaddr_un server_addr;
  struct stat stbuf;

  if(stat(strAddr, &stbuf) != -1)
  {
    // ������ļ��Ѵ���,����һ��SOCKET�ļ�,��ɾ��
    if(S_ISSOCK(stbuf.st_mode))
      unlink(strAddr);
  }


  if((sockfd=socket(PF_UNIX, type, 0)) == -1)
    return -1;

  // socket������ܳ���1024,��ֹ�ϲ�select����
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
 * @brief ����ָ����socket��ַ
 * @param[in] serverAddr  ָ�����ӵĵ�ַ
 * @param[in] type  Э������,tcp����SOCK_STREAM; udp����SOCK_DGRAM\n
 *                  ��ע�������udpЭ��,ʵ�ʲ�����Ŀ�ĵ�ַ��������,��ֻ�ǽ�Ŀ���ַ���õ���socket�����,
 *                  �Ժ����ʱͨ��send���ü���,����Ҫ��ʹ��sendto
 * @param[in] srcAddr ��unix socket��,��ʹ��udpЭ��ʱ,����ͻ���Ҳ��һ����ַ,������server�˵�
                    recvfrom()�еò����ͻ��˵�ַ. ��ֵ��ΪNULL, ���ʾ��Ҫ�󶨿ͻ��˵�ַ
 * @return   ���ӵ�socket���
 * @retval -1: ����ʧ��,ͬʱ������errnoֵ.
 * @retval >=0: �����ӵ�socket���
 */
int SocketUtil::createUnixConn(const char* serverAddr, int type, const char* srcAddr)
{
  int sockfd;
  struct sockaddr_un server_addr;

  if((sockfd=socket(PF_UNIX, type, 0)) == -1)
    return -1;

  // socket������ܳ���1024,��ֹ�ϲ�select����
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
      // ������ļ��Ѵ���,����һ��SOCKET�ļ�,��ɾ��
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

  // CentOS5 Linux����:
  // ��unix��socket��,connect����Ϊ�������û����listenʱ,����111(Connection refused);
  // ��������˵�listen��������(û��acceptʱ),����Ϊ:
  // ������ʱ,����11(Resource temporarily unavailable);
  // ����������,һֱ����ֱ������˽���accept��listen�����п��е�.
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
 * @brief �������ݵ�socket.�ú����ᾡ��������bufferLen�����ٷ���.��֧��tcp.
 * @param[in] sockfd  socket���
 * @param[in] buffer  ���͵�����
 * @param[in] bufferLen  buffer���ݳ���
 * @param[in] ms ��ʱ������,0��ἴ����.
 * @return
 * @retval -1: ����ʧ��,ͬʱ������errnoֵ.errnoΪEAGAINʱ,�Ƿ���ʵ�ʷ��͵���.
 * @retval >=0:  �ѷ���������ݳ���
 *              ������: ����bufferLen;
 *              ���Ͳ���(��Ҫ����): �����ѷ����ݳ�
 *              ����:          -1
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
    // ��socket��Ҫ����ʱ,����Ϊ�Ǵ������.
    if(tmp == -1 && errno == EAGAIN)
      return 0;
    else
      return tmp;
  }
  else
  {
    //ǿת���
    if(bufferLen == (size_t)tmp)
      //�����޷���,�������
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
 * @brief ��socket�ж�ȡ����.�ú����ᾡ����ȡbufferLen�����ٷ���.��֧��tcp.
 * @param[in] sockfd  socket���
 * @param[in] buffer  ��ȡ������
 * @param[in] bufferLen  buffer���ݳ���
 * @param[in] ms ��ʱ������,0��ἴ����.
 * @return
 * @retval -1:  ��ȡʧ��,ͬʱ������errnoֵ.errnoΪEAGAINʱ,�Ƿ���ʵ�ʷ��͵���.
 * @retval >=0: �Ѷ�ȡ������ݳ���.��socket�����ر�,�򷵻�-1.
 *              ������: ����bufferLen;
 *              ���ղ���(��Ҫ����): �����������ݳ�
 *              ����:          -1
 *              �Զ�socket�ر�: -1
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
    // ��socket��Ҫ����ʱ,����Ϊ�Ǵ������.
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
      return -1; // socket����
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
 * @brief ��socket��Ϊ������״̬,һ������accpet()��recv()֮ǰ.
 * @param[in] sockfd  socket���
 * @return
 * @retval 0:  ���óɹ�
 * @retval -1: fcntl()��������ʧ��.
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
 * @brief     ���ͨ�������ĸ�IP��ַ��destAddr��������,��Դ·��.
 * @param[in] destAddr Ŀ��IP��ַ
 * @param[out] srcAddr  ����IP��ַ
 * @return
 * @retval 0:  ������.
 * @retval -1: socket����.
*/
int SocketUtil::detectSrcAddr(const char* destAddr, char* srcAddr)
{
  int sockfd = SocketUtil::createConn(destAddr, 80 /* �ö˿����κ����� */, SOCK_DGRAM, 0);
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
 * @brief        ǰ���������ο�ϵͳaccept().
 * @param[in] ms ��ʱ����.��Ϊ0��ֱ�ӵ���ϵͳaccept,�Ƿ������ȡ���ھ������.
 * @return
 * @retval >=0: ���������ӵľ��  
 * @retval -1:  socket�����ʱ
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
 * @brief        ����TCP��keepalive����
 * @param[in] sockfd  �����õ�socket���
 * @param[in] on      ������
 * @param[in] intvl   ��Ӧnet.ipv4.tcp_keepalive_intvl, 0ȡϵͳֵ
 * @param[in] probes  ��Ӧnet.ipv4.tcp_keepalive_probes, 0ȡϵͳֵ
 * @param[in] time    ��Ӧnet.ipv4.tcp_keepalive_time, 0ȡϵͳֵ
 * @return
 * @retval 0:     �ɹ�
 * @retval -1:  setsockopt����
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
 * @brief ����һ��������socket,�������ض˿�.
 * @param[in] ipAddr  ָ������������ip��ַ,��������е�ַ,�ø�ֵΪNULL
 * @param[in] port  �����Ķ˿�
 * @param[in] type  Э������,tcp����SOCK_STREAM; udp����SOCK_DGRAM
 * @param[in] tcpMaxQueue  ��ָ����tcpЭ��,���ֵ��ʾlistenʱ�Ķ��д�С;udpЭ����Ը�ֵ.
 * @return   ������socket���
 * @retval -1: ����ʧ��,ͬʱ������errnoֵ.
 * @retval >=0: ������socket���
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

  // ��������socket��������,���Է���ֵ.
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
 * @brief ����ָ����socket��ַ
 * @param[in] ipAddr  ָ�����ӵ�ip��ַ
 * @param[in] port  ���ӵĶ˿�
 * @param[in] type  Э������,tcp����SOCK_STREAM; udp����SOCK_DGRAM\n
 *                  ��ע�������udpЭ��,ʵ�ʲ�����Ŀ�ĵ�ַ��������,��ֻ�ǽ�Ŀ���ַ���õ���socket�����,
 *                  �Ժ����ʱͨ��send���ü���,����Ҫ��ʹ��sendmsg
 * @param[in] seconds ���ӳ�ʱ��
 * @return   ���ӵ�socket���
 * @retval -1: ����ʧ��,ͬʱ������errnoֵ.
 * @retval >=0: �����ӵ�socket���
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
    //������Ҫ������¼���ص��ļ�������
    ev.data.fd=sockfd;
    //����Ҫ������¼�����
    ev.events=EPOLLOUT|EPOLLET;
    //ע��epoll�¼�
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
 * @brief        �ر�һ����SocketUtil�ഴ����socket���.
 * @param[in] sockfd  socket���
 * @return
*/
void SocketUtil::safeClose(int sockfd)
{
  if(sockfd >= 0)
    close(sockfd);
}
