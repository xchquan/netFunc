#ifndef __HEAD_NET_FUNC_H
#define __HEAD_NET_FUNC_H

#include <errno.h>
#ifdef _MSC_VER
#include <WinSock2.h>
#include <MSTcpIP.h>
#include <WS2tcpip.h>
#else

#include <fcntl.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#ifndef SOCKET
typedef int SOCKET;
#endif

#define INVALID_SOCKET (SOCKET)(~0)

#ifndef closesocket
#define closesocket(fd)	do	\
{	\
	close(fd);	\
} while (0)
#endif	/// closesocket

inline int	WSAGetLastError(){	return errno;	}

#endif	/// _MSC_VER

#ifdef _MSC_VER
/// ��������ģʽ bNoBlock trueΪ��������falseΪ����
inline	int 	SetNoBlockMode(SOCKET sock, bool bNoBlock)
{
	/// ul���������ģʽ����㣬���ֹ������ģʽ��Ϊ��
	
	u_long	ul = bNoBlock ? 1 : 0; 
	return ioctlsocket(sock,FIONBIO,&ul);
}

#else

inline	int 	SetNoBlockMode(int fd, bool bNoBlock)
{
	int value = fcntl(fd, F_GETFL, 0);
	int ret = value & O_NONBLOCK;
	if(bNoBlock)
	{
		value |= O_NONBLOCK;
	}
	else
	{
		int temp = O_NONBLOCK;
		temp = ~temp;
		value &= temp;
	}

	ret = fcntl(fd, F_SETFL, value);
	return ret;
}
#endif

inline int		SetSocketNoDelay(SOCKET s, bool bNoDelay)
{
	if (!bNoDelay)
		return 0;

	int on = 1;
	/* make socket here */
#ifdef _MSC_VER
	if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(on)) < 0)
		return -1;
#else
	if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (void *)&on, sizeof(on)) < 0)
		return -1;
#endif
	return 0;
}


/// ����fd��keepalivedģʽ
#ifdef _MSC_VER

inline	int		SetKeepAliveMode(SOCKET sock, bool bKeepAlive)
{
	if (!bKeepAlive)
		return 0;

	struct tcp_keepalive keepin;
	struct tcp_keepalive keepout;

	keepin.keepaliveinterval=1000 * 5;	/// 15s ÿ15S����1��̽�ⱨ�ģ���5��û�л�Ӧ���ͶϿ�
	keepin.keepalivetime=1000*60;		/// 60s ����60Sû�����ݣ��ͷ���̽���
	keepin.onoff=1;

	DWORD dwRet = 0;
	return WSAIoctl(sock,SIO_KEEPALIVE_VALS,&keepin,sizeof(keepin),&keepout,sizeof(keepout),&dwRet,NULL,NULL);
}

#else

inline	int		SetKeepAliveMode(int fd, bool bKeepAlive)
{
	if (!bKeepAlive)
		return 0;

	int keepalive = 1; 			///	����keepalive����

	int keepidle = 120; 		///	���������60����û���κ���������,�����̽��

	int keepinterval = 15; 		///	̽��ʱ������ʱ����Ϊ5 ��

	int keepcount = 3; 			///	̽�Ⳣ�ԵĴ���.�����1��̽������յ���Ӧ��,���2�εĲ��ٷ�.

	int iRet = 0;
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive, sizeof(keepalive)) ||
		setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, (void*)&keepidle, sizeof(keepidle)) ||
		setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, (void *)&keepinterval, sizeof(keepinterval)) ||
		setsockopt(fd, SOL_TCP, TCP_KEEPCNT, (void *)&keepcount, sizeof(keepcount))
		) {
		iRet = -1;
	}

	return iRet;
}

#endif


#define		CNT_RECONN_LMT		5

/// �������ӣ����ط�����fd	�����ص�fd���ã�

inline	SOCKET 	ConnectSync(const struct sockaddr* pAddr, socklen_t salen)
{
	SOCKET fd = INVALID_SOCKET;
	int iRet = 0;
	int iCount = CNT_RECONN_LMT;

#ifdef _MSC_VER
	int timeo = 6000;
#else
	struct timeval timeo = {6, 0};
#endif
	socklen_t len = sizeof(timeo);

STARTCONN:
	if ((iCount--) != CNT_RECONN_LMT)
	{
#ifdef _MSC_VER
		Sleep(1000);
#else
		usleep(1000 * 1000);
#endif
	}
	if (iCount < 0)
		return fd;
	
	fd = socket(pAddr->sa_family, SOCK_STREAM, IPPROTO_TCP);

	//���÷��ͳ�ʱ6��
	if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeo, len))
	{
		closesocket(fd);
		fd = INVALID_SOCKET;
	}

	//���ý��ճ�ʱ6��
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeo, len))
	{
		closesocket(fd);
		fd = INVALID_SOCKET;
	}

	if(fd != INVALID_SOCKET)
	{
		//����������,���ӳɹ���,���óɷ�������ʽ
		iRet = connect(fd, pAddr, salen);
		if(iRet)	//<0
		{
			closesocket(fd);
			fd = INVALID_SOCKET;

#ifdef _MSC_VER
			if (WSAGetLastError() == WSAEINTR)
#else
			if (WSAGetLastError() == EINTR)
#endif
				goto STARTCONN;
		}
		else
		{
			SetNoBlockMode(fd, true);
		}
	}
	return fd;
}

/// ���������ӣ����ط�����fd	(���ص�fd��һ�����ã���Ҫͨ���жϲ���ȷ��)

inline	SOCKET	ConnectAsync(const struct sockaddr* pAddr, socklen_t salen)
{
	SOCKET fd = socket(pAddr->sa_family, SOCK_STREAM, IPPROTO_TCP);

	/// timeout 6s
#ifdef _MSC_VER
	int timeo = 6000;
#else
	struct timeval timeo = {6, 0};
#endif
	socklen_t len = sizeof(timeo);

	if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeo, len) <= -1)
	{
		closesocket(fd);
		fd = INVALID_SOCKET;
	}

	//���ý��ճ�ʱ6��
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeo, len) <= -1)
	{
		closesocket(fd);
		fd = INVALID_SOCKET;
	}

	int iRet = 0;
	if(fd != INVALID_SOCKET)
	{
		//����������		
		SetNoBlockMode(fd, true);
		iRet = connect(fd,pAddr,salen);

#ifdef _MSC_VER
		if(iRet < 0 && WSAGetLastError() != EWOULDBLOCK)
#else
		if(iRet < 0 && errno != EINPROGRESS && errno != EWOULDBLOCK)
#endif
		{
			closesocket(fd);
			fd = INVALID_SOCKET;
			return fd;
		}
	}
	return fd;
}

inline	SOCKET	ConnectSync(const char* pszIp,unsigned short uPort)
{
	struct sockaddr_in	addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(uPort);

	SOCKET fd = INVALID_SOCKET;
	unsigned int uIP = inet_addr(pszIp);
	if(INADDR_NONE == uIP)
	{ /// ip��ַΪ����
		int iNum = 0;
		struct hostent	*inhost= NULL;

#ifdef _MSC_VER
		if (!(inhost = gethostbyname(pszIp)))
			return INVALID_SOCKET;
#else
		struct hostent hostinfo;
		char	szAddBuf[8192] = {0};
		if(gethostbyname_r(pszIp, &hostinfo, szAddBuf, 8192, &inhost, &iNum) || !inhost)
			return -1;
#endif

		for(int i = 0; inhost->h_addr_list[i]; i++)
		{
			memcpy(&addr.sin_addr, inhost->h_addr_list[i], inhost->h_length);
			fd = ConnectSync((const sockaddr *)&addr, (socklen_t)sizeof(addr));
			if(fd != INVALID_SOCKET)
				break;
		}
	}
	else
	{ /// ip
		addr.sin_addr.s_addr = inet_addr(pszIp);
		fd = ConnectSync((const sockaddr *)&addr,(socklen_t)sizeof(addr));
	}
	return fd;
}

inline	SOCKET	ConnectAsync(const char* pszIp,unsigned short uPort)
{
	struct sockaddr_in	addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(uPort);

	SOCKET fd = INVALID_SOCKET;
	unsigned int uIP = inet_addr(pszIp);
	if(INADDR_NONE == uIP)
	{
		int iNum = 0;
		struct hostent	*inhost= NULL;

#ifdef _MSC_VER
		if (!(inhost = gethostbyname(pszIp)))
			return INVALID_SOCKET;
#else
		struct hostent hostinfo;
		char	szAddBuf[8192] = {0};
		if(gethostbyname_r(pszIp, &hostinfo, szAddBuf, 8192, &inhost, &iNum) || !inhost)
			return -1;
#endif

		for(int i = 0; inhost->h_addr_list[i]; i++)
		{
			memcpy(&addr.sin_addr, inhost->h_addr_list[i], inhost->h_length);
			fd = ConnectAsync((const sockaddr *)&addr, (socklen_t)sizeof(addr));
			if(fd != INVALID_SOCKET)
				break;
		}
	}
	else
	{ /// ip
		addr.sin_addr.s_addr = inet_addr(pszIp);
		fd = ConnectAsync((const sockaddr *)&addr,(socklen_t)sizeof(addr));
	}
	return fd;
}

/// ͬ����������
// inline	int		RecvSync(SOCKET s,char *pBuf,int iLen,int iTimeOutS,int iTimeOutUs)
// {
// 	struct timeval tv;
// 	tv.tv_sec = iTimeOutS;
// 	tv.tv_usec = iTimeOutUs;
// 
// 	fd_set fsRead;
// 	FD_ZERO(&fsRead);
// 	FD_SET(s,&fsRead);
// 
// 	int ifds = s + 1;
// 	int iRes = 0;
// 
// 	int iCount = 10;
// 	int iRecv = 0;
// 	while (iLen > iRecv && iCount > 0)
// 	{
// 		iRes = select(ifds,&fsRead,NULL,NULL,&tv);
// 		if (iRes < 0)
// 			return iRes;
// 		else if (iRes == 0)
// 			return iRecv;
// 
// 		iRes = recv(s,pBuf + iRecv,iLen - iRecv,0);
// 		if (iRes == 0)
// 			return -1;
// 		else if (iRes < 0)
// 		{
// 
// 		}
// 		else
// 		{
// 			iRecv += iRes;
// 		}
// 	}
// }

#endif	//__HEAD_NET_FUNC_H