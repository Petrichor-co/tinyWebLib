#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>

class EventLoop;
// class Timestamp;

/*
理清楚eventloop channel  poller之间的关系，他们在reactor模型上对应 Demultiplex多路事件分发器
Channel 理解为通道，封装了sockfd和其感兴趣的event，如EPOLLIN  EPOLLOUT事件
还绑定了poller返回的具体事件
*/
class Channel : noncopyable  // 无法执行拷贝和赋值操作
{
public:
    // https://blog.csdn.net/qq_42595835/article/details/131117121 std::function 可调用对象
    // 使用std::function通用多态函数包装器，将类型EventCallback和ReadEventCallback定义成function对象，将函数存储和操作为对象的方式
    // 不用typedef，而用using定义类型
    using EventCallback =  std::function<void()>; // 事件回调
    using ReadEventCallback = std::function<void(Timestamp)>; // 只读事件回调  // 用function实现对bind绑定的函数对象的类型保留

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // fd得到poller通知以后，处理事件的
    void handleEvent(Timestamp receiveTime);

    // 这里四个set是为了 设置(相当于赋值操作) 回调函数对象  提供 <对外的接口>  （EventLoop、TcpConnection中会进行设置）
    // cb是一个函数对象，存在值和地址，直接使用会导致资源复制，这里直使用std::move移动语义避免不必要的复制和提高性能
    // 将cb标记为右值引用，在调用对象的移动构造函数或移动赋值运算符完成实际移动操作。
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) {writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) {closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) {errorCallback_ = std::move(cb); }

    // ⭐ 防止当TcpConnection被手动remove掉，channel还在执行回调操作
    // Channel类中的`tie`成员及相关函数主要用于增强对象生命周期管理，确保`Channel`在处理回调时，相关的对象（例如`TcpConnection`）仍然存活（回调对象是否存活在网络编程中很重要）
    void tie(const std::shared_ptr<void>&);

    // fd处理函数
    int fd() const {return fd_;}
    int events() const {return events_;} // fd所感兴趣的事件
    int set_revents(int revt) {return revents_ = revt;} // Poller（相当于epoll）监听到了发生的事件，会通过set_revents函数来设置Channel中的revents_（set_revents是给Poller提供的接口）

    // 设置fd相应的事件 状态，要让fd对这个事件感兴趣
    // update就是调用epoll_ctrl, 通知poller把fd感兴趣的事件添加到fd上
    // 先填上事件，然后再 update
    void enableReading() { events_ |= kReadEvent; update();} // 执行 events_或等于fd感兴趣的读事件，然后 update  读
    void disableReading() {events_ &= ~kReadEvent; update();} // 执行 events_与等于fd感兴趣的不读事件 然后 update  不读
    void enableWriting() {events_ |= kWriteEvent; update();}
    void disableWriting() {events_ &= ~kWriteEvent; update();}
    void disableAll() {events_ = kNoneEvent; update(); } // 都不感兴趣

    // 返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    // index用来表示channel在poller中的位置 or 状态（添加没添加）？一个poller中有很多个channel
    int index() {return index_; }
    void set_index(int idx) {index_ = idx; } // 设置 index_

    // one loop per thread 
    EventLoop* ownerLoop() {return loop_; } // 当前channel属于哪个eventloop
    void remove(); // 删除channel


private: //内部接口

    void update(); //更新，内部对象调用
    void handleEventWithGuard(Timestamp reveiveTime); //受保护的处理事件

    // 表示当前fd和其 状态，是没有对任何事件感兴趣，还是对读或者写感兴趣
    // 给events_用的
    static const int kNoneEvent; //都不感兴趣
    static const int kReadEvent; //读事件
    static const int kWriteEvent; //写事件

    EventLoop *loop_; // 事件循环
    const int fd_; // fd，poller监听的对象   epoll_ctl
    int events_; // 注册fd感兴趣的事件
    int revents_; // poller返回 的具体发生的事件
    int index_;

    std::weak_ptr<void> tie_; //弱指针 绑定用处  关于 弱指针和强指针 https://blog.csdn.net/qq_38410730/article/details/105903979
    bool tied_; // 判断绑没绑定

    // 因为Channel通道里面能够获知fd最终发生的具体的事件revents，所以它负责调用具体事件的回调操作
    // 根据不同的事件，进行相应的操作
    // 这些回调是用户设定的，通过接口传给channel来负责调用，channel才知道fd上是什么事件
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;

};