#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"

#include <memory>

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop)
    , name_(nameArg)
    , started_(false)
    , numThreads_(0)
    , next_(0)
{

}

EventLoopThreadPool::~EventLoopThreadPool()
{
}


void EventLoopThreadPool::start(const ThreadInitCallback &cb)//开启整个事件循环线程
{
    started_ = true;

    for (int i = 0; i < numThreads_; i++) 
    {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));//unique_ptr，不想手动delete
        loops_.push_back(t->startLoop());//底层创建线程，绑定一个新的EventLoop，并返回该loop的地址
    }

    // 整个服务端只有一个线程运行着 baseloop，就是用户创建的mainloop
    if (numThreads_ = 0 && cb) 
    {
        cb(baseLoop_);
    }
        
}

//如果工作在多线程中，baseLoop_默认以 轮询 的方式分配channel给subloop
EventLoop* EventLoopThreadPool::getNextLoop()
{
    EventLoop* loop = baseLoop_; //用户创建的mainloop

    if (!loops_.empty()) { // 通过轮询获取处理下一个事件的loop
        loop = loops_[next_];
        ++next_;
        if (next_ >= loops_.size()){
            next_ = 0;
        }
    }

    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops() //返回池里的所有loop 
{
    if (loops_.empty())
    {
        return std::vector<EventLoop*>(1, baseLoop_);
    }
    else {
        return loops_;
    }
}