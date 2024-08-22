#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include <strings.h>
#include <functional>

static EventLoop* CheckLoopNotNull (EventLoop* loop)  // 防止不同文件函数名字冲突
{
    if (loop == nullptr) {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

// 构造
TcpServer::TcpServer(EventLoop *loop,
            const InetAddress &listenAddr,
            const std::string &nameArg, //服务器名称
            Option option)
            : loop_(CheckLoopNotNull(loop)) // 主loop不能为空啊
            , ipPort_(listenAddr.toIpPort())
            , name_(nameArg)
            , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))
            , threadPool_(new EventLoopThreadPool(loop, name_))
            , connectionCallback_()
            , messageCallback_()
            , nextConnId_(1)
            , started_(0) // 不是静态变量（自动初始化为0），所以得自定义初始化
{   
    //当有新用户连接时，会执行TcpServer::newConnection回调，代码中是对应的是Acceptor::handleRead()
    //两个参数 fd 地址
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
        std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for (auto &item : connections_)
    {   
        //这个局部的shared_ptr智能指针对象，出右括号，可以自动释放new出来的TcpConnection对象资源了
        TcpConnectionPtr conn(item.second);
        item.second.reset();

        //销毁连接
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn)
        );
    }
}

//设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

//开启服务器监听  实际上就是开启mainloop的acceptor的listen 
void TcpServer::start()
{
    if (started_++ == 0)  // 防止一个Tcpserver对象被start多次，第一次为0，后面就++了进不来循环了
    {
        threadPool_->start(threadInitCallback_); // 启动底层的loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// 有一个新的客户端的连接，acceptor会执行这一个回调
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)//有新连接来了
{   
    //轮询算法，选择一个subLoop，来管理channel
    EventLoop *ioLoop = threadPool_->getNextLoop(); 

    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
        name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    //通过sockfd获取其绑定的本机的ip地址和端口信息
    sockaddr_in local; // IPv4地址
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }
    InetAddress localAddr(local);
 
    //根据连接成功的sockfd，创建TcpConnection连接对象 
    //TcpConnection用智能指针管理
    TcpConnectionPtr conn(new TcpConnection(
                            ioLoop, //所在的事件循环
                            connName, //名字
                            sockfd,   // Socket Channel
                            localAddr, // 本地IP和端口号
                            peerAddr    // 客户端IP和端口号
                            ));
    connections_[name_] = conn;
    //下面的回调都是用户设置给TcpServer=>TcpConnection=>Channel=>Poller=>notify channel调用回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    //设置了如何关闭连接的回调   conn->shutDown()
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)
    );

    /*只要有一个新的客户端连接，最终就会生成响应的TCP connection对象，并直接
    调用TcpConnection::connectEstablished->做的事情就是把这个连接的state从刚开始的connecting变成connected连接建立。
    然后tie绑定自己一下，然后channel_->enableReading()向poller中注册这个Channel的epollin事件，开始监听
    EventLoop *ioLoop = threadPool_->getNextLoop()上面这里选择了一个loop
    监听事件后 connectionCallback_(shared_from_this())可以开始执行回调了，用户调用connected()判断是否连接成功
    */
    // 直接调用TcpConnection::connectEstablished
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}


void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this,conn)
    );
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n", 
        name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop(); //拿这条连接对应的loop
    ioLoop->queueInLoop( //去执行该连接的关闭
        std::bind(&TcpConnection::connectDestroyed, conn)
    );
}