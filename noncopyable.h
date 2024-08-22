#pragma once   // 防止头文件被重复包含
 
/*
派生类在调用构造函数和析构函数时会首先调用基类的构造函数和析构函数

noncopyable 被继承后 派生类对象可以正常调用 构造函数 和 析构函数
但是派生类对象无法使用拷贝构造函数 和 赋值操作
*/
class noncopyable
{
public:
    noncopyable(const noncopyable&) = delete; // 拷贝
    noncopyable& operator=(const noncopyable&) = delete; // 赋值
    
protected:
    noncopyable() = default;
    ~noncopyable() = default;
};