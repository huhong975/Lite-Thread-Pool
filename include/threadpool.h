#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <thread>
// 线程池支持的模式
enum class PoolMode
{
    MODE_FIEXED,
    MODE_CACHED,
};

// Any 类型：可以接收任意数据的类型
class Any
{
public:
    Any() = default;
    ~Any() = default;
    // 禁止复制和左值引用，保留右值引用
    Any(const Any&) = delete;
    Any& operator=(const Any&) = delete;
    Any(Any&&) = default;
    Any& operator=(Any&&) = default;
public:
    // Any类型接受任意其他数据
    template<typename T>
    Any(T data) : base_(std::make_unique<Derive<T>>(data))
    {}

    // 把Any对象中存储的data数据提取出来
    template<typename T>
    T cast_()
    {
        // 从基类指针base_找到他所指向的Derive对象，从它里面取出data成员变量
        Derive<T> *pd = dynamic_cast<Derive<T>*>(base_.get());
        if(pd == nullptr)
        {
            throw "type is unmatch";
        }
        return pd->data_;
    }
private:
    // 基类类型
    class Base
    {
    public:
        virtual ~Base() = default;
    };

    // 派生类类型
    template <typename T>
    class Derive : public Base
    {
    public:
        Derive(T data) : data_(data) {}
        T data_;
    };
private:
    // 定义一个基类指针(不能复制和左值引用)
    std::unique_ptr<Base> base_;
};

// 信号量
class Semaphore
{
public:
    Semaphore(size_t limit = 0)
        : resLimit_(limit)
    {}
    ~Semaphore() = default;
    
    // 获取一个信号量资源
    void wait()
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cond_.wait(lock, [&]() -> bool {return resLimit_ > 0;});
        resLimit_--;
    }
    // 增加一个信号量资源
    void post()
    {
        std::unique_lock<std::mutex> lock(mtx_);
        resLimit_++;
        cond_.notify_all();
    }
private:
    size_t resLimit_;
    std::mutex mtx_;
    std::condition_variable cond_;
};
class Result;
class Task
{
public:
    Task();
    ~Task() = default;
    void exec();
    virtual Any run() = 0;
    void setResult(Result* res);
private:
    Result* result_;
};

// 实现接受提交到线程池的task执行完成后的返回值类型Result
class Result
{
public:
    Result(std::shared_ptr<Task> task, bool isValid = true)
    : isValid_(isValid)
    , task_(task)
    {
        task_->setResult(this);
    }
    ~Result() = default;
    // setVal方法，获取任务执行完的返回值
    void setVal(Any any)
    {
        // 存储task返回值
        this->any_ = std::move(any);
        sem_.post();
    }
    // get方法，用户调用这个方法获取task的返回值
    Any get()
    {
        if (!isValid_) return Any();
        sem_.wait();
        return std::move(any_);
    }
private:
    Any any_;
    Semaphore sem_;
    std::shared_ptr<Task> task_;  // 指向对应获取返回值的任务对象
    std::atomic_bool isValid_;
};

// 任务抽象基类


class Thread
{
public:
    using ThreadFunc = std::function<void()>;

    Thread(ThreadFunc func);
    ~Thread();
    void start();

private:
    ThreadFunc func_;
};

/*
example
ThreadPool pool;
pool.start(4);
class MyTask: public Task
{
public :
    void run() { // thread code}
}
pool.submitTask(std::make_shared<MyTask>());
*/

class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    void start(size_t initThreadSize = 4);
    void setMode(PoolMode mode);
    void setTaskQueMaxTreshHold(size_t threshhold);
    Result submitTask(std::shared_ptr<Task> sp);

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    // 定义线程函数
    void  threadFunc();
private:
    std::vector<std::unique_ptr<Thread>> threads_; // 使用智能指针，避免内存泄漏
    size_t initThreadSize_;

    std::queue<std::shared_ptr<Task>> taskQue_;
    std::atomic_int taskSize_; //任务数量
    size_t taskQueMaxThreshHold_; // 任务队列数量上限阈值

    std::mutex taskQueMtx_;
    std::condition_variable notFull_;
    std::condition_variable notEmpty_;
    PoolMode poolMode_;
};

#endif