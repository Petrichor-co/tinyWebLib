#pragma once

#include "Poller.h"
#include "Timestamp.h"

#include <vector>
#include <sys/epoll.h>

class Channel;

class EPollPoller : public Poller
{
public:
    // 1. epoll_create
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override; // 前面定义了虚函数

    // 重写基类Poller的抽象方法
    // 2. epoll_wait
    Timestamp poll(int timeoutMs, ChannelList *arctiveChannels) override;
    // 3. epoll_ctl
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    static const int kInitEventListSize = 16; // 初始化vector长度

    // 填写活跃的链接
    void fillActiveChannles(int numEvents, ChannelList *activeChannles) const;
    // 更新Channel通道
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>; // 事件数组

    int epollfd_; // epoll_wait的第一个参数
    EventList events_; // epoll_wait的第二个参数
};