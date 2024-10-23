# tinyNetWorkLibrary

| **Part Ⅰ**            | **Part Ⅱ**            | **Part Ⅲ**            | **Part Ⅳ**            | **Part Ⅴ**            | **Part Ⅵ**            | **Part Ⅶ**            |
| --------------------- | --------------------- | --------------------- | --------------------- | --------------------- | --------------------- | --------------------- |
| [项目介绍](#项目介绍) | [项目特点](#项目特点) | [开发环境](#开发环境) | [并发模型](#并发模型) | [构建项目](#构建项目) | [运行案例](#运行案例) | [模块讲解](#模块讲解) |

## 项目介绍

本项目是参考 muduo 实现的基于 Reactor 模型的多线程网络库。使用 C++ 11 编写去除 muduo 对 boost 的依赖，同时使用部分modern C++的特性，让该网络库使用起来更加方便，而源码的阅读也更加容易。

项目已经实现了 Channel 模块、Poller 模块、事件循环模块、定时器模块、内存池模块。   

## 项目特点

- 底层使用 Epoll + LT 模式的 I/O 复用模型，并且结合非阻塞 I/O  实现主从 Reactor 模型。
- 采用「one loop per thread」线程模型，并向上封装线程池避免线程创建和销毁带来的性能开销。
- 采用 eventfd 作为事件通知描述符，方便高效派发事件到其他线程执行异步任务。
- 基于红黑树实现定时器管理结构，内部使用 Linux 的 timerfd 通知到期任务，高效管理定时任务。
- 尽可能使用智能指针管理内存，遵循 RAII理念，减小内存泄露风险。
- 提供面向用户的TcpServer类，用户只需要通过bind绑定自定义的回调方法即可开始使用。

## 开发环境

- 操作系统：`Ubuntu 20.04.6 LTS`
- 编译器：`g++ 9.4.0`
- 编辑器：`vscode`
- 版本控制：`git`
- 项目构建：`cmake 3.16.3`

## 并发模型

![image.png](https://cdn.nlark.com/yuque/0/2022/png/26752078/1670853134528-c88d27f2-10a2-46d3-b308-48f7632a2f09.png?x-oss-process=image%2Fresize%2Cw_937%2Climit_0)

项目采用主从 Reactor 模型，MainReactor 只负责监听派发新连接，在 MainReactor 中通过 Acceptor 接收新连接并轮询派发给 SubReactor，SubReactor 负责此连接的读写事件。

调用 TcpServer 的 start 函数后，会内部创建线程池。每个线程独立的运行一个事件循环，即 SubReactor。MainReactor 从线程池中轮询获取 SubReactor 并派发给它新连接，处理读写事件的 SubReactor 个数一般和 CPU 核心数相等。使用主从 Reactor 模型有诸多优点：

1. 响应快，不必为单个同步事件所阻塞，虽然 Reactor 本身依然是同步的；
2. 可以最大程度避免复杂的多线程及同步问题，并且避免多线程/进程的切换；
3. 扩展性好，可以方便通过增加 Reactor 实例个数充分利用 CPU 资源；
4. 复用性好，Reactor 模型本身与具体事件处理逻辑无关，具有很高的复用性；

## 构建项目

安装Cmake

```shell
sudo apt-get update
sudo apt-get install cmake
```

下载项目

```shell
git clone git@github.com:Petrichor-co/tinyNetWorkLibrary.git
```

执行脚本构建项目

```shell
chmod +x autobuild.sh
sudo ./autobuild.sh
```
![image](https://github.com/user-attachments/assets/9298b396-686a-4d0a-b4ff-d5d80d088122)


## 运行案例

这里以一个简单的回声服务器作为案例：

```shell
cd ./example
make
./EchoServer
```
注：如果make时报错：make: 'testserver' is up to date.  则说明之前已经编译过了，并且此次编译和之前编译的结果没有变化。


![image](https://github.com/user-attachments/assets/92d4b9b5-6b89-4215-b8ed-27652d3e0961)


## 模块讲解
依据个人对项目的理解自制的流程思路（精华）：
![image](https://github.com/user-attachments/assets/ce75c41b-78d3-4290-af0c-e784e787294d)
![image](https://github.com/user-attachments/assets/1461ab48-e017-4b0d-898e-ae13e7941806)
![image](https://github.com/user-attachments/assets/75ec42b0-04d6-493a-a6d0-784cef4c67e8)
![image](https://github.com/user-attachments/assets/cb4960ed-de08-4ee3-8adf-1db37bc5eca7)

继续更新......
