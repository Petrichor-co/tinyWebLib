#pragma once

#include <arpa/inet.h>
#include <netinet/in.h> //sockaddr_in
#include <string>

// 封装socket地址类型
class InetAddress
{
public:
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1"); // 默认构造，Acceptor.cc中用
    explicit InetAddress(const sockaddr_in &addr)
        : addr_(addr)
        {}

    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    const sockaddr_in* getSockAddr() const {return &addr_;}
    void setSockAddr(const sockaddr_in &addr) { addr_ = addr;} 

private:  
    sockaddr_in addr_; // sockaddr_in 描述互联网套接字地址的结构

};