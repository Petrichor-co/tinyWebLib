#pragma once // 当你在多个文件中包含同一个头文件时，#pragma once 确保该头文件的内容只会被 编译 一次，避免重复定义的错误。

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

#include <memory>
#include <string>
#include <atomic>


/*
    TcpServer  =>  Acceptor  =>  有一个新用户连接，通过accept函数拿到connfd
    =》 TcpConnection 设置回调 =》 Channel =》 Poller =》 Channel的回调操作
*/
// 类的前向声明，用于在当前文件中使用这些类，但不需要包含他们的完整定义，用于减少不必要的编译依赖，如果想要具体实现细节（比如调用成员函数），那么需要include头文件，将另一个文件的内容插入到当前文件中
class Channel;
class EventLoop;
class Socket;

class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop,
                const std::string &nameArg,
                int sockfd,
                const InetAddress& localAddr_,
                const InetAddress& peerAddr_);
    ~TcpConnection();

    EventLoop* getLoop() const {return loop_;}
    const std::string& name() const {return name_;}
    const InetAddress& localAddress() const {return localAddr_;}
    const InetAddress& peerAddress() const {return peerAddr_;}

    bool connected() const {return state_ == kConnected;}

    //发送数据
    void send(const std::string &buf); // 这个要在public中，因为要被用户调用
    // void send(const void *message, int len); // 这个没重写
    //关闭连接
    void shutdown();
    
    void setConnectionCallback(const ConnectionCallback& cb)
    { connectionCallback_ = cb; }
 
    void setMessageCallback(const MessageCallback& cb)
    { messageCallback_ = cb; }
 
    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    { writeCompleteCallback_ = cb; }
 
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }
 
    void setCloseCallback(const CloseCallback& cb)
    { closeCallback_ = cb; }
 
    //连接建立
    void connectEstablished();
    //连接销毁
    void connectDestroyed();

    // void setState(StateE state) {state_ = state;}
private:
    //枚举连接状态 ： 已经断开 正在连接 连接成功 正在断开
    enum StateE {kDisconnected, kConnecting, kConnected, kDisconnecting};
    // 设置状态，handleClose()用
    void setState(StateE state) { state_ = state; }
 
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();

    EventLoop *loop_; // 这里绝对不是baseloop，因为Tcpconnection都是在subLoop里管理的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;
    
    // 这里和Acceptor类似   Acceptor在mainloop里，TcpConnection在subloop里
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_; //有新连接时的回调
    MessageCallback messageCallback_; //已连接用户有读写消息时的回调 reactor调用
    WriteCompleteCallback writeCompleteCallback_;//消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_;//水位 控制发送数据的速度
    CloseCallback closeCallback_;

    size_t highWaterMark_;//水位标志
    
    Buffer inputBuffer_;//接收数据的缓冲区
    Buffer outputBuffer_;//发送数据的缓冲区
};