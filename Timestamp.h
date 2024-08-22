#pragma once

#include<string>
#include<iostream>

class Timestamp
{
public:
    Timestamp(); // 默认构造
    explicit Timestamp(int64_t microSecondsSinceEpoch); // 带参数构造 // explicit用于含有一个参数的构造函数，禁止类对象之间的隐式转换，以及禁止隐式调用拷贝构造函数
    static Timestamp now(); // now方法 获取当前的时间
    std::string  toString() const;// 获取当前时间年月日格式输出

private:
    int64_t microSecondsSinceEpoch_; // 底层成员变量是一个int64_t位的记录事件的整数microSecondsSinceEpoch_

};