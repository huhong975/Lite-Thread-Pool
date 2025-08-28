#include <chrono>
#include <numeric>
#include "threadpool.h"

using uLong =  uint64_t;

class Mytask : public Task
{
public:
    Mytask(size_t begin, size_t end)
        : begin_(begin)
        , end_(end)
        {};
    Any run()
    {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        std::cout << "tid:" << std::this_thread::get_id() << "begin!" << std::endl;
        size_t sum = 0;
        for (size_t i = begin_; i <= end_; i++)
            sum += i;
        std::cout << "tid: " << std::this_thread::get_id() << "end!" << std::endl;
        return sum;
    }

private:
    size_t begin_;
    size_t end_;
};

int main ()
{
    // 问题：ThreadPool对象析构以后，怎么把线程池相关的线程资源全部回收？
    {
        ThreadPool thread_pool;
        thread_pool.setMode(PoolMode::MODE_CACHED);
        thread_pool.start(4);
        Result res1 = thread_pool.submitTask(std::make_shared<Mytask>(1, 100000000));
        Result res2 = thread_pool.submitTask(std::make_shared<Mytask>(100000001, 200000000));
        Result res3 = thread_pool.submitTask(std::make_shared<Mytask>(200000001, 300000000));
        Result res4 = thread_pool.submitTask(std::make_shared<Mytask>(200000001, 300000000));

        Result res5 = thread_pool.submitTask(std::make_shared<Mytask>(200000001, 300000000));
        Result res6 = thread_pool.submitTask(std::make_shared<Mytask>(200000001, 300000000));
        // // get返回了一个Any类型，怎么转换成具体的类型呢？
        size_t sum1 = res1.get().cast_<uLong>();
        size_t sum2 = res2.get().cast_<uLong>();
        size_t sum3 = res3.get().cast_<uLong>();
        std::cout << "sum:" << (sum1 + sum2 + sum3) << std::endl;
    }
    getchar();
}