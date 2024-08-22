#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h> // 信号

std::atomic_int Thread::numCreated_(0);

// 构造函数
Thread::Thread(ThreadFunc func, const std::string &name) //  = std::string()  参数默认值声明的时候给，定义的时候不用给
    : started_(false)
    , joined_(false)
    // , thread_() 这个是在start()中初始化的，意味着开启线程
    , tid_(0)
    , func_(std::move(func)) // move 底层资源直接给成员变量
    , name_(name)
{
    setDefaultName(); // 给线程设置默认名字
}
// 析构函数
Thread::~Thread()
{
    if (started_ && !joined_) //线程运行起来且不是
    {
        thread_->detach(); // thread 提供了设置线程分离的方法
    }
}

// 启动当前线程
void Thread::start() // 一个Thread对象就是记录的一个新线程的详细信息
{
    started_ = true;
    // 信号量的作用是等待新线程的创建完成，以确保线程对象的 tid_ 成员变量被正确设置
    // 解释：如果strat线程走的快，直接执行到sem(wait(&sem)，由于还没有子线程没有给信号量加一 sem_post(&sem)，
    // 所以就阻塞住了，等待子线程给信号量加一， tid_已经正确设置了，就可以继续执行了
    sem_t sem;
    sem_init(&sem, false, 0);

    // 开启线程
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        // 获取线程的tid值
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        //开启一个新线程，专门执行该线程函数
        func_(); //包含一个eventLoop
    }));

    // 这里必须等待获取上面新创建线程的tid值
    sem_wait(&sem);
}

//当前线程阻塞，直到被调用的线程执行完毕并退出
//也可以理解为主线程等待其他线程完成，确保主线程结束之前所有的子线程都已经完成
void Thread::join()
{
    joined_ = true;
    thread_->join();
}

// 给线程设置默认的名字
void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf,sizeof(buf),"Thread %d",num);
        name_ = buf;
    }
}