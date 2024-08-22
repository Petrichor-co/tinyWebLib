#pragma once

#include "noncopyable.h"

#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <atomic>
#include <string>


class Thread : noncopyable
{
public:

    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc func, const std::string &name = std::string());
    ~Thread();

    void start();// 启动当前线程
    void join();// 当前线程等待其他线程完了再运行下去

    bool started() const { return started_;}
    bool joined() const { return joined_; }
    const std::string& name() const { return name_; }

    static int numCreated() {return numCreated_;}
private:
    void setDefaultName(); //给线程设置默认的名字

    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;
    ThreadFunc func_; // 存储线程的函数
    std::string name_;
    static std::atomic_int numCreated_; // 线程数量


};