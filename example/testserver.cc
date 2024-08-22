#include <mymuduo/TcpServer.h>
#include <mymuduo/Logger.h>

#include <string>
#include <functional>

class EchoServer
{
public:
    EchoServer(EventLoop* loop,
                const InetAddress &addr,
                const std::string name)
                : server_(loop, addr, name)
                , loop_(loop)
    {
        // 注册回调函数
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1)
        );

        server_.setMessageCallback( // onMessage 需要三个参数 所以有三个参数占位符
            std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
        );

        // 设置合适的loop线程数量  loopthread的数量
        server_.setThreadNum(3);
    }

    void start()
    {
        server_.start();
    }

private:
    // 连接建立或者断开的回调
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            LOG_INFO("conn UP : %s ", conn->peerAddress().toIpPort().c_str()); 
        }
        else
        {
            LOG_INFO("conn DOWN : %s ", conn->peerAddress().toIpPort().c_str()); 
        }
    }

    // 可读写事件回调
    void onMessage(const TcpConnectionPtr &conn,
                    Buffer* buf,
                    Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        conn->shutdown(); // 写端
    }
     
    EventLoop* loop_;
    TcpServer server_;

};

int main()
{
    EventLoop loop;
    InetAddress addr(8000);
    /*
    TcpServer构造函数要做的事情：
        1. TcpServer构造函数，传入当前mainloop，当前InetAddress，当前服务器name，创建一个Acceptor；

        2. Acceptor构造函数通过createNonblocking函数创建一个用于监听的Listenfd，并将其封装成一个acceptChannel_；

        3. TcpServer::start()调用Acceptor::listen()，通过acceptChannel_.enableReading将该channel注册到Poller中，交给Poller监听；

        4. Poller监听到acceptorChannel_上有事件发生，通过acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this))中的handleRead返回一个connfd，并且去执行newConnectionCallback_回调（由TcpServer::newConnection设置的）
    */
    EchoServer server(&loop, addr, "EchoSerer-01"); // 1、创建Acceptor  2、创建non-blocking listenfd 创建create 再bind 
    server.start();// 3、listen  loopthread  listenfd => acceptChannel => mainLoop
    loop.loop(); // 启动mainLoop底层的Poller，开始监听事件了

    return 0;
}