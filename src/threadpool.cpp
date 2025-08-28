#include <threadpool.h>

constexpr int TASK_MAX_THRESHHOLD = INT32_MAX;
constexpr int THREAD_MAX_THRESHHOLD = 1024;
constexpr int THREAD_MAX_IDLE_TIME = 60;
ThreadPool::ThreadPool()
    : initThreadSize_(0),
      taskSize_(0),
      idleThreadSize_(0),
      curThreadSize_(0),
      taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
      threadSizeThreshHold_(THREAD_MAX_THRESHHOLD),
      poolMode_(PoolMode::MODE_FIEXED),
      isPoolRunning_(false)
{}

ThreadPool::~ThreadPool() 
{

}

void ThreadPool::start(size_t initThreadSize)
{
    this->isPoolRunning_ = true;
    this->initThreadSize_ = initThreadSize;
    this->curThreadSize_ = initThreadSize;
    // 创建线程对象
    for (int i = 0; i < initThreadSize_; i++)
    {
        auto ptr = std::make_unique<Thread> (std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        // threads_.emplace_back(std::move(ptr)); // unique_ptr 不允许左值拷贝，但是开放了右值拷贝
        size_t threadId = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));
    }
    // 启动所有线程
    for (int i = 0; i < initThreadSize_; i++)
    {
        threads_[i]->start(); // 需要去执行一个线程函数
        idleThreadSize_++;
    }
}

void ThreadPool::setMode(PoolMode mode)
{
    if (checkRunningState())
        return;
    this->poolMode_ = mode;
}

void ThreadPool::setTaskQueMaxTreshHold(size_t threshhold)
{
    if (checkRunningState())
        return;
    this->taskQueMaxThreshHold_ = threshhold;
}

void ThreadPool::setThreadSizeTreshHold(size_t threshhold)
{
    if (checkRunningState())
        return;
    if (poolMode_ == PoolMode::MODE_CACHED)
    {
        this->threadSizeThreshHold_ = threshhold;
    }
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

    // cached 模式， 任务处理比较紧急 场景：小而快的任务
    // 需要根据任务数量和空闲线程的数量，判断是否需要创建新的线程出来？
    if (poolMode_ == PoolMode::MODE_CACHED &&
        taskSize_ > idleThreadSize_ &&
        curThreadSize_ < threadSizeThreshHold_)
    {
        // 创建新线程
        std::cout << ">>> create new thread" << std::endl;
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        // threads_.emplace_back(std::move(ptr));
        size_t threadId = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));
        threads_[threadId]->start();
        curThreadSize_++;
        idleThreadSize_++;
    }

    return Result(sp);
}

void  ThreadPool::threadFunc(size_t threadid)
{
    // std::cout << "begin threadFunc tid:" << std::this_thread::get_id() << std::endl;
    // std::cout << "end threadFunc tid: " << std::this_thread::get_id() << std::endl;
    auto lastTime = std::chrono::high_resolution_clock().now();
    for (;;)
    {
        std::shared_ptr<Task> task;
        {
            std::unique_lock<std::mutex> lock(taskQueMtx_); // 先获取锁
            // std::cout << "tid:" << std::this_thread::get_id() << "尝试获取任务..." << std::endl;
            // 等待notempty 条件
            
            // cached模式下，有可能已经创建了很多的线程，但是空闲时间超过60s，应该把多余的线程
            // 结束回收掉（超过initThreadSize_数量的线程要进行回收）
            // 当前时间 -  上一次线程执行的时间 > 60s
            if(poolMode_ == PoolMode::MODE_CACHED)
            {
                // 每一秒钟返回一次
                // 怎么区分：超时返回？还是有任务待执行返回
                while(taskQue_.empty())
                {
                    // 条件变量超时返回了
                    if(std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
                    {
                        auto now = std::chrono::high_resolution_clock().now();
                        auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime).count();
                        if (dur >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
                        {
                            // 开始回收线程
                            // 修改记录线程数量的变量
                            // 把线程对象东容器中删除 难点：（没有办法判断threadFunc 对应的thread对象是哪一个）
                            // threadid => thread对象 => 删除
                            threads_.erase(threadid);
                            curThreadSize_--;
                            idleThreadSize_--;
                            std::cout << "threadid:" << std::this_thread::get_id() << "exit!" << std::endl;
                            return;
                        }
                    }
                }
            }else
            {
                notEmpty_.wait(lock, [&]() -> bool{ return !taskQue_.empty(); });
            }
            idleThreadSize_--;
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
        idleThreadSize_++;
        lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完任务的时间
    }
}

bool ThreadPool::checkRunningState() const
{
    return isPoolRunning_;
}

size_t Thread::generateId_ = 0;

Thread::Thread(ThreadFunc func)
    : func_(func)
    , threadId_(generateId_++)
{}

Thread::~Thread() {}

void Thread::start()
{
    // 创建一个线程来执行线程函数
    std::thread t(func_, threadId_); // 对C++11来说 线程对象t 和线程函数func_
    t.detach(); // 设置分离线程
}

size_t Thread::getId() const
{
    return this->threadId_;
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