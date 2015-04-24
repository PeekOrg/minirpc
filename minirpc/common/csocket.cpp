/*
** Copyright (C) 2014 Wang Yaofu
** All rights reserved.
**
**Author:Wang Yaofu voipman@qq.com
**Description: The source file of class CSocket.
*/
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#ifdef SunOS
#include <sys/sockio.h>
#endif
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "common/csocket.h"
#include "common/localdef.h"
#include "common/clogwriter.h"
#include "common/chttputils.h"
// ---------------------------------------------------------------------------
// CSocket::CSocket()
//
// constructor.
// ---------------------------------------------------------------------------
//
CSocket::CSocket() {
}

// ---------------------------------------------------------------------------
// CSocket::~CSocket()
//
// Destructor.
// ---------------------------------------------------------------------------
//
CSocket::~CSocket() {
}

// ---------------------------------------------------------------------------
// int CSocket::InitData(struct CConfig aConfigInfo)
//
// init operation where local class is created.
// ---------------------------------------------------------------------------
//
int CSocket::InitData() {
    DEBUG_FUNC_NAME("CSocket::InitData");
    DEBUG(LL_ALL, "%s::Begin.", funcName);
    DEBUG(LL_ALL, "%s:End", funcName);
    return 0;
}

// from string ip port to sockaddr_union
int CSocket::InitSockAddrUn(SAddrInfo* aAddrInfo) {
    if (aAddrInfo == NULL) return RET_ERROR;
    DEBUG_FUNC_NAME("CSocket::InitSockAddrUn");
    DEBUG(LL_ALL, "%s:Begin.", funcName);
    memset(&aAddrInfo->iSockUnion, 0, sizeof(union sockaddr_union));
    switch (aAddrInfo->iFamily) {
        case AF_INET6:
            aAddrInfo->iSockUnion.s.sa_family = PF_INET6;
            if (aAddrInfo->iAddr.empty()) {
                aAddrInfo->iSockUnion.sin6.sin6_addr = in6addr_any;
            } else {
                inet_pton(AF_INET6, aAddrInfo->iAddr.c_str(),
                    &aAddrInfo->iSockUnion.sin6.sin6_addr);
            }
            aAddrInfo->iSockUnion.sin6.sin6_port = htons(aAddrInfo->iPort);
            break;
        case AF_INET:
            aAddrInfo->iSockUnion.s.sa_family = PF_INET;
            if (aAddrInfo->iAddr.empty()) {
                aAddrInfo->iSockUnion.sin.sin_addr.s_addr = INADDR_ANY;
            } else {
                aAddrInfo->iSockUnion.sin.sin_addr.s_addr =
                    inet_addr(aAddrInfo->iAddr.c_str());
            }
            aAddrInfo->iSockUnion.sin.sin_port = htons(aAddrInfo->iPort);
            break;
        case AF_UNIX:
            aAddrInfo->iSockUnion.sun.sun_family = AF_LOCAL;
            strncpy(aAddrInfo->iSockUnion.sun.sun_path,
                aAddrInfo->iAddr.c_str(),
                sizeof(aAddrInfo->iSockUnion.sun.sun_path)-1);
            break;
        default:
            LOG(LL_ERROR, "%s:unknown AF family %d.",
                funcName, aAddrInfo->iFamily);
            return RET_ERROR;
    }
    DEBUG(LL_ALL, "%s:End.", funcName);
    return RET_OK;
}


// get port of ipv6 and ipv4.
unsigned short CSocket::SuGetPort(union sockaddr_union* su) {
    switch (su->s.sa_family) {
        case AF_INET:
            return ntohs(su->sin.sin_port);
        case AF_INET6:
            return ntohs(su->sin6.sin6_port);
        default:
            return 0;
    }
}

// sets the port number (host byte order)
void CSocket::SuSetPort(union sockaddr_union* su, unsigned short port) {
    switch (su->s.sa_family) {
    case AF_INET:
        su->sin.sin_port=htons(port);
        break;
    case AF_INET6:
        su->sin6.sin6_port=htons(port);
        break;
    default:
        printf("su_set_port: BUG: unknown address family %d\n",
        su->s.sa_family);
    }
}

// ---------------------------------------------------------------------------
// int CSocket::CreateSocket()
//
// create socket,socket.
// ---------------------------------------------------------------------------
//
int CSocket::CreateSocket(SAddrInfo* aAddrInfo) {
    if (aAddrInfo == NULL) return RET_ERROR;
    DEBUG_FUNC_NAME("CSocket::CreateSocket");
    DEBUG(LL_ALL, "%s:Begin.", funcName);
    if (aAddrInfo->iFamily != AF_INET && aAddrInfo->iFamily != AF_INET6) {
        aAddrInfo->iFamily = AF_INET;
    }
    if (InitSockAddrUn(aAddrInfo) == RET_ERROR) {
        LOG(LL_ERROR, "%s:InitSockAddrUn client error.", funcName);
        return RET_ERROR;
    }
    LOG(LL_DBG, "%s:try to make fd by socket method.", funcName);
    if ((aAddrInfo->iSockId = socket(aAddrInfo->iSockUnion.s.sa_family,
        PROTOCOL(aAddrInfo->iProtocol), 0)) < 0) {
        LOG(LL_ERROR, "%s:socket error.", funcName);
        return RET_ERROR;
    }
    DEBUG(LL_ALL, "%s:End.", funcName);
    return RET_OK;
}

// ---------------------------------------------------------------------------
// int CSocket::ListenServer()
//
// setsockopt,bind and listen.
// ---------------------------------------------------------------------------
//
int CSocket::ListenServer(SAddrInfo* aAddrInfo) {
    if (aAddrInfo == NULL) return RET_ERROR;
    DEBUG_FUNC_NAME("CSocket::ListenTcpServer()");
    DEBUG(LL_ALL, "%s:Begin.", funcName);
    int ret = RET_OK;
    do {
        if (SetReuseAddr(aAddrInfo->iSockId) == RET_ERROR) {
            LOG(LL_ERROR, "%s:setsockopt SO_REUSEADDR or SO_REUSEPORT error.", funcName);
            ret = RET_ERROR;
            break;
        }
        LOG(LL_DBG, "%s:try to bind socket.", funcName);
        if (bind(aAddrInfo->iSockId,
            &aAddrInfo->iSockUnion.s,
            SOCKADDRUN_LEN(aAddrInfo->iSockUnion)) < 0) {
            LOG(LL_ERROR, "%s:bind error.", funcName);
            ret = RET_ERROR;
            break;
        }
        if (aAddrInfo->iProtocol == TCP) {
            LOG(LL_DBG, "%s:try to listen socket.", funcName);
            if (listen(aAddrInfo->iSockId, 511) < 0) {
                LOG(LL_ERROR, "%s:listen error.", funcName);
                ret = RET_ERROR;
                break;
            }
            if (SetLinger(aAddrInfo->iSockId) == RET_ERROR) {
                LOG(LL_ERROR, "%s:SetLinger error.", funcName);
                ret = RET_ERROR;
                break;
            }
        }
    } while (0);
    if (ret == RET_ERROR) {
        close(aAddrInfo->iSockId);
    }
    LOG(LL_VARS, "%s:socket id:[%d]", funcName, aAddrInfo->iSockId);
    return ret;
}

int CSocket::TcpServer(SAddrInfo* aAddrInfo) {
    DEBUG_FUNC_NAME("CSocket::TcpServer()");
    DEBUG(LL_ALL, "%s:Begin.", funcName);
    if (aAddrInfo == NULL) {
        LOG(LL_ERROR, "%s:address is null.", funcName);
        return RET_ERROR;
    }
    if (CreateSocket(aAddrInfo) == RET_ERROR) {
        LOG(LL_ERROR, "%s::create CSocket.socket failed.", funcName);
        return RET_ERROR;
    }
    if (ListenServer(aAddrInfo) == RET_ERROR) {
        LOG(LL_ERROR, "%s::ListenServer.bind.listen failed.", funcName);
        return RET_ERROR;
    }
    DEBUG(LL_ALL, "%s:End.", funcName);
    return RET_OK;
}

int CSocket::Tcp6Server(SAddrInfo* aAddrInfo) {
    DEBUG_FUNC_NAME("CSocket::Tcp6Server()");
    DEBUG(LL_ALL, "%s:Begin.", funcName);
    if (aAddrInfo == NULL) {
        LOG(LL_ERROR, "%s:address is null.", funcName);
        return RET_ERROR;
    }
    aAddrInfo->iFamily = AF_INET6;
    if (CreateSocket(aAddrInfo) == RET_ERROR) {
        LOG(LL_ERROR, "%s::create CSocket.socket failed.", funcName);
        return RET_ERROR;
    }
    if (ListenServer(aAddrInfo) == RET_ERROR) {
        LOG(LL_ERROR, "%s::ListenServer.bind.listen failed.", funcName);
        return RET_ERROR;
    }
    DEBUG(LL_ALL, "%s:End.", funcName);
    return RET_OK;
}

int CSocket::TcpUnixServer(SAddrInfo* aAddrInfo) {
    DEBUG_FUNC_NAME("CSocket::TcpUnixServer()");
    DEBUG(LL_ALL, "%s:Begin.", funcName);
    if (aAddrInfo == NULL) {
        LOG(LL_ERROR, "%s:address is null.", funcName);
        return RET_ERROR;
    }
    if ((aAddrInfo->iSockId = socket(AF_LOCAL,
        PROTOCOL(aAddrInfo->iProtocol), 0)) < 0) {
        LOG(LL_ERROR, "%s:socket error.", funcName);
        return RET_ERROR;
    }
    aAddrInfo->iSockUnion.sun.sun_family = AF_LOCAL;
    strncpy(aAddrInfo->iSockUnion.sun.sun_path,
        aAddrInfo->iAddr.c_str(), sizeof(aAddrInfo->iSockUnion.sun.sun_path)-1);
    if (ListenServer(aAddrInfo) == RET_ERROR) {
        LOG(LL_ERROR, "%s::ListenServer.bind.listen failed.", funcName);
        return RET_ERROR;
    }
    DEBUG(LL_ALL, "%s:End.", funcName);
    return RET_OK;
}
// ---------------------------------------------------------------------------
// int CSocket::TcpConnect()
//
// connect.
// ---------------------------------------------------------------------------
//
int CSocket::TcpConnect(const SAddrInfo& aAddrInfo) {
    DEBUG_FUNC_NAME("CSocket::TcpConnect");
    LOG(LL_DBG, "%s:try to connect sockid:(%d).",
        funcName, aAddrInfo.iSockId);
    if (connect(aAddrInfo.iSockId, &(aAddrInfo.iSockUnion.s),
        SOCKADDRUN_LEN(aAddrInfo.iSockUnion)) < 0) {
        close(aAddrInfo.iSockId);
        LOG(LL_ERROR, "%s:connect sockid:(%d),error:(%s).",
            funcName, aAddrInfo.iSockId, strerror(errno));
        return RET_ERROR;
    }
    LOG(LL_DBG, "%s:connect sockid:(%d) success.",
        funcName, aAddrInfo.iSockId);
    return RET_OK;
}

// ---------------------------------------------------------------------------
// int CSocket::MakeTcpConn(struct SAddrInfo &aAddrInfo)
//
// make connect to tcp server.
// ---------------------------------------------------------------------------
//
int CSocket::MakeTcpConn(SAddrInfo* aAddrInfo) {
    DEBUG_FUNC_NAME("CSocket::MakeTcpConn");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    if (aAddrInfo == NULL) {
        LOG(LL_ERROR, "%s:address is null.", funcName);
        return RET_ERROR;
    }
    DEBUG(LL_DBG, "%s:try to create socket.", funcName);
    if (RET_ERROR == CreateSocket(aAddrInfo)) {
        LOG(LL_ERROR, "%s:CreateSocket.ip:(%s),port:(%d),error:(%s).",
            funcName, aAddrInfo->iAddr.c_str(),
            aAddrInfo->iPort, strerror(errno));
        return RET_ERROR;
    }
    int tryTime = 0, ret = RET_ERROR;
    do {
        DEBUG(LL_DBG, "%s:try to connect ip:(%s), port:(%d), trytime:(%d).",
            funcName, aAddrInfo->iAddr.c_str(), aAddrInfo->iPort, tryTime);
        ret = TcpConnect(*aAddrInfo);
    } while (ret == RET_ERROR && tryTime++ < 3);
    if (RET_ERROR == ret) {
        LOG(LL_ERROR, "%s:TcpConnect.ip:(%s),port:(%d),error:(%s).",
            funcName, aAddrInfo->iAddr.c_str(),
            aAddrInfo->iPort, strerror(errno));
        return RET_ERROR;
    }
    DEBUG(LL_ALL, "%s:End", funcName);
    return RET_OK;
}

int CSocket::MakeUnixConnect(SAddrInfo* aAddrInfo) {
    DEBUG_FUNC_NAME("CSocket::MakeUnixConnect");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    if (aAddrInfo == NULL) {
        LOG(LL_ERROR, "%s:address is null.", funcName);
        return RET_ERROR;
    }
    aAddrInfo->iFamily = AF_LOCAL;
    DEBUG(LL_DBG, "%s:try to create socket.", funcName);
    if (RET_ERROR == CreateSocket(aAddrInfo)) {
        LOG(LL_ERROR, "%s:CreateSocket.unix_path:(%s),port:(%d),error:(%s).",
            funcName, aAddrInfo->iAddr.c_str(),
            aAddrInfo->iPort, strerror(errno));
        return RET_ERROR;
    }
    int tryTime = 0, ret = RET_ERROR;
    do {
        DEBUG(LL_DBG, "%s:try to connect ip:(%s), port:(%d), trytime:(%d).",
            funcName, aAddrInfo->iAddr.c_str(), aAddrInfo->iPort, tryTime);
        ret = TcpConnect(*aAddrInfo);
    } while (ret == RET_ERROR && tryTime++ < 3);
    if (RET_ERROR == ret) {
        LOG(LL_ERROR, "%s:TcpConnect.ip:(%s),port:(%d),error:(%s).",
            funcName, aAddrInfo->iAddr.c_str(),
            aAddrInfo->iPort, strerror(errno));
        return RET_ERROR;
    }
    DEBUG(LL_ALL, "%s:End", funcName);
    return RET_OK;
}

// ---------------------------------------------------------------------------
// int CSocket::AcceptConnection()
//
// accept socket, the client is added into host queue when new conn is coming.
// ---------------------------------------------------------------------------
//
int CSocket::AcceptConnection(int aSockId, SAddrInfo* aClntAddrInfo) {
    DEBUG_FUNC_NAME("CSocket::AcceptConnection()");
    DEBUG(LL_ALL, "%s:Begin.", funcName);
    int clntSock = RET_ERROR;

#ifdef HPUX
    int clntlen;
#else
    socklen_t clntlen;
#endif
    clntlen = sizeof(aClntAddrInfo->iSockUnion);
    do {
        clntSock = accept(aSockId, &(aClntAddrInfo->iSockUnion.s), &clntlen);
    } while (clntSock < 0 && errno == EINTR);
    if (clntSock < 0) {
        LOG(LL_ERROR, "%s:Accept socket fail, sockid:(%d),error:(%s)",
            funcName, aSockId, strerror(errno));
        return RET_ERROR;
    }
    SAddrInfo2String(aClntAddrInfo);
    aClntAddrInfo->iSockId = clntSock;
    LOG(LL_WARN, "%s:new client ip:(%s),port:(%d),sockid:(%d)",
        funcName, aClntAddrInfo->iAddr.c_str(),
        aClntAddrInfo->iPort, aClntAddrInfo->iSockId);
    DEBUG(LL_ALL, "%s:End.", funcName);
    return RET_OK;
}

// ---------------------------------------------------------------------------
// int CSocket::SAddrInfo2String()
//
// SAddrInfo to ip:port string:int.
// ---------------------------------------------------------------------------
//
int CSocket::SAddrInfo2String(SAddrInfo* aClntAddrInfo) {
    if (aClntAddrInfo == NULL) return RET_ERROR;
    DEBUG_FUNC_NAME("CSocket::SAddrInfo2String()");
    DEBUG(LL_ALL, "%s:Begin.", funcName);
    char clnIpBuf[64] = {0};
    if (aClntAddrInfo->iSockUnion.s.sa_family == AF_INET) {
        inet_ntop(AF_INET, &(aClntAddrInfo->iSockUnion.sin.sin_addr),
            clnIpBuf, sizeof(clnIpBuf));
        aClntAddrInfo->iPort = ntohs(aClntAddrInfo->iSockUnion.sin.sin_port);
    } else {
        inet_ntop(AF_INET6, &(aClntAddrInfo->iSockUnion.sin6.sin6_addr),
            clnIpBuf, sizeof(clnIpBuf));
        aClntAddrInfo->iPort = ntohs(aClntAddrInfo->iSockUnion.sin6.sin6_port);
    }
    aClntAddrInfo->iAddr = clnIpBuf;
    DEBUG(LL_ALL, "%s:End.", funcName);
    return RET_OK;
}

// host name is ip address to ipv4 or ipv6 if aIsIPOnly is ture.
// otherwise, host name is normal string instead of ip address format.
bool CSocket::HostResolve(const char* aInHost, char *aOutIP, int aOutSize, bool aIsIPOnly) {
    if (aInHost == NULL || aOutIP == NULL) return false;
    struct addrinfo hints, *info;
    int rv;

    memset(&hints, 0, sizeof(hints));
    if (aIsIPOnly) {
        //IP_ONLY
        hints.ai_flags = AI_NUMERICHOST;
    }
    hints.ai_family = AF_UNSPEC;
    // specify socktype to avoid dups
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(aInHost, NULL, &hints, &info)) != 0) {
        LOG(LL_ERROR, "CSocket:Accept getaddrinfo fail error:(%s)",
            gai_strerror(rv));
        return false;
    }
    if (info->ai_family == AF_INET) {
        struct sockaddr_in *sa = (struct sockaddr_in *)info->ai_addr;
        inet_ntop(AF_INET, &(sa->sin_addr), aOutIP, aOutSize);
    } else {
        struct sockaddr_in6 *sa = (struct sockaddr_in6 *)info->ai_addr;
        inet_ntop(AF_INET6, &(sa->sin6_addr), aOutIP, aOutSize);
    }

    freeaddrinfo(info);
    return true;
}

// ---------------------------------------------------------------------------
// bool CSocket::Name2IP()
//
// DNS to ip address.
// ---------------------------------------------------------------------------
//
bool CSocket::Name2IPv4(const char* aInHost, char *aOutIPv4) {
    if ((int)(inet_addr(aInHost)) == -1) {
        struct hostent *hp;
        struct in_addr in;
        hp = gethostbyname(aInHost);
        if (hp == NULL) {
            return false;
        }
        memcpy(&in.s_addr, *(hp->h_addr_list), sizeof(in.s_addr));
        strcpy(aOutIPv4, inet_ntoa(in));
    }
    return true;
}

// whether ipstr with ipv4 format x.x.x.x.
bool CSocket::IsIPv4Addr(const string &aIpStr) {
    struct in_addr addr;
    if (inet_aton(aIpStr.c_str(), &addr) != 0)
        return true;
    else
        return false;
}

// whether ipstr with ipv6 format
bool CSocket::IsIPv6Addr(const string &aIpStr) {
    struct in6_addr addr;
    if (inet_pton(AF_INET6, aIpStr.c_str(), &addr) != 0)
        return true;
    else
        return false;
}

// ---------------------------------------------------------------------------
// int CSocket::NetPeerToStr(int fd, char *aIp, size_t aIpLen, int *aPort)
//
// get remote IP and Port by socket id.
// ---------------------------------------------------------------------------
//
int CSocket::NetPeerToStr(int fd, char *aIp, size_t aIpLen, int *aPort) {
    struct sockaddr_storage sa;
    #ifdef HPUX
    int salen = sizeof(sa);
    #else
    socklen_t salen = sizeof(sa);
    #endif

    if (getpeername(fd, (struct sockaddr*)&sa, &salen) == -1) {
        if (aPort) *aPort = 0;
        aIp[0] = '?';
        aIp[1] = '\0';
        return RET_ERROR;
    }
    if (aPort) *aPort = 0;
    if (sa.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&sa;
        if (aIp) inet_ntop(AF_INET, (void*)&(s->sin_addr), aIp, aIpLen);
        if (aPort) *aPort = ntohs(s->sin_port);
    } else if (sa.ss_family == AF_INET6) {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&sa;
        if (aIp) inet_ntop(AF_INET6, (void*)&(s->sin6_addr), aIp, aIpLen);
        if (aPort) *aPort = ntohs(s->sin6_port);
    } else if (sa.ss_family == AF_UNIX) {
        if (aIp) strncpy(aIp, "/unixsocket", aIpLen);
    } else {
        aIp[0] = '?';
        aIp[1] = '\0';
        return RET_ERROR;
    }
    return RET_OK;
}

// ---------------------------------------------------------------------------
// int CSocket::NetPeerToStr(int fd, char *aIp, size_t ip_len, int *aPort)
//
// get local IP and Port by socket id.
// ---------------------------------------------------------------------------
//
int CSocket::NetLocalSockName(int aSockId, char *aIp, size_t aIpLen, int *aPort) {
    struct sockaddr_storage sa;
    #ifdef HPUX
    int salen = sizeof(sa);
    #else
    socklen_t salen = sizeof(sa);
    #endif

    if (getsockname(aSockId, (struct sockaddr*)&sa, &salen) == -1) {
        if (aPort) *aPort = 0;
        aIp[0] = '?';
        aIp[1] = '\0';
        return RET_ERROR;
    }
    if (sa.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&sa;
        if (aIp) inet_ntop(AF_INET, (void*)&(s->sin_addr), aIp, aIpLen);
        if (aPort) *aPort = ntohs(s->sin_port);
    } else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&sa;
        if (aIp) inet_ntop(AF_INET6, (void*)&(s->sin6_addr), aIp, aIpLen);
        if (aPort) *aPort = ntohs(s->sin6_port);
    }
    return RET_OK;
}

// ---------------------------------------------------------------------------
// int CSocket::NonBlock(int aSockId)
//
// fcntl socket id to NO_BLOCK.
// ---------------------------------------------------------------------------
//
int CSocket::NonBlock(int aSockId, bool aIsNonBlock) {
    DEBUG_FUNC_NAME("CSocket::NonBlock");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    /* Set the socket non-blocking.
     * Note that fcntl(2) for F_GETFL and F_SETFL can't be
     * interrupted by a signal. */
    int flags;
    if ((flags = fcntl(aSockId, F_GETFL)) == RET_ERROR) {
        LOG(LL_ERROR, "%s:fcntl(F_GETFL): %s",
            funcName, strerror(errno));
        return RET_ERROR;
    }
    if (aIsNonBlock) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    if (fcntl(aSockId, F_SETFL, flags) == RET_ERROR) {
        LOG(LL_ERROR, "%s:fcntl(F_SETFL,O_NONBLOCK): %s",
            funcName, strerror(errno));
        return RET_ERROR;
    }
    DEBUG(LL_ALL, "%s:End", funcName);
    return RET_OK;
}

/* Set TCP keep alive option to detect dead peers. The interval option
 * is only used for Linux as we are using Linux-specific APIs to set
 * the probe send time, interval, and count. */
int CSocket::KeepAlive(int aSockId, int aInterval) {
    DEBUG_FUNC_NAME("CSocket::KeepAlive");
    DEBUG(LL_ALL, "%s:End", funcName);
    LOG(LL_VARS, "%s:sockid:(%d), interval:(%d)",
        funcName, aSockId, aInterval);
    int val = 1;
    if (setsockopt(aSockId, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val)) == -1) {
        LOG(LL_ERROR, "%s:setsockopt SO_KEEPALIVE: error:(%s).",
            funcName, strerror(errno));
        return RET_ERROR;
    }

#ifdef __linux__
    /* Default settings are more or less garbage, with the keepalive time
     * set to 7200 by default on Linux. Modify settings to make the feature
     * actually useful. */

    /* Send first probe after interval. */
    val = aInterval;
    if (setsockopt(aSockId, IPPROTO_TCP, TCP_KEEPIDLE, &val, sizeof(val)) < 0) {
        LOG(LL_ERROR, "%s:setsockopt TCP_KEEPIDLE: error:(%s).",
            funcName, strerror(errno));
        return RET_ERROR;
    }

    /* Send next probes after the specified interval. Note that we set the
     * delay as interval / 3, as we send three probes before detecting
     * an error (see the next setsockopt call). */
    val = aInterval/3;
    if (val == 0) val = 1;
    if (setsockopt(aSockId, IPPROTO_TCP, TCP_KEEPINTVL, &val, sizeof(val)) < 0) {
        LOG(LL_ERROR, "%s:setsockopt TCP_KEEPINTVL: error:(%s).",
            funcName, strerror(errno));
        return RET_ERROR;
    }

    /* Consider the socket in error state after three we send three ACK
     * probes without getting a reply. */
    val = 3;
    if (setsockopt(aSockId, IPPROTO_TCP, TCP_KEEPCNT, &val, sizeof(val)) < 0) {
        LOG(LL_ERROR, "%s:setsockopt TCP_KEEPCNT: error:(%s).",
            funcName, strerror(errno));
        return RET_ERROR;
    }
#endif
    DEBUG(LL_ALL, "%s:End", funcName);
    return RET_OK;
}

// ---------------------------------------------------------------------------
// int CSocket::SetTcpNoDelay(int aSockId, int aVal)
//
// set tcp socket no delay.
// aVal=1:enable, aVal=1:disable
// ---------------------------------------------------------------------------
//
int CSocket::SetTcpNoDelay(int aSockId, int aVal) {
    DEBUG_FUNC_NAME("CSocket::SetTcpNoDelay");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    LOG(LL_VARS, "%s:sockid:(%d), val:(%d)",
        funcName, aSockId, aVal);
    if (setsockopt(aSockId, IPPROTO_TCP, TCP_NODELAY,
        &aVal, sizeof(aVal)) == -1) {
        LOG(LL_ERROR, "%s:setsockopt TCP_NODELAY: error:(%s).",
            funcName, strerror(errno));
        return RET_ERROR;
    }
    DEBUG(LL_ALL, "%s:End", funcName);
    return RET_OK;
}

// ---------------------------------------------------------------------------
// int CSocket::SetSendBuffer(int aSockId, int aBuffSize)
//
// set socket send buffer size.
// ---------------------------------------------------------------------------
//
int CSocket::SetSendBuffer(int aSockId, int aBuffSize) {
    DEBUG_FUNC_NAME("CSocket::SetSendBuffer");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    LOG(LL_VARS, "%s:sockid:(%d), buffersize:(%d)",
        funcName, aSockId, aBuffSize);
    if (setsockopt(aSockId, SOL_SOCKET, SO_SNDBUF,
        &aBuffSize, sizeof(aBuffSize)) == -1) {
        LOG(LL_ERROR, "%s:setsockopt SO_SNDBUF: error:(%s).",
            funcName, strerror(errno));
        return RET_ERROR;
    }
    DEBUG(LL_ALL, "%s:End", funcName);
    return RET_OK;
}

// ---------------------------------------------------------------------------
// int CSocket::SetRecvBuffer(int aSockId, int aBuffSize)
//
// set socket recv buffer size.
// ---------------------------------------------------------------------------
//
int CSocket::SetRecvBuffer(int aSockId, int aBuffSize) {
    DEBUG_FUNC_NAME("CSocket::SetRecvBuffer");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    LOG(LL_VARS, "%s:sockid:(%d), buffersize:(%d)",
        funcName, aSockId, aBuffSize);
    if (setsockopt(aSockId, SOL_SOCKET, SO_RCVBUF,
        &aBuffSize, sizeof(aBuffSize)) == -1) {
        LOG(LL_ERROR, "%s:setsockopt SO_RCVBUF: error:(%s).",
            funcName, strerror(errno));
        return RET_ERROR;
    }
    DEBUG(LL_ALL, "%s:End", funcName);
    return RET_OK;
}

int CSocket::SetBroadCast(int aSockId, int aBroadCast) {
    DEBUG_FUNC_NAME("CSocket::SetBroadCast");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    LOG(LL_VARS, "%s:sockid:(%d), broad cast flag:(%d)",
        funcName, aSockId, aBroadCast);
    if (setsockopt(aSockId, SOL_SOCKET, SO_BROADCAST,
        &aBroadCast, sizeof(aBroadCast)) < 0) {
        LOG(LL_ERROR, "%s:setsockopt SO_BROADCAST: error:(%s).",
            funcName, strerror(errno));
        return RET_ERROR;
    }
    DEBUG(LL_ALL, "%s:End", funcName);
    return RET_OK;
}

// ---------------------------------------------------------------------------
// int CSocket::SetMultiCast()
//
// aFlag = 1:add into group. aFalg=0:drop from group.
// ---------------------------------------------------------------------------
//
int CSocket::SetMultiCast(int aSockId, EMultiCaseOption aFlag,
                          const string& aMultiAddress) {
    DEBUG_FUNC_NAME("CSocket::SetMultiCast");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    LOG(LL_VARS, "%s:sockid:(%d), multi cast flag:(%d), address:(%s)",
        funcName, aSockId, aFlag, aMultiAddress.c_str());
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(aMultiAddress.c_str());
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (aFlag == kJoinMultiCast) {
        // add into group.
        if(setsockopt(aSockId, IPPROTO_IP, IP_ADD_MEMBERSHIP,
            &mreq, sizeof(mreq)) < 0) {
            LOG(LL_ERROR, "%s:setsockopt IP_ADD_MEMBERSHIP: error:(%s).",
                funcName, strerror(errno));
            return RET_ERROR;
        }
    } else {
        // drop membership.
        if(setsockopt(aSockId, IPPROTO_IP, IP_DROP_MEMBERSHIP,
            &mreq, sizeof(mreq)) < 0) {
            LOG(LL_ERROR, "%s:setsockopt IP_DROP_MEMBERSHIP: error:(%s).",
                funcName, strerror(errno));
            return RET_ERROR;
        }
    }
    DEBUG(LL_ALL, "%s:End", funcName);
    return RET_OK;
}

// aFlag = 1:add into group. aFalg=0:drop from group.
// aIfName is interface name, such as 'etho'.
int CSocket::SetMultiCastIpv6(int aSockId, EMultiCaseOption aFlag,
    const struct sockaddr_in6 *aSin6, const char *aIfName) {
    DEBUG_FUNC_NAME("CSocket::SetMultiCastIpv6");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    LOG(LL_VARS, "%s:sockid:(%d), multi cast flag:(%d)",
        funcName, aSockId, aFlag);
    struct ipv6_mreq mreq;
    if (aIfName != NULL && aIfName[0] != 0x00) {
        if ((mreq.ipv6mr_interface = if_nametoindex(aIfName)) == 0) {
            errno = ENXIO;
            return RET_ERROR;
        }
    } else {
        mreq.ipv6mr_interface = 0;
    }
    memcpy(&mreq.ipv6mr_multiaddr, &aSin6->sin6_addr, sizeof(struct in6_addr));
    if (aFlag == kJoinMultiCast) {
        // add into group.
        if(setsockopt(aSockId, IPPROTO_IPV6, IPV6_JOIN_GROUP,
            &mreq, sizeof(mreq)) < 0) {
            LOG(LL_ERROR, "%s:setsockopt IPV6_JOIN_GROUP: error:(%s).",
                funcName, strerror(errno));
            return RET_ERROR;
        }
    } else {
        // drop membership.
        if(setsockopt(aSockId, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
            &mreq, sizeof(mreq)) < 0) {
            LOG(LL_ERROR, "%s:setsockopt IPV6_LEAVE_GROUP: error:(%s).",
                funcName, strerror(errno));
            return RET_ERROR;
        }
    }
    DEBUG(LL_ALL, "%s:End", funcName);
    return RET_OK;
}

// SO_REUSEADDR or SO_REUSEPORT is set for TCP
int CSocket::SetReuseAddr(int aSockId) {
    int opt = 1;
#if defined(HPUX) || defined(AIX)
    if (setsockopt(aSockId, SOL_SOCKET, SO_REUSEPORT,
        (char *)&opt,sizeof(opt)) < 0)
#else
    if (setsockopt(aSockId, SOL_SOCKET, SO_REUSEADDR,
        (char *)&opt,sizeof(opt)) < 0)
#endif
    {
        return RET_ERROR;
    }
    return RET_OK;
}
// ---------------------------------------------------------------------------
// int CSocket::SetLinger()
//
// SO_LINGER is set for TCP.
// ---------------------------------------------------------------------------
//
int CSocket::SetLinger(int aSockId) {
    DEBUG_FUNC_NAME("CSocket::SetLinger");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    LOG(LL_VARS, "%s:sockid:(%d)", funcName, aSockId);
    struct linger l;
    l.l_onoff=1;
    l.l_linger=0;
    if (setsockopt(aSockId, SOL_SOCKET, SO_LINGER, (char *) &l, sizeof(l)) < 0) {
        LOG(LL_ERROR, "%s:setsockopt SO_LINGER error:(%s).",
            funcName, strerror(errno));
        close(aSockId);
        return RET_ERROR;
    }
    DEBUG(LL_ALL, "%s:End", funcName);
    return RET_OK;
}

int CSocket::SetV6Only(int aSockId) {
    DEBUG_FUNC_NAME("CSocket::SetLinger");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    DEBUG(LL_VARS, "%s:sockid:(%d)", funcName, aSockId);
    int yes = 1;
    if (setsockopt(aSockId, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(yes)) < 0) {
        LOG(LL_ERROR, "%s:setsockopt SO_LINGER error:(%s).",
            funcName, strerror(errno));
        close(aSockId);
        return RET_ERROR;
    }
    DEBUG(LL_ALL, "%s:End", funcName);
    return RET_OK;
}

// ---------------------------------------------------------------------------
// int CSocket::SetSockTimeOut(int aSockId, int aFlag, int aTimeOut)
//
// SO_RCVTIMEO and SO_SNDTIMEO are set for UDP.
// ---------------------------------------------------------------------------
//
int CSocket::SetSockTimeOut(int aSockId, int aFlag, int aTimeOut) {
    DEBUG_FUNC_NAME("CSocket::SetSockTimeOut");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    LOG(LL_VARS, "%s:sockid:(%d),flag:(%d),timeout:(%d)",
        funcName, aSockId, aFlag, aTimeOut);
    if (aTimeOut < 0) {
        aTimeOut = FD_WAIT_MS;
    }

    struct timeval tv_out;
    tv_out.tv_sec = aTimeOut/1000;
    tv_out.tv_usec = (aTimeOut % 1000) * 1000;
    if(tv_out.tv_usec >= 1000000) {
        tv_out.tv_sec++;
        tv_out.tv_usec -= 1000000;
    }
    if (aFlag & FD_SET_SO_RCVTIMEO) {
        // setsockopt(aSockId, SOL_SOCKET, SO_RCVTIMEO, &aTimeOut, sizeof(aTimeOut)
        if (setsockopt(aSockId, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out)) < 0) {
            LOG(LL_ERROR, "%s:setsockopt SO_RCVTIMEO error:(%s).",
                funcName, strerror(errno));
            close(aSockId);
            return RET_ERROR;
        }
    }
    if (aFlag & FD_SET_SO_SNDTIMEO) {
        if (setsockopt(aSockId, SOL_SOCKET, SO_SNDTIMEO, &tv_out, sizeof(tv_out)) < 0) {
            LOG(LL_ERROR, "%s:setsockopt SO_SNDTIMEO error:(%s).",
                funcName, strerror(errno));
            close(aSockId);
            return RET_ERROR;
        }
    }
    DEBUG(LL_ALL, "%s:End", funcName);
    return RET_OK;
}

// ---------------------------------------------------------------------------
// int CSocket::NetRead(int aSockId, char *aBuf, int aCount)
//
// Like read(2) but make sure 'aCount' is read before to return
// (unless error or EOF condition is encountered)
// ---------------------------------------------------------------------------
//
int CSocket::NetRead(int aSockId, char *aBuf, int aCount) {
    if (aBuf == NULL) return RET_ERROR;
    int nread =0 , totlen = 0;
    while (totlen != aCount) {
        nread = read(aSockId, aBuf+totlen, aCount-totlen);
        if (nread == 0) return totlen;
        if (nread == -1) return RET_ERROR;
        totlen += nread;
    }
    aBuf[totlen] = 0x00;
    return totlen;
}

// ---------------------------------------------------------------------------
// int CSocket::NetWrite(int aSockId, char *aBuf, int aCount)
//
// Like write(2) but make sure 'aCount' is read before to return
// (unless error is encountered)
// ---------------------------------------------------------------------------
//
int CSocket::NetWrite(int aSockId, const char* aBuf, int aCount) {
    if (aBuf == NULL) return RET_ERROR;
    int nwritten =0, totlen = 0;
    while (totlen != aCount) {
        nwritten = write(aSockId, aBuf+totlen, aCount-totlen);
        if (nwritten == 0) return totlen;
        if (nwritten == -1) return RET_ERROR;
        totlen += nwritten;
    }
    return totlen;
}

// ---------------------------------------------------------------------------
// SelectSockId(int aSockId, int aTimeOut, int aMask)
//
// check socket id whether event is occurred.
// ---------------------------------------------------------------------------
//
int CSocket::SelectSockId(int aSockId, int aTimeOut, int aMask) {
    DEBUG_FUNC_NAME("CSocket::SelectSockId");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    if (aSockId < 0 || aMask == FD_NONE) {
        LOG(LL_ERROR, "%s:bad socket id:(%d).", funcName, aSockId);
        return RET_ERROR;
    }
    if (aTimeOut < 0) aTimeOut = FD_WAIT_MS;

    DEBUG(LL_VARS, "%s:sockid:(%d),timeout:(%d),flag:(%d)",
        funcName, aSockId, aTimeOut, aMask);

    //init result check
    bool bRead  = (aMask & FD_READABLE) ? true : false;
    bool bWrite = (aMask & FD_WRITABLE) ? true : false;
    bool bExcep = (aMask & FD_RWERROR)  ? true : false;

    struct timeval tval, *tvalp;
    fd_set rset, wset, eset;
    fd_set *rsetptr = NULL;
    fd_set *wsetptr = NULL;
    fd_set *esetptr = NULL;
    // using tick hz is instead of gettimebyday when EINTR == errno.
    unsigned int TimeOutHz = aTimeOut / 16;
    while (true) {
        // set select timeout
        tvalp = NULL;
        if (aTimeOut > 0) {
            tval.tv_sec = aTimeOut/1000;
            tval.tv_usec = (aTimeOut % 1000) * 1000;
            if(tval.tv_usec >= 1000000) {
                tval.tv_sec++;
                tval.tv_usec -= 1000000;
            }
            tvalp = &tval;
        } else {
            LOG(LL_ERROR, "%s:Socket time out is occurred.", funcName);
            return RET_ERROR;
        }
        // reset the fd set pointer
        rsetptr = NULL;
        wsetptr = NULL;
        esetptr = NULL;

        if (bRead) {
            FD_ZERO(&rset);
            FD_SET(aSockId, &rset);
            rsetptr = &rset;
        }
        if (bWrite) {
            FD_ZERO(&wset);
            FD_SET(aSockId, &wset);
            wsetptr = &wset;
        }
        if (bExcep) {
            FD_ZERO(&eset);
            FD_SET(aSockId, &eset);
            esetptr = &eset;
        }
        int ret = select(aSockId+1, rsetptr, wsetptr, esetptr, tvalp);
        if (0 == ret) {
            // timeout, result is set to 0
            LOG(LL_ERROR, "%s:select time out error.", funcName);
            return RET_ERROR;
        } else if(0 > ret) {
            // check errno
            if(EINTR == errno) {
                LOG(LL_WARN, "%s:EINTR is occurred.", funcName);
                aTimeOut -= TimeOutHz;
                continue;
            }
            LOG(LL_ERROR, "%s:select:(%d) other error:(%s)",
                funcName, ret, strerror(errno));
            // error ocurred, but not EINTR
            return RET_ERROR;
        } // end if(0 == ret)

        // select ok, break the loop
        break;
    } // end while

    DEBUG(LL_INFO, "%s:select returned.", funcName);
    //clear the result
    int result = 0;

    //check the read fd set
    if (bRead && FD_ISSET(aSockId, &rset)) {
        //select read ok
        DEBUG(LL_INFO, "%s:select read set is ok.", funcName);
        result |= FD_READABLE;
    }

    //check the write fd set
    if (bWrite && FD_ISSET(aSockId, &wset)) {
        //select write ok
        DEBUG(LL_INFO, "%s:select write set is ok.", funcName);
        result |= FD_WRITABLE;
    }

    //check the exception fd set
    if (bExcep && FD_ISSET(aSockId, &eset)) {
        //select exception ok
        DEBUG(LL_INFO, "%s:select exception set is ok.", funcName);
        result |= FD_RWERROR;
    }
    DEBUG(LL_INFO, "%s:return result=%d", funcName, result);
    return result;
}

// Wait for milliseconds until the given file descriptor becomes
// writable/readable/exception
int CSocket::PollSockId(int aSockId, long long aMillisecs, int aMask) {
    struct pollfd pfd;
    int retmask = 0, retval;

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = aSockId;
    if (aMask & FD_READABLE) pfd.events |= POLLIN;
    if (aMask & FD_WRITABLE) pfd.events |= POLLOUT;

    if ((retval = poll(&pfd, 1, aMillisecs))== 1) {
        if (pfd.revents & POLLIN) retmask |= FD_READABLE;
        if (pfd.revents & POLLOUT) retmask |= FD_WRITABLE;
        if (pfd.revents & POLLERR) retmask |= FD_WRITABLE;
        if (pfd.revents & POLLHUP) retmask |= FD_WRITABLE;
        return retmask;
    } else {
        return retval;
    }
}

#define TSEND_RECV_INIT(ev) \
    int n = 0; \
struct pollfd pf; \
    unsigned int expire, ticks_raw; \
    signed short diff; \
    short event; \
    ticks_raw = TIMER_TICKS_HZ; \
    expire = ticks_raw + MS_TO_TICKS((unsigned int)timeout); \
    event = ev; \
    pf.fd=fd; \
    pf.events=event

#define TSEND_RECV_POLL(f_name) \
poll_loop: \
    while(1){ \
        if (timeout==-1) \
            n=poll(&pf, 1, -1); \
        else{ \
            diff=expire-ticks_raw; \
            if (diff<=0){ \
                LOG(LL_ERROR, "ERROR: " f_name ": timeout (%d)\n", timeout);\
                goto error; \
            } \
            n=poll(&pf, 1, TICKS_TO_MS((unsigned int)diff)); \
        } \
        if (n<0){ \
            if (errno==EINTR) continue; \
                LOG(LL_ERROR, "ERROR: " f_name ": poll failed: %s [%d]\n", \
                    strerror(errno), errno); \
            goto error; \
        } else if (n==0){ \
            LOG(LL_ERROR, "ERROR: " f_name ": timeout (p %d)\n", timeout); \
            goto error; \
        } \
        if (pf.revents&event){ \
            goto again; \
        } else if (pf.revents&(POLLERR|POLLHUP|POLLNVAL)){ \
            LOG(LL_ERROR, "ERROR: " f_name ": bad poll flags %x\n", \
            pf.revents); \
            goto error; \
        } \
    }

#define TSEND_RECV_ERR_CHECK(f_name)\
    if (n<0){ \
        if (errno==EINTR) goto again; \
        else if (errno!=EAGAIN && errno!=EWOULDBLOCK){ \
            LOG(LL_ERROR, "ERROR: " f_name ": failed: (%d) %s\n", \
                errno, strerror(errno)); \
            goto error; \
        } else goto poll_loop; \
    }

int CSocket::UdpSend(int fd, char* buffer, int b_size,
                     int timeout, union sockaddr_union* su_cliaddr) {
    if (buffer == NULL || b_size < 0 || su_cliaddr == NULL) {
        LOG(LL_ERROR,
            "CSocket::UdpSend:buffer is null or size:(%d) less than zero.",
            b_size);
        return RET_ERROR;
    }
    DEBUG_HEX_ASC(LL_VARS, buffer, b_size);
    TSEND_RECV_INIT(POLLOUT);
again:
    n = sendto(fd, buffer, b_size, 0,
        &((*su_cliaddr).s), sizeof(*su_cliaddr));
    TSEND_RECV_ERR_CHECK("UdpSend");
    return n;
    TSEND_RECV_POLL("UdpSend");
error:
    LOG(LL_ERROR, "UdpSend::while sending packet: %s",
        strerror(errno));
    return RET_ERROR;
}

int CSocket::UdpRecv(int fd, char *pkg_buffer, int b_size,
                     int timeout, union sockaddr_union* su_cliaddr) {
    if (pkg_buffer == NULL || b_size < 0 || su_cliaddr == NULL) {
        LOG(LL_ERROR,
            "CSocket::UdpRecv:buffer is null or size:(%d) less than zero.",
            b_size);
        return RET_ERROR;
    }
    DEBUG_FUNC_NAME("CSocket::UdpRecv");
    DEBUG(LL_ALL, "%s:Begin", funcName);
#ifdef HPUX
    int clntlen;
#else
    socklen_t clntlen;
#endif
    TSEND_RECV_INIT(POLLIN);
    clntlen = sizeof(*su_cliaddr);
again:
    n = recvfrom(fd, pkg_buffer, b_size, 0,
        &((*su_cliaddr).s), &clntlen);

    LOG(LL_VARS, "%s:recvfrom str:", funcName);
    LOG_HEX_ASC(LL_VARS, pkg_buffer, n);
    TSEND_RECV_ERR_CHECK("UdpRecv");
    DEBUG(LL_ALL, "%s:End", funcName);
    return n;
    TSEND_RECV_POLL("UdpRecv");
error:
    LOG(LL_ERROR, "%s:while recving packet error:(%s).",
        funcName, strerror(errno));
    return RET_ERROR;
}

// ---------------------------------------------------------------------------
// void CSocket::TcpSend(int aSockId)
//
// send buffer.
// ---------------------------------------------------------------------------
//
int CSocket::TcpSend(int aSockId, const char* aBuffer,
                     int aBufSize, int aTimeOut, bool aHasPolled) {
    DEBUG_FUNC_NAME("CSocket::TcpSend");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    if (aBuffer == NULL || aBufSize < 0) {
        LOG(LL_ERROR, "%s:buffer is null or size:(%d) less than zero.",
            funcName, aBufSize);
        return RET_ERROR;
    }
    DEBUG(LL_VARS, "%s:sockid:(%d), sendsize:(%d), timeout:(%d)",
        funcName, aSockId, aBufSize, aTimeOut);
    LOG_HEX_ASC(LL_VARS, aBuffer, aBufSize);
    int n = 0, sendSize = 0, status = FD_WRITABLE;
    aTimeOut *= TIMER_S_TO_MS;
    // using tick hz is instead of time(NULL) when write.
    unsigned int tmpTimeOutHz = aTimeOut / TIMER_TICKS_HZ;
    while (aBufSize > 0) {
        aTimeOut -= tmpTimeOutHz;
        if (aTimeOut <= 0) {
            LOG(LL_ERROR, "%s:Socket time out is occurred.", funcName);
            return RET_ERROR;
        }
        if (aHasPolled) {
            status = FD_WRITABLE;
        } else {
            status = PollSockId(aSockId, aTimeOut, FD_WRITABLE|FD_RWERROR);
            //status = SelectSockId(aSockId, aTimeOut, FD_WRITABLE|FD_RWERROR);
            if (status <= 0 || status & FD_RWERROR) {
                LOG(LL_ERROR, "%s:SelectSockId error status:(%d).", funcName, status);
                return RET_ERROR;
            }
        }
        if (status & FD_WRITABLE) {
            if ((n = write(aSockId, aBuffer+sendSize, aBufSize)) <= 0) {
                LOG(LL_ERROR, "%s::while sending packet error:(%s)",
                    funcName, strerror(errno));
                return RET_ERROR;
            }
            aBufSize -= n;
            sendSize += n;
        }
        aHasPolled = false;
    }
    DEBUG(LL_ALL, "%s:End", funcName);
    return sendSize;
}

// ---------------------------------------------------------------------------
// void CSocket::TcpSendV
//
// send buffer.
// ---------------------------------------------------------------------------
//
int CSocket::TcpSendV(int aSockId, const struct iovec* aIovec,
                      int aIovCnt, int aTimeOut, bool aHasPolled) {
    DEBUG_FUNC_NAME("CSocket::TcpSendV");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    if (aIovec == NULL || aIovCnt <= 0) {
        LOG(LL_ERROR, "%s:vector is null or size:(%d) less than zero.",
            funcName, aIovCnt);
        return RET_ERROR;
    }
    DEBUG(LL_VARS, "%s:sockid:(%d), count:(%d), timeout:(%d)",
        funcName, aSockId, aIovCnt, aTimeOut);
    int n = 0, status = FD_WRITABLE;
    aTimeOut *= TIMER_S_TO_MS;
    // using tick hz is instead of time(NULL) when write.
    unsigned int tmpTimeOutHz = aTimeOut / TIMER_TICKS_HZ;
    while (aTimeOut > 0) {
        aTimeOut -= tmpTimeOutHz;
        if (aTimeOut <= 0) {
            LOG(LL_ERROR, "%s:Socket time out is occurred.", funcName);
            return RET_ERROR;
        }
        if (aHasPolled) {
            status = FD_WRITABLE;
        } else {
            status = PollSockId(aSockId, aTimeOut, FD_WRITABLE|FD_RWERROR);
            //status = SelectSockId(aSockId, aTimeOut, FD_WRITABLE|FD_RWERROR);
            if (status <= 0 || status & FD_RWERROR) {
                LOG(LL_ERROR, "%s:SelectSockId error status:(%d).", funcName, status);
                return RET_ERROR;
            }
        }
        if (status & FD_WRITABLE) {
            if ((n = writev(aSockId, aIovec, aIovCnt)) <= 0) {
                LOG(LL_ERROR, "%s::while sending packet error:(%s)",
                    funcName, strerror(errno));
                return RET_ERROR;
            }
            break;
        }
    }
    DEBUG(LL_ALL, "%s:End", funcName);
    return n;
}

// ---------------------------------------------------------------------------
// void CSocket::TcpRecv(int aSockId)
//
// recv buffer.
// ---------------------------------------------------------------------------
//
int CSocket::TcpRecv(int aSockId, char *aBuffer,
                     int aBufSize, int aTimeOut,
                     bool aHasPolled, int aMaxSize) {
    DEBUG_FUNC_NAME("CSocket::TcpRecv");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    if (aBuffer == NULL || aBufSize < 0) {
        LOG(LL_ERROR, "%s:buffer is null or size:(%d) less than zero.",
            funcName, aBufSize);
        return RET_ERROR;
    }
    DEBUG(LL_VARS, "%s:sockid:(%d), recvsize:(%d), timeout:(%d)",
        funcName, aSockId, aBufSize, aTimeOut);
    int n = 0, recvSize = 0, status = FD_READABLE;
    bool isBody = false;
    aTimeOut *= TIMER_S_TO_MS;
    // using tick hz is instead of time(NULL) when read.
    unsigned int tmpTimeOutHz = aTimeOut / TIMER_TICKS_HZ;
    while (aBufSize > 0) {
        aTimeOut -= tmpTimeOutHz;
        if (aTimeOut <= 0) {
            LOG(LL_ERROR, "%s:Socket time out is occurred.", funcName);
            return RET_ERROR;
        }
        if ((status = aHasPolled ? FD_READABLE :
            PollSockId(aSockId, aTimeOut, FD_READABLE)) <= 0) {
            //SelectSockId(aSockId, aTimeOut, FD_READABLE)) <= 0) {
            LOG(LL_ERROR,
                "%s:SelectSockId error status:(%d).", funcName, status);
            return RET_ERROR;
        }
        if (status & FD_READABLE) {
            DEBUG(LL_INFO, "%s:select read set is ok.", funcName);
            if ((n = read(aSockId, aBuffer+recvSize, aBufSize)) <= 0) {
                LOG(LL_ERROR,
                    "%s:while read packet error:(%s)",
                    funcName, strerror(errno));
                return RET_ERROR;
            }
            aTimeOut += tmpTimeOutHz;
            aBufSize -= n;
            recvSize += n;
            if (!isBody && recvSize >= PKG_HEADER_LEN) {
                isBody = true;
                if (memcmp(aBuffer, PKG_TCP_HEAD_FLAG, PKG_HEADFLAG_LEN) != 0) {
                    LOG(LL_ERROR,
                        "%s:bad tcp cmd header:(%s).", funcName, aBuffer);
                    return RET_ERROR;
                }
                unsigned int len = 0;
                memcpy(&len, aBuffer+PKG_HEADFLAG_LEN, sizeof(unsigned int));
                len = ntohl(len);
                if (len > aMaxSize) {
                    LOG(LL_ERROR, "%s:to big tcp pkg len:(%lu) failed.",
                        funcName, len);
                    return RET_ERROR;
                }
                aBufSize = static_cast<int>(len);
                recvSize = 0;
                aHasPolled = true;
                continue;
            }
        }
        aHasPolled = false;
    }
    aBuffer[recvSize] = 0x00;
    LOG(LL_VARS, "%s:recv str:", funcName);
    LOG_HEX_ASC(LL_VARS, aBuffer, recvSize);
    DEBUG(LL_ALL, "%s:End", funcName);
    return recvSize;
}

// ---------------------------------------------------------------------------
// void CSocket::TcpRecvOnce(int aSockId)
//
// recv buffer once.
// ---------------------------------------------------------------------------
//
int CSocket::TcpRecvOnce(int aSockId, char *aBuffer, int aBufSize) {
    DEBUG_FUNC_NAME("CSocket::TcpRecvOnce");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    if (aBuffer == NULL || aBufSize < 0) {
        LOG(LL_ERROR, "%s:buffer is null or size:(%d) less than zero.",
            funcName, aBufSize);
        return RET_ERROR;
    }
    DEBUG(LL_VARS, "%s:sockid:(%d), recvsize:(%d)",
        funcName, aSockId, aBufSize);
    int ret = RET_ERROR;
    if((ret = read(aSockId, aBuffer, aBufSize)) <= 0) {
        LOG(LL_ERROR, "%s:sockid:(%d),read error:(%s).",
            funcName, aSockId, strerror(errno));
        return RET_ERROR;
    }
    aBuffer[ret] = 0x00;
    LOG(LL_VARS, "%s:read str:", funcName);
    LOG_HEX_ASC(LL_VARS, aBuffer, ret);
    DEBUG(LL_ALL, "%s:End", funcName);
    return ret;
}

// recv buffer as max size.
int CSocket::TcpRecvAll(int aSockId, char *aBuffer, int aBufSize, int aTimeOut) {
    DEBUG_FUNC_NAME("CSocket::TcpRecvAll");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    if (aBuffer == NULL || aBufSize < 0) {
        LOG(LL_ERROR, "%s:buffer is null or size:(%d) less than zero.",
            funcName, aBufSize);
        return RET_ERROR;
    }
    DEBUG(LL_VARS, "%s:sockid:(%d), recvsize:(%d), timeout:(%d)",
        funcName, aSockId, aBufSize, aTimeOut);
    int n = 0, recvSize = 0;
    bool isBody = false;
    aTimeOut *= TIMER_S_TO_MS;
    // using tick hz is instead of time(NULL) when read.
    unsigned int tmpTimeOutHz = aTimeOut / TIMER_TICKS_HZ;
    while (aBufSize > 0) {
        aTimeOut -= tmpTimeOutHz;
        if (aTimeOut <= 0) {
            LOG(LL_ERROR, "%s:Socket time out is occurred.", funcName);
            return RET_ERROR;
        }
        int status = PollSockId(aSockId, aTimeOut, FD_READABLE|FD_RWERROR);
        //int status = SelectSockId(aSockId, aTimeOut, FD_READABLE|FD_RWERROR);
        if (status <= 0 || status & FD_RWERROR) {
            if (recvSize > 0) {
                break;
            }
            LOG(LL_ERROR, "%s:SelectSockId error status:(%d).", funcName, status);
            return RET_ERROR;
        }
        if (status & FD_READABLE) {
            DEBUG(LL_INFO, "%s:select read set is ok.", funcName);
            n = read(aSockId, aBuffer+recvSize, aBufSize);
            if (n <= 0) {
                LOG(LL_ERROR, "%s:while read packet error:(%s)",
                    funcName, strerror(errno));
                break;
            }
            aTimeOut += tmpTimeOutHz;
            aBufSize -= n;
            recvSize += n;
            aBuffer[recvSize] = 0x00;
        }
    }
    aBuffer[recvSize] = 0x00;
    LOG(LL_VARS, "%s:recv buffer:", funcName);
    LOG_HEX_ASC(LL_VARS, aBuffer, recvSize);
    DEBUG(LL_ALL, "%s:End", funcName);
    return recvSize;
}

// ---------------------------------------------------------------------------
// void CSocket::TcpRecvHttpHeader(int aSockId)
//
// recv buffer once.
// ---------------------------------------------------------------------------
//
int CSocket::TcpRecvHttpHeader(int aSockId, char *aBuffer,
                               int aBufSize, int aTimeOut) {
    DEBUG_FUNC_NAME("CSocket::TcpRecvHttpHeader");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    if (aBuffer == NULL || aBufSize < 0) {
        LOG(LL_ERROR, "%s:buffer is null or size:(%d) less than zero.",
            funcName, aBufSize);
        return RET_ERROR;
    }
    DEBUG(LL_VARS, "%s:sockid:(%d), recvsize:(%d), timeout:(%d)",
        funcName, aSockId, aBufSize, aTimeOut);
    int n = 0, recvSize = 0;
    bool isBody = false;
    aTimeOut *= TIMER_S_TO_MS;
    // using tick hz is instead of time(NULL) when read.
    unsigned int tmpTimeOutHz = aTimeOut / TIMER_TICKS_HZ;
    while (aBufSize > 0) {
        aTimeOut -= tmpTimeOutHz;
        if (aTimeOut <= 0) {
            LOG(LL_ERROR, "%s:Socket time out is occurred.", funcName);
            return RET_ERROR;
        }
        int status = PollSockId(aSockId, aTimeOut, FD_READABLE|FD_RWERROR);
        //int status = SelectSockId(aSockId, aTimeOut, FD_READABLE|FD_RWERROR);
        if (status <= 0 || status & FD_RWERROR) {
            LOG(LL_ERROR, "%s:SelectSockId error status:(%d).", funcName, status);
            return RET_ERROR;
        }
        if (status & FD_READABLE) {
            DEBUG(LL_INFO, "%s:select read set is ok.", funcName);
            if ((n = read(aSockId, aBuffer+recvSize, aBufSize)) <= 0) {
                LOG(LL_ERROR, "%s:while read packet error:(%s)",
                    funcName, strerror(errno));
                return RET_ERROR;
            }
            aTimeOut += tmpTimeOutHz;
            aBufSize -= n;
            recvSize += n;
            aBuffer[recvSize] = 0x00;
            if (strstr(aBuffer, "\r\n\r\n") != NULL) {
                break;
            }
        }
    }
    aBuffer[recvSize] = 0x00;
    LOG(LL_VARS, "%s:recv buffer:", funcName);
    LOG_HEX_ASC(LL_VARS, aBuffer, recvSize);
    DEBUG(LL_ALL, "%s:End", funcName);
    return recvSize;
}

int CSocket::RecvHttpPkg(int aSockId, char *aBuffer, int aBufSize, int aTimeOut) {
    DEBUG_FUNC_NAME("CSocket::RecvHttpPkg");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    if (aBuffer == NULL || aBufSize < 0) {
        LOG(LL_ERROR, "%s:buffer is null or size:(%d) less than zero.",
            funcName, aBufSize);
        return RET_ERROR;
    }
    int recvLen = TcpRecvHttpHeader(aSockId, aBuffer, MAX_SIZE_1K, aTimeOut);
    if (recvLen == RET_ERROR) {
        LOG(LL_ERROR, "%s:sockid:(%d),recv data error:(%s).",
            funcName, aSockId, strerror(errno));
        return RET_ERROR;
    }
    const char *ptr = wyf::CHttpUtils::GetBodyStr(aBuffer);
    if (ptr == NULL) {
        LOG(LL_ERROR, "%s:bad HTTP package.", funcName);
        return RET_ERROR;
    }
    int hasRecvLen = recvLen - (ptr - aBuffer);
    int needRecvLen = wyf::CHttpUtils::GetHeaderValue(aBuffer, "Content-Length");
    needRecvLen -= hasRecvLen;
    DEBUG(LL_VARS, "%s:has recv len:(%d), need recv len:(%d)",
        funcName, hasRecvLen, needRecvLen);
    if (needRecvLen + recvLen > aBufSize) {
        LOG(LL_ERROR, "%s:recv too large http body (%d).",
            funcName, needRecvLen);
        return RET_ERROR;
    }
    if (needRecvLen > 0) {
        // continue ro receiving http body.
        if (recvLen == TcpRecvByLen(aSockId, aBuffer+recvLen,
            needRecvLen, 0, aTimeOut)) {
            LOG(LL_ERROR, "%s:recv http body error:(%s).",
                funcName, strerror(errno));
            return RET_ERROR;
        }
    }
    DEBUG(LL_ALL, "%s:End", funcName);
    return recvLen;
}

// ---------------------------------------------------------------------------
// void CSocket::TcpRecvLine(int aSockId)
//
// recv buffer by line,end of '\n'. aBufSize is max length of aBuffer.
// ---------------------------------------------------------------------------
//
int CSocket::TcpRecvLine(int aSockId, char *aBuffer,
                         int aBufSize, int aTimeOut) {
    DEBUG_FUNC_NAME("CSocket::TcpRecvLine");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    if (aBuffer == NULL || aBufSize < 0) {
        LOG(LL_ERROR, "%s:buffer is null or size:(%d) less than zero.",
            funcName, aBufSize);
        return RET_ERROR;
    }
    DEBUG(LL_VARS, "%s:sockid:(%d), recvsize:(%d), timeout:(%d)",
        funcName, aSockId, aBufSize, aTimeOut);
    int n = 0, recvSize = 0;
    bool isBody = false;
    aTimeOut *= TIMER_S_TO_MS;
    // using tick hz is instead of time(NULL) when read.
    unsigned int tmpTimeOutHz = aTimeOut / TIMER_TICKS_HZ;
    while (aBufSize > 0) {
        aTimeOut -= tmpTimeOutHz;
        if (aTimeOut <= 0) {
            LOG(LL_ERROR, "%s:Socket time out is occurred.", funcName);
            return RET_ERROR;
        }
        int status = PollSockId(aSockId, aTimeOut, FD_READABLE|FD_RWERROR);
        //int status = SelectSockId(aSockId, aTimeOut, FD_READABLE|FD_RWERROR);
        if (status <= 0 || status & FD_RWERROR) {
            LOG(LL_ERROR, "%s:SelectSockId error status:(%d).", funcName, status);
            return RET_ERROR;
        }
        if (status & FD_READABLE) {
            DEBUG(LL_INFO, "%s:select read set is ok.", funcName);
            aTimeOut += tmpTimeOutHz;
            do {
                if ((n = read(aSockId, aBuffer+recvSize, 1)) <= 0) {
                    LOG(LL_ERROR, "%s:while read packet error:(%s)",
                        funcName, strerror(errno));
                    return RET_ERROR;
                }
                if (*(aBuffer+recvSize) == '\n') {
                    *(aBuffer+recvSize) = 0x00;
                    if (recvSize > 2 && *(aBuffer+recvSize-1) == '\r') {
                        *(aBuffer+recvSize-1) = 0x00;
                        recvSize--;
                    }
                    break;
                }
                aBufSize -= n;
                recvSize += n;
            } while (aBufSize > 0);
            break;
        }
    }
    aBuffer[recvSize] = 0x00;
    LOG(LL_VARS, "%s:recv str:", funcName);
    LOG_HEX_ASC(LL_VARS, aBuffer, recvSize);
    DEBUG(LL_ALL, "%s:End", funcName);
    return recvSize;
}

// ---------------------------------------------------------------------------
// void CSocket::TcpRecvByLen(int aSockId)
//
// recv buffer by XXXXyyyyy. XXXX is hex len and yyyy is buffer.
// ---------------------------------------------------------------------------
//
int CSocket::TcpRecvByLen(int aSockId, char *aBuffer, int aBufSize, int aFlag, int aTimeOut) {
    DEBUG_FUNC_NAME("CSocket::TcpRecvByLen");
    DEBUG(LL_ALL, "%s:Begin", funcName);
    if (aBuffer == NULL || aBufSize < 0) {
        LOG(LL_ERROR, "%s:buffer is null or size:(%d) less than zero.",
            funcName, aBufSize);
        return RET_ERROR;
    }
    DEBUG(LL_VARS, "%s:sockid:(%d), recvsize:(%d), timeout:(%d)",
        funcName, aSockId, aBufSize, aTimeOut);
    int n = 0, recvSize = 0;
    bool isBody = false;
    aTimeOut *= TIMER_S_TO_MS;
    // using tick hz is instead of time(NULL) when read.
    unsigned int tmpTimeOutHz = aTimeOut / TIMER_TICKS_HZ;
    while (aBufSize > 0) {
        aTimeOut -= tmpTimeOutHz;
        if (aTimeOut <= 0) {
            LOG(LL_ERROR, "%s:Socket time out is occurred.", funcName);
            return RET_ERROR;
        }
        int status = PollSockId(aSockId, aTimeOut, FD_READABLE|FD_RWERROR);
        //int status = SelectSockId(aSockId, aTimeOut, FD_READABLE|FD_RWERROR);
        if (status <= 0 || status & FD_RWERROR) {
            LOG(LL_ERROR, "%s:SelectSockId error status:(%d).", funcName, status);
            return RET_ERROR;
        }
        if (status & FD_READABLE) {
            DEBUG(LL_INFO, "%s:select read set is ok.", funcName);
            if ((n = read(aSockId, aBuffer+recvSize, aBufSize)) <= 0) {
                LOG(LL_ERROR, "%s:while read packet error:(%s)",
                    funcName, strerror(errno));
                return RET_ERROR;
            }
            aTimeOut += tmpTimeOutHz;
            aBufSize -= n;
            recvSize += n;
            if (aFlag != 0 && !isBody && recvSize >= PKG_CMDLEN_LEN) {
                isBody = true;
                aBuffer[recvSize] = 0x00;
                aBufSize = (aFlag == 16) ? LenXHex2Int(aBuffer) : atoi(aBuffer);
                if (aBufSize <= 0) {
                    LOG(LL_ERROR, "%s:fetch tcp pkg len:(%s) failed.",
                        funcName, aBuffer);
                    return RET_ERROR;
                }
                recvSize = 0;
            }
        }
    }
    aBuffer[recvSize] = 0x00;
    LOG(LL_VARS, "%s:recv str:", funcName);
    LOG_HEX_ASC(LL_VARS, aBuffer, recvSize);
    DEBUG(LL_ALL, "%s:End", funcName);
    return recvSize;
}

// Create Package header.
// 'YF'dddd  'YF' is identity and dddd is length encoding by Integer ntohl.
int CSocket::CreatePkgHeader(int aLen, char *aOutBuf, const string& aAppStr) {
    unsigned int len = static_cast<unsigned int>(aLen+aAppStr.length());
    len = htonl(len);
    memcpy(aOutBuf, PKG_TCP_HEAD_FLAG, PKG_HEADFLAG_LEN);
    memcpy(aOutBuf+PKG_HEADFLAG_LEN, &len, sizeof(unsigned int));
    if (!aAppStr.empty()) {
        memcpy(aOutBuf+PKG_HEADER_LEN, aAppStr.data(), aAppStr.length());
    }
    
    return PKG_HEADFLAG_LEN+sizeof(unsigned int)+aAppStr.length();
}
// ---------------------------------------------------------------------------
// unsigned int CSocket::LenXHex2Int(char* _s, int len = 4)
//
// hex string to int.
// ---------------------------------------------------------------------------
//
unsigned int CSocket::LenXHex2Int(const char* aStr, int len) {
    unsigned int i, res = 0;

    for(i = 0; i < len; i++) {
        res *= 16;
        if ((aStr[i] >= '0') && (aStr[i] <= '9')) {
            res += aStr[i] - '0';
        } else if ((aStr[i] >= 'a') && (aStr[i] <= 'f')) {
            res += aStr[i] - 'a' + 10;
        } else if ((aStr[i] >= 'A') && (aStr[i] <= 'F')) {
            res += aStr[i] - 'A' + 10;
        } else {
            return 0;
        }
    }
    return res;
}

// ---------------------------------------------------------------------------
// int CSocket::Hex2Int(char hex_digit)
//
// hex to int
// ---------------------------------------------------------------------------
//
int CSocket::Hex2Int(const char hex_digit) {
    if (hex_digit >= '0' && hex_digit <= '9') return hex_digit-'0';
    if (hex_digit >= 'a' && hex_digit <= 'f') return hex_digit-'a'+10;
    if (hex_digit >= 'A' && hex_digit <= 'F') return hex_digit-'A'+10;
    return -1;
}


// ---------------------------------------------------------------------------
// int CSocket::GetLocalIP(char* aOutIP)
//
// getting local ip address and ignore local loopback ip.
// ---------------------------------------------------------------------------
//
int CSocket::GetLocalIP(char* aOutIP) {
    int i=0;
    int sockfd;
    struct ifconf ifconf;
    char buf[512];
    struct ifreq *ifreq;
    char* ip;
    // ifconf initialization.
    ifconf.ifc_len = 512;
    ifconf.ifc_buf = buf;

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < RET_OK) {
        return RET_ERROR;
    }
    // get all interface information.
    ioctl(sockfd, SIOCGIFCONF, &ifconf);
    close(sockfd);
    // get ip address one by one.
    ifreq = (struct ifreq*)buf;
    for(i=(ifconf.ifc_len/sizeof(struct ifreq)); i > 0; i--) {
        ip = inet_ntoa(((struct sockaddr_in*)&(ifreq->ifr_addr))->sin_addr);
        // local loopback ip is ignoring, finding next one.
        if(strcmp(ip,"127.0.0.1")==0) {
            ifreq++;
            continue;
        }
        strcpy(aOutIP, ip);
        return RET_OK;
    }

    return RET_ERROR;
}
// end of local file.
