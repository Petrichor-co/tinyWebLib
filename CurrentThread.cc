#include "CurrentThread.h"

namespace CurrentThread
{
    __thread int t_cachedTid = 0;

    void cacheTid()
    {
        if (t_cachedTid == 0)
        {   
            // 通过Linux系统调用获得当前线程的值
            t_cachedTid = static_cast<pid_t> (::syscall(SYS_gettid));
        }

    }
}