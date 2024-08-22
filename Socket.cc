#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <strings.h>

Socket::~Socket()
{
    close(sockfd_);//调用系统的close
}

// 底层就是bind函数
void Socket::bindAddress(const InetAddress &localaddr)
{   
    /* Give the socket FD the local address ADDR (which is LEN bytes long). 
    extern int bind (int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len)
     __THROW;  */
    if (0 != ::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in)))
    {
        LOG_FATAL("bind sockfd: %d failed \n", sockfd_);
    }
}

void Socket::listen()
{   
    /* Prepare to accept connections on socket FD.
    N connection requests will be queued before further requests are refused.
    Returns 0 on success, -1 for errors.  
    extern int listen (int __fd, int __n) __THROW; */
    if (0 != ::listen(sockfd_, 1024))
    {
        LOG_FATAL("listen sockfd: %d failed \n", sockfd_);
    }
}


int Socket::accept(InetAddress *peeraddr)
{
     /**
     * 1. accept函数的参数不合法：len必须初始化
     * 2. 对返回的connfd没有设置非阻塞
     * 本模型是一个基于多线程得Reactor模型 one loop per thread
     * 每个loop里面都是一个 poller + non-blocking IO
     */ 
    sockaddr_in addr;
    socklen_t len = sizeof addr;
    //void bzero(void *s, int n);  bzero()将参数s 所指的内存区域前n 个字节全部设为零。
    bzero(&addr, sizeof addr);

    /* accept 是阻塞调用，会一直等待直到有新的连接到达；而accept4 可以设置为非阻塞模式，即使没有新连接到达，他也会立即返回
       accept4 额外提供了创建新设置套接字的选项，SOCK_NONBLOCK 非阻塞模式 SOCK_CLOEXEC 执行exec系统调用时自动关闭套接字*/
    // int connfd = ::accept(sockfd_, (sockaddr*)&addr, &len); // addr 指向sockaddr结构的指针，接受 连接方的地址信息
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC); // 设置为非阻塞模式

    if (connfd > 0) 
    {
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

void Socket::shutdownWrite() // 只关闭写端
{
    if (::shutdown(sockfd_, SHUT_WR) < 0) 
    {
        LOG_ERROR("shutdownwrite error!");
    }
}

void Socket::setTcpNoDelay(bool on) //直接发送，数据不进行TCP缓存 
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);

}
void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}
void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}
void Socket::setKeepAlive(bool on)//启动Tcp Socket的保活机制
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}
