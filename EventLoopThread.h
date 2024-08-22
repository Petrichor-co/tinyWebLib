#pragma once

#include "noncopyable.h"
#include "Thread.h"

#include <functional>
#include <condition_variable>
#include <mutex>
#include <string>

class EventLoop;

// ⭐ 一个Loop运行在一个Thread上，通过绑定器将其绑定
class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallback = std::function<void (EventLoop*)>;
    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
    const std::string &name = std::string() );
    ~EventLoopThread();

    EventLoop* startLoop();
private:
    void threadFunc();

    EventLoop *loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};