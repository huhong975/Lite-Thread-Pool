#include <threadpool.h>

constexpr int TASK_MAX_THRESHHOLD = 4;

ThreadPool::ThreadPool()
    : initThreadSize_(0),
      taskSize_(0),
      taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
      poolMode_(PoolMode::MODE_FIEXED)
{
}

ThreadPool::~ThreadPool() {}

void ThreadPool::start(size_t initThreadSize)
{
    this->initThreadSize_ = initThreadSize;
    // 创建线程对象
    for (int i = 0; i < initThreadSize_; i++)
    {
        auto ptr = std::make_unique<Thread> (std::bind(&ThreadPool::threadFunc, this));
        threads_.emplace_back(std::move(ptr)); // unique_ptr 不允许左值拷贝，但是开放了右值拷贝
    }
    // 启动所有线程
    for (int i = 0; i < initThreadSize_; i++)
    {
        threads_[i]->start(); // 需要去执行一个线程函数
    }
}

void ThreadPool::setMode(PoolMode mode)
{
    this->poolMode_ = mode;
}

void ThreadPool::setTaskQueMaxTreshHold(size_t threshhold)
{
    this->taskQueMaxThreshHold_ = threshhold;
}

Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
    // 获取锁
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    // 线程的通信，等待
    // while(taskQue_.size() == taskQueMaxThreshHold_)
    // {
    //     notFull_.wait(lock);
    // }
    // notFull_.wait(lock, [&]() -> bool { return taskQue_.size() < taskQueMaxThreshHold_; });
    if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]() -> bool
                          { return taskQue_.size() < taskQueMaxThreshHold_; }))
    {
        std::cerr << "task queue if full, submit task fail." << std::endl;
        return Result(std::move(sp), false);
    }
    // 如果有空余，把人物放入任务不队列中
    taskQue_.emplace(sp);
    taskSize_++;
    // 因为新放了任务，任务队列肯定不空了，在notEmpty_上进行通知
    notEmpty_.notify_all();

    // 需要根据任务数量和空闲线程的数量，判断是否需要创建新的线程出来？


    return Result(sp);
}

void  ThreadPool::threadFunc()
{
    // std::cout << "begin threadFunc tid:" << std::this_thread::get_id() << std::endl;
    // std::cout << "end threadFunc tid: " << std::this_thread::get_id() << std::endl;
    for (;;)
    {
        std::shared_ptr<Task> task;
        {
            std::unique_lock<std::mutex> lock(taskQueMtx_); // 先获取锁
            std::cout << "tid:" << std::this_thread::get_id() << "尝试获取任务..." << std::endl;
            // 等待notempty 条件
            
            // cached模式下，有可能已经创建了很多的线程，但是空闲时间超过60s，应该把多余的线程回收掉？？

            notEmpty_.wait(lock, [&]() -> bool{ return !taskQue_.empty(); });
            std::cout << "tid:" << std::this_thread::get_id() << "获取任务成功" << std::endl;
            // 从任务队列中取一个任务出来
            task = taskQue_.front();
            taskQue_.pop();
            taskSize_--;

            // 通知
            if (taskQue_.size() > 0)
                notEmpty_.notify_all();
            notFull_.notify_all();
        } // 释放锁

        // 当前线程负责执行这个任务
        if(task != nullptr) 
        {
            // task->run();
            task->exec();
        }
    }
}

Thread::Thread(ThreadFunc func)
    :func_(func)
{}

Thread::~Thread() {}

void Thread::start()
{
    // 创建一个线程来执行线程函数
    std::thread t(func_); // 对C++11来说 线程对象t 和线程函数func_
    t.detach(); // 设置分离线程
}
Task::Task()
    : result_(nullptr)
{}
void Task::exec()
{
    if (result_)
        result_->setVal(run());
}
void Task::setResult(Result *res)
{
    result_ = res;
}