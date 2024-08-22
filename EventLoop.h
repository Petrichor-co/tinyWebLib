#pragma once

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

class Channel;
class Poller;

// 时间循环类，主要包含两个大模块：Channel   Poller（epoll的抽象）
class EventLoop : noncopyable
{
public:
    // C++11 using 代替typrdef重命名
    // std::function是一种通用、多态的函数封装
    // std::function的实例可以对任何可以调用的目标实体进行存储、复制、和调用操作，这些目标实体包括普通函数、Lambda表达式、函数指针、以及其它函数对象等。
    // std::function对象是对C++中现有的可调用实体的一种类型安全的包裹（我们知道像函数指针这类可调用实体，是类型不安全的）
    using Functor = std::function<void()>; // 放一些回调函数

    EventLoop();
    ~EventLoop();

    //开启事件循环
    void loop();
    //退出事件循环
    void quit();
    //返回当前时间
    Timestamp pollReturnTime() const {return pollReturnTime_;}

    // ⭐着重强调：
    //在当前loop中执行cb，cb是回调操作
    void runInLoop(Functor cb);
    //把cb放入队列中，唤醒loop所在的线程，执行cb
    void queueInLoop(Functor cb);

     //用来唤醒loop所在的线程的 (mainReactor用来唤醒subReactor)
     void wakeup();

     //EventLoop的方法,其中调用的是Poller的方法
     void updateChannel(Channel *channel);
     void removeChannel(Channel *channel);
     bool hasChannel(Channel *channel);

    //判断EventLoop对象是否在自己的线程里面
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
    void handleRead(); //唤醒wake up
    void doPendingFunctors(); // 执行回调

    using ChannelList = std::vector<Channel*>; // vector数组
    std::atomic_bool looping_; // 原子操作，通过CAS实现
    std::atomic_bool quit_; // 标识退出loop循环

    const pid_t threadId_; // 记录当前loop所在线程的id
    // 作为EventLoop的成员变量记录了创建的EventLoop对象所在的线程的id，跟当前线程id一比较，就能够判断EventLoop在不在它自己的线程里面

    Timestamp pollReturnTime_; // poller返回的发生事件的channels的时间点
    std::unique_ptr<Poller> poller_; // EventLoop所管理的poller

    // mainReactor如何将发生事件的channel给到subReactor
    // Linux内核的eventfd创建的 
    int wakeupFd_; // 主要作用：当mainLoop获取一个新用户的channel，通过轮询算法选择一个subloop（还在睡觉），然后通过该成员唤醒subloop处理channel
    std::unique_ptr<Channel> wakeupChannel_; //包括wakeupFd和感兴趣的事件 

    ChannelList activeChannels_; //eventloop管理的所有channel
    Channel *currentActiveChannel_;

    std::atomic_bool callingPendingFunctors_; // 标识当前loop是否有需要执行回调的操作
    std::vector<Functor> pendingFunctors_; //存储loop需要执行的所有的回调操作 Functor 格式
    std::mutex mutex_; // 互斥锁，用来保护上面vector容器的线程安全操作

};