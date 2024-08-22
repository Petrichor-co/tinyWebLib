#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

//防止一个线程创建多个EventLoop   __thread：thread_local线程局部存储关键字用于声明一个线程局部变量
//当一个eventloop创建起来它就指向那个对象，在一个线程里再去创建一个对象，由于这个指针不为空，就不创建 
__thread EventLoop *t_loopInThisThread = nullptr;

//定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs = 10000;//10秒钟 

//创建wakeupfd，用来notify唤醒subReactor处理新来的channel
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d \n", errno);
    }
    return evtfd;
}

// 构造函数
EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_))//this:需要知道Channnel所在的loop
    , currentActiveChannel_(nullptr)
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if (t_loopInThisThread)//这个线程已经有loop了，就不创建了 
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d, FATAL and exit \n", t_loopInThisThread, threadId_);
    }
    else//这个线程还没有loop，创建 
    {
        t_loopInThisThread = this;
    }
    //设置 wakeupfd 的事件类型以及发生事件后的回调操作,  wakeupfd就是为了唤醒subloop的！
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    //每一个eventloop都将监听wakeupchannel的EPOLLIN读事件了
    wakeupChannel_->enableReading();
}
// 析构函数
EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// ⭐ 最核心函数 loop：
//开启事件循环！  驱动底层的poller执行poll 
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;
 
    LOG_INFO("EventLoop %p start looping... \n", this);
 
    while(!quit_)
    {
        //先清空activeChannels_容器
        activeChannels_.clear();
        //1、监听两类fd   一种是client的fd，一种wakeupfd
        //通过poller的poll方法底层调用  epoll_wait 把活跃Channel都放到activeChannels_容器中
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);

        //2、遍历 activeChannels_ 调用Channel中的 handleEvent 去执行具体事件类型的操作
        for (Channel *channel : activeChannels_)
        {
            //Poller能监听哪些channel发生事件了，然后上报给EventLoop，EventLoop通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);//事先已经绑定好
        }
 
        // 3、执行当前EventLoop事件循环需要处理的 回调 操作
        /** mainLoop只做accept新用户的连接的工作  （mainLoop相当于mainReactor）
         * IO线程 mainLoop：accept  channel打包fd ---》 subloop  1个mainloop 3个subloop 
         * mainLoop 事先注册一个回调cb（需要subloop来执行）  mainloop wakeup subloop后，执行下面的方法，执行之前mainloop注册的cb操作
         */ 
        doPendingFunctors();//mainloop注册回调给subloop
    }
 
    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
}


//退出事件循环  
//1.loop在自己的线程中调用 quit：不会阻塞在poll函数上，因为在自己的线程上执行一个函数，肯定从poll函数上返回了!
//2.在非loop的线程中，调用loop的quit：
// 比如：在一个subloop(worker线程)中，调用了mainLoop(IO线程)的quit
// 这时候，应该给mainLoop唤醒 ，它就从poll返回回来
// 此时你已经将其quit 置为true，在回到loop函数的while循环中，已经不满足表达式了! 就结束了
/**
 *              mainLoop
 * 
 *        直接通过wakeupfd实现线程间唤醒          muoduo库里没有 ==================== 生产者-消费者的线程安全的队列
                                                mainloop生产 subloop消费  逻辑好处理 但是muduo库没有这个 
                                                是直接通过wakefd通信实现线程间notify唤醒 
 * 
 *  subLoop1     subLoop2     subLoop3    
 */ 
void EventLoop::quit()
{
    quit_ = true; //上面loop函数中的while(!quit_)循环结束
 
    if (!isInLoopThread())  // 不在主线程里，所以isInLoopThread()是false
    {
        wakeup();//因为不知道主线程是什么情况，需要唤醒一下 
    }
}

//在当前loop中执行cb，cb是回调操作
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) //在当前的loop线程中，执行cb
    {
        cb();
    }
    else //在非当前loop线程中执行cb , 就需要唤醒loop所在线程，执行cb
    {
        queueInLoop(cb);
    }   
}

//把cb放入队列中，唤醒loop所在的线程，执行cb
//一个loop运行在自己的线程里。比如在subloop2调用subloop3的 runInLoop
void EventLoop::queueInLoop(Functor cb)
{   
    {
    std::unique_lock<std::mutex> lock(mutex_); //智能锁，因为有并发的访问
    pendingFunctors_.emplace_back(cb);//C++11 emplace_back： 直接在底层vector内存里构造cb，而push_back是拷贝构造
    }

    //唤醒相应的，需要执行上面回调操作的loop的线程了
    // || callingPendingFunctors_的意思是：当前loop正在执行回调，但是loop又有了新的回调
    if (!isInLoopThread() || callingPendingFunctors_) 
    {
        wakeup();//唤醒loop所在线程，继续执行回调 
    }

}


//唤醒subLoop用的
void EventLoop::handleRead()//就是随便读一个数 1，写啥读啥无所谓，就是为了唤醒loop线程执行回调 
{
  uint64_t one = 1;
  ssize_t n = read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8", n);
  }
}

//用来唤醒loop所在的线程的 (mainReactor用来唤醒subReactor)
//向wakeupfd_写一个数据，wakeupChannel就发生读事件，当前loop线程就会被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    size_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)//子线程无法被唤醒 
    {
        LOG_ERROR("EventLoop::wakeup() write %lu bytes instead of 8 \n", n);
    }
}

//EventLoop的方法，channel.cc中调用的，channel想在Poller上更新删除，但是没法直接完成，需要EventLoop调用poller的函数间接完成
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel); // Poller的派生类EPollPoller实现的
}
void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors() // 执行回调  (操作具体实现在TcpServer中)
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);//资源交换，把pendingFunctors_ 置为空
        //不需要pendingFunctors_了  不妨碍 mainloop向 pendingFunctors_写回调操作cb
    }
    for (const Functor &functor : functors)
    {
        functor();//执行当前loop需要执行的回调操作
    }
 
    callingPendingFunctors_ = false;
}