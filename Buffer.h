#pragma once

#include <vector>
#include <string>
#include <algorithm>

// 网络库底层的缓冲区类型定义
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;//头部字节大小 记录数据包的长度 
    static const size_t kInitialSize = 1024;//缓冲区的大小

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + kInitialSize)//开辟的大小
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {}

    //  prependableBytes    |       readableBytes         |       writableBytes
    //     <=         readerIndex_       <=         writerIndex_     <=    buffer_.size()
    //可读的数据长度 
    size_t readableBytes() const 
    {
        return writerIndex_ - readerIndex_;
    }

    //可写的缓冲区长度 
    size_t writableBytes() const 
    {
        return buffer_.size() - writerIndex_;
    }

    //返回头部的空间的大小 
    size_t prependableBytes() const 
    {
        return readerIndex_;
    }

    //返回缓冲区 可读 数据的起始地址
    const char* peek() const
    {
        return begin() + readerIndex_;
    }

    // 在onMessage的时候 把数据从Buffer转成string类型
    void retrieve(size_t len)
    {
        if (len < readableBytes())
        {
            readerIndex_ += len; // 应用只读取了可读缓冲区数据的一部分，就是len长度，还剩下 readerIndex_ += len -> writerIndex_
        } else { // len == readableBytes()
            retrieveAll();
        }
    }
    
     //将readerIndex_和writerIndex_复位
    void retrieveAll()
    {   
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

     //把onMessage函数上报的Buffer数据，转成string类型的数据返回
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes());// 应用可读取数据的长度
    }

    std::string retrieveAsString(size_t len) 
    {
        std::string result(peek(), len);
        retrieve(len); // 上面一句把缓冲区中可读的数据，已经读取出来，这里肯定要对缓冲区进行 复位 操作
        return result;
    }

    // 确保剩余可写缓冲区大小比len大，否则调用makeSpace扩容
    // buffer_.size() - writeIndex_            
    void ensureWriteableBytes(size_t len) 
    {
        if (writableBytes() < len)
        {
            makeSpace(len); // 扩容函数
        }
    }

     // 把[data, data+len]内存上的数据，添加到writable缓冲区当中
    void append(const char *data, size_t len)
    {
        ensureWriteableBytes(len);
        std::copy(data, data+len, beginWrite()); // 把你要读取的数据data放到可写的缓冲区beginWrite()中   copy(_II __first, _II __last, _OI __result)
        writerIndex_ += len;
    }

    char* beginWrite()
    {
        return begin() + writerIndex_;
    }

    const char* beginWrite() const
    {
        return begin() + writerIndex_;
    }

    // TcpConnection.cc中的handleRead和handleWrite中使用
    // 从fd上读取数据 
    ssize_t readFd(int fd, int* saveErrno);
    // 通过fd发送数据
    ssize_t writeFd(int fd, int* saveErrno);

private:
    char* begin()
    {   
        // it.operator*().operator&()  先 * 获取迭代器指向的元素，然后再 & 获取元素的地址，也就是数组的起始地址
        return &*buffer_.begin();
    }
    const char* begin() const
    {
        return &*buffer_.begin();
    }

    // 扩容函数
    void makeSpace(size_t len) 
    {
        /*
          prependableBytes    |       readableBytes         |       writableBytes
             <=         readerIndex_       <=         writerIndex_     <=    buffer_.size()

        kCheapPrepend  |  reader  |  writer  |
        kCheapPrepend  |          len           |
        
        kCheapPrepend  |  reader  |  writer  | 这些空间加起来都没有len + kCheapPrepend大
        */
       if (writableBytes() + prependableBytes() < len + kCheapPrepend)
       {
            buffer_.resize(writerIndex_ + len);
       }
       else // writableBytes() + prependableBytes() 的空间够写len长度的数据
       {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_, // 前面有空闲空间，后面也有空闲空间，把中间未读取数据往begin() + kCheapPrepend处搬
                    begin() + writerIndex_,
                    begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend; // 回到原位置   8下标位置
            writerIndex_ = readerIndex_ + readable;
       }
    }
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;

};