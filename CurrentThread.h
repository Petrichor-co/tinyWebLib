#pragma once
 
#include <unistd.h>
#include <sys/syscall.h>
 
namespace CurrentThread
{
    //全局变量 __thread (C++11 是thread_local , 系统中是__thread)
    //全局变量，但是会在每一个线程存储一份拷贝
    //每个线程都有一个自己的t cachedTid。
    extern __thread int t_cachedTid;
 
    //Tid的访问是一个系统调用，总是从用空间切换到内核空间，比较浪费效率!
    //第一次访问，就把当前线程的Tid存储起来，后边如果再访问就从缓存取
    void cacheTid();
 
    
    inline int tid()//内联函数，在当前文件起作用 
    {
        if (__builtin_expect(t_cachedTid == 0, 0))//还没有获取过当前线程的id 
        {
            cacheTid();
        }
        return t_cachedTid;
    }
}