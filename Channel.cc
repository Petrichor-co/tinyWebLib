#include "Channel.h"
#include "EventLoop.h"

#include "Logger.h"

#include <sys/epoll.h>//epoll的表示

const int Channel::kNoneEvent = 0; // 无事件，都不感兴趣
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI; //epoll事件宏，对应的文件描述符可读 或 有紧急的数据可读  (muduo默认用epoll)
const int Channel::kWriteEvent = EPOLLOUT; // 对应的文件表舒服可写

//构造函数
//EventLoop底层：ChannelList   Poller 每个channel属于1个loop
//EventLoop *loop 将Channel所属的loop记下来
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1)
    , tied_(false)
{

}

//析构函数
Channel::~Channel(){
    // assert(!eventHandling_);
    // assert(!addedToLoop_);
    // if (loop_->isInLoopThread()) //（一个线程里有一个EventLoop有一个poller，有多个channel）判断一个channel必须是否在当前事件循环里，是否在当前事件循环所在的线程里
    // {
    // assert(!loop_->hasChannel(this));
    // }
}

// channel的tie方法什么时候调用呢？一个TcpConnection新连接创建的时候，TcpConnection => Channel
void Channel::tie(const std::shared_ptr<void>& obj)  // 区分 智能指针的提升操作，弱.lock()->强，观察者模式下用来判断 监听者对象 是否存活
{
    tie_ = obj; // 将一个shared_ptr赋值给weak_ptr时，实际上是将资源的控制权交给了shared_ptr，然后将tied_标记为true，表示绑定操作已经完成。
    tied_ = true; // 把tied_改为true
} 


//当改变Channel所表示fd的events事件后，update负责在poller里面更改fd相应的事件  epoll_ctl
//enableReading() => ChannelList [Update()->UpdateChannel()] => EventLoop [UpdateChannel] => Poller（EventLoop中用一个vector保存了很多个channel）
void Channel::update() // channel要更新在poller上的状态，它无法直接访问poller，所以要在EventLoop上访问
{
    //通过channel所属的EventLoop，调用poller的相应方法，注册fd的events事件
    loop_->updateChannel(this);
}

// 在channel所属的Eventloop中，把当前的channel删除掉
void Channel::remove() // 删除通道
{
    loop_->removeChannel(this);
}


//fd得到poller通知后，处理事件的函数
void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_) // 如果绑定
    {
        std::shared_ptr<void> guard = tie_.lock(); // .lock() 智能指针的提升操作  弱->强
        if (guard) // 提升成功才执行，否则说明与tie_绑定的对象已经销毁，就忽略事件处理
        {
            handleEventWithGuard(receiveTime);
        }
    }else { // 没绑定
        handleEventWithGuard(receiveTime);
    }
}

// 根据poller通知的channel发生的具体事件，由channel负责具体调用的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{   
    LOG_INFO("channel handleEvent revents:%d", revents_);

    // read write close error
    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }

    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) // 发生异常了
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
    }

    if (revents_ & EPOLLERR) // 错误事件
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }
}