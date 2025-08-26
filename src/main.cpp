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
        // std::this_thread::sleep_for(std::chrono::seconds(5));
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
    ThreadPool thread_pool;
    thread_pool.start(4);
    Result res1 = thread_pool.submitTask(std::make_shared<Mytask>(1, 100000000));
    Result res2 = thread_pool.submitTask(std::make_shared<Mytask>(100000001, 200000000));
    Result res3 = thread_pool.submitTask(std::make_shared<Mytask>(200000001, 300000000));
    // // get返回了一个Any类型，怎么转换成具体的类型呢？
    size_t sum1 = res1.get().cast_<uLong>();
    size_t sum2 = res2.get().cast_<uLong>();
    size_t sum3 = res3.get().cast_<uLong>();
    std::cout << "sum:" << (sum1 + sum2 + sum3) << std::endl;
    // getchar();
}