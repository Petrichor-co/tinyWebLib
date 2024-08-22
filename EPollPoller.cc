#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>

// channel未添加到 poller 中
const int kNew = -1; //channel的index_初始化就是-1，所以index_表示的就是它在poller中的状态
// channel已添加到 poller 中
const int kAdded = 1;
// channel在 poller 中删除
const int kDeleted = 2;

// epoll_create 
EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitEventListSize) // events_ 是一个vector<epoll_event>的容器，这里定义为大小为16的数组
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error : %d \n", errno);
    }
}
// epoll的fd的close 
EPollPoller::~EPollPoller()
{
    ::close(epollfd_);  
}

// ⭐ epoll_wait 会一直执行着
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *arctiveChannels)
{
    // 实际上应该用LOG_DEBUG输出日志更为合理
    LOG_INFO("func = %s => fd total count = %lu /n", __FUNCTION__, channels_.size());
    // epoll_wait 的第二个参数需要传入一个 epoll_event类型的地址
    // &*events_.begin() events_.begin()获得首个元素的迭代器，然后通过*解引用获取首个元素，最后&取地址
    // static_cast 类型安全的转换 因为epoll_wait输入的是一个int
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);

    int saveErrno = errno;

    Timestamp now(Timestamp::now());

    if (numEvents > 0)
    {
        LOG_INFO("%d events happened !", numEvents);
        fillActiveChannles(numEvents, arctiveChannels);
        if (numEvents == events_.size()) // 所有监听的事件都发生了，events_数组需要扩容了
        {
            events_.resize(events_.size() * 2);
        }
        else if (numEvents == 0) // epoll_wait 这一轮没有监听到事件，超时了
        {
            LOG_DEBUG("%s timeout ! \n", __FUNXTION__);
        }
        else 
        {
            if (saveErrno != EINTR) // 不等于外部中断，是由其他错误类型造成的
            {
                errno = saveErrno;
                LOG_ERROR("EPollerPoll::poll() error !"); 
            }
        }
    }
    return now;
}

// 下面两个函数是 epoll_ctl
// channel update remove => EventLoop updateChannel removeChannel => Poller updateChannel removeChannel
/**
 *                EventLoop => Poller.poll 
 * ChannelList  add/remove/mod->  Poller
 *                          ChannelMap <fd, Channel*>
*/
// 从Poller中更新channel的逻辑
void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("func = %s => fd= %d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);

    if (index == kNew || index == kDeleted){
        if (index == kNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel; // channels_ 是抽象基类Poller的map类型的成员变量
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);

    } else // channel已经在 Poller 上注册过了，update或者delete
    {
        int fd = channel->fd();
        if (channel->isNoneEvent()) // 如果 fd 对事件都不感兴趣了
        {
            update(EPOLL_CTL_DEL, channel); // 删除
        } 
        else // fd 还是对一些事件感兴趣的
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// 从Poller中删除channel的逻辑
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd(); 
    channels_.erase(fd); // channels_是个 unordered_map<int, Channel*>

    int index = channel->index();

    LOG_INFO("func = %s => fd= %d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);

    if (index = kAdded) // 如果在 channel已经在Poller里了，那么就删除掉
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew); // 重新设置index的状态为 未添加

}

// 填写活跃的连接
void EPollPoller::fillActiveChannles(int numEvents, ChannelList *activeChannles) const
{
    for (int i = 0; i < numEvents; ++i){
        Channel *channel = static_cast<Channel*> (events_[i].data.ptr); // 因为events_[i].data.ptr是void*类型，所以要强转换
        channel->set_revents(events_[i].events);
        activeChannles->push_back(channel); // EventLoop就拿到了它的Poller所有发生事件的channel列表
    }
}


// 更新通道 进行 add delete modify 等操作 epoll_ctl
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    // memset(&event, 0, sizeof event);
    bzero(&event, sizeof event);
    int fd = channel->fd();

    event.events = channel->events(); // channel中fd感兴趣事件赋值给evnet
    event.data.fd = fd; // epoll_event中数据联合体中的fd，设置为当前channel的fd
    event.data.ptr = channel; // epoll_event中数据联合体中的指针，指向当前的channel，相当于绑定到channel上了
   
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {   
        // 出错了
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error: %d /n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error: %d /n", errno);
        }
    }
}


