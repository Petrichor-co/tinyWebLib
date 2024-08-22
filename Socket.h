#pragma once

#include "noncopyable.h"

class InetAddress;

class Socket : noncopyable
{
public:
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
        {}
    ~Socket();

    int fd() const {return sockfd_;}
    void bindAddress(const InetAddress &localaddr);
    void listen();
    int accept(InetAddress *peeraddr);

    void shutdownWrite();

    void setTcpNoDelay(bool on);//直接发送，数据不进行TCP缓存 
    void setReuseAddr(bool on); //地址重用
    void setReusePort(bool on); //端口重用
    void setKeepAlive(bool on);

private:
    const int sockfd_;
};