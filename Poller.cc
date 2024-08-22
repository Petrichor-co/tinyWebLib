#include "Poller.h"
#include "Channel.h"

Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop)
{
}

bool Poller::hasChannel(Channel *channel) const
{
    auto it = channels_.find(channel->fd());  // channels_是ChannelMap
    return it != channels_.end() && it->second == channel;
}

// 下面语法上没问题，但这个函数要生成具体的IO对象，并返回一个基类的指针。所以一定要包含 #include "PollPoller.h" #include "EpollPoller" 这几个Poller的派生类
// 派生类可以引用基类，基类不能引用派生类，所以不能在这里引用派生类的对象。所以需要写一个源文件DefaultPoller.cc实现

// Poller* newDefaultPoller(EventLoop *loop){}
