#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <vector>
#include <unordered_map>

class Channel;
class EventLoop;

// muduo库中 多路事件分发器 的核心IO复用模块 {Demultiplex 开启事件循环 epoll_wait}
class Poller
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop *loop); //构造函数
    virtual ~Poller() = default;

    // 给所有IO复用保留统一的接口
    // virtual = 0 纯虚函数 在基类(此时为抽象类）中不给出具体实现，仅作为接口声明，要求派生类必须实现
    virtual Timestamp poll(int timeoutMs, ChannelList *arctiveChannels) = 0; // 启动epoll_wait
    // 传入Channel类型的指针，因为Channel.cc中使用的是 this指针 
    virtual void updateChannel(Channel *Channel) = 0; // 启动epoll_ctrl
    virtual void removeChannel(Channel* channel) = 0; // 把fd所感兴趣的事件delete掉
    
    // 判断参数channel是否在当前Poller当中
    bool hasChannel(Channel *channel) const;

    // EventLoop事件循环可以通过该接口获取默认的IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop *loop);

protected:
    // map的key表示：sockfd，value表示sockfd所属的Channel（通道类型）
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;

private:
    EventLoop *ownerLoop_; // 定义Poller所属的事件循环EventLoop
};