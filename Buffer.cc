#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/*
    从fd上读取数据  Poller工作在LT模式
    Buffer缓冲区是有大小的！ 但是从fd上读数据的时候，却不知道TCP数据最终的大小
*/ 
ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    char extrabuf[65536] = {0}; // 这是栈上的内存空间，一次最多读64K

    struct iovec vec[2]; // iovec结构体有两个成员iov_base和iov_len
    const size_t writable = writableBytes();//这是Buffer底层缓冲区 剩余 的可写空间大小: buffer_.size() - writerIndex_

    // 第一块缓冲区
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    // 第二块缓冲区(如果第一块不够，就需要往这里面填数据)
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1; // writable小于 65536/1024 = 64，iovcnt为2
    const ssize_t n = ::readv(fd, vec, iovcnt); // extern ssize_t readv (int __fd, const struct iovec *__iovec, int __count) __wur;
    if (n < 0) // 出错
    {
        *saveErrno = errno;
    }
    else if (n <= writable) // Buffer的可写缓冲区已经够存储读出来的数据了，即vec[0]够用
    {
        writerIndex_ += n;
    }
    else // extrabuf里面也写入了数据，即vec[0]不够用
    {
        writerIndex_ = buffer_.size(); // writableBytes写满了，所以writerIndex_等于buffer_.size()，相当于到Buffer的末尾了
        append(extrabuf, n-writable); // 从writeIndex_开始写 n-writable 大小的数据，writable大小的数据在Buffer中写了，即vec[0]
    }
    return n;
}



ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0) 
    {
        *saveErrno = errno;
    }
    return n;
}