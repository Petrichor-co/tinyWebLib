#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
    const std::string &name) 
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name) //绑定回调函数 Thread类构造函数有两个参数，一个函数模板，一个线程名字
    , mutex_()
    , cond_()
    , callback_(cb)
{

}

EventLoopThread::~EventLoopThread()//析构函数
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop()//开启循环
{
    thread_.start();//启动底层的新线程
	//启动后执行的是EventLoopThread::threadFunc
    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while ( loop_ == nullptr )
        {
            cond_.wait(lock);//挂起，等待  threadFunc中的loop创建好并通知
        }
        loop = loop_;
    }
    return loop;
}


// 下面这个方法是在单独的新线程里运行的
void EventLoopThread::threadFunc()
{
    EventLoop loop; // 创建一个独立的EventLoop，和上面的线程是一一对应的，真正的 one loop per thread ！！

    if (callback_)//如果有回调
    {
        callback_(&loop);//绑定loop做一些事情
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;//就是运行在这个线程的loop对象
        cond_.notify_one();//唤醒1个线程
    }


    // 一般来说会一直在loop函数中循环  
    loop.loop();//相当于EventLoop loop  => Poller.poll

    //执行到这个下边就说明服务器程序要关闭掉了
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}