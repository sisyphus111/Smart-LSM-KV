#pragma once
#include <thread>          // 线程支持
#include <future>          // 异步操作和future支持
#include <functional>      // 函数对象支持
#include <queue>           // 队列容器
#include <vector>          // 向量容器
#include <mutex>           // 互斥锁支持
#include <condition_variable>  // 条件变量支持
#include <stdexcept>       // 标准异常类
#include <utility>         // 通用工具函数

/**
 * @brief 线程池类，用于管理多个工作线程并执行任务队列中的任务
 */
class ThreadPool {
public:
    /**
     * @brief 线程池构造函数
     * @param num_threads 要创建的工作线程数量
     */
    ThreadPool(size_t num_threads) : stop(false) {
        // 创建指定数量的工作线程
        for (size_t i = 0; i < num_threads; ++i) {
            // 使用emplace_back在workers向量中直接构造线程对象
            workers.emplace_back([this] {
                // 工作线程的主循环，每个线程都会执行这个lambda函数
                while (true) {
                    // 声明一个函数对象用于存储从队列中取出的任务
                    std::function<void()> task;
                    
                    // 临界区开始：使用大括号限制锁的作用域
                    {
                        // 获取队列互斥锁，保护任务队列的访问
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        
                        // 等待条件：线程池停止标志为true 或 任务队列不为空
                        this->condition.wait(
                            lock, [this] { return this->stop || !this->tasks.empty(); });
                        
                        // 如果线程池已停止且任务队列为空，则退出线程
                        if (this->stop && this->tasks.empty()) {
                            return;
                        }
                        
                        // 从任务队列前端取出一个任务
                        task = std::move(this->tasks.front());
                        // 从队列中移除已取出的任务
                        this->tasks.pop();
                    }
                    // 临界区结束：锁被自动释放
                    
                    // 执行取出的任务（在锁外执行，避免阻塞其他线程）
                    task();
                }
            });
        }
    }

    /**
     * @brief 向线程池提交一个任务
     * @tparam F 函数类型
     * @tparam Args 函数参数类型包
     * @param f 要执行的函数
     * @param args 函数参数
     * @return std::future对象，可用于获取任务执行结果
     */
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> 
    // "->"表示尾置返回类型，函数返回类型为std::future，使用了C++17的特性
    // 该函数接受一个可调用对象和任意数量的参数，并返回一个future对象
    {
        // 使用std::invoke_result推导函数返回类型
        // typename表示这是一个类型别名，std::invoke_result<F, Args...>::type表示函数f在Args...参数下的返回类型。std::invoke_result是一个模板类，必须使用typename来指明它的类型成员
        using return_type = typename std::invoke_result<F, Args...>::type;

        
        // std::packaged_task将函数和参数封装成一个可调用对象，并提供一个future来获取结果
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            // 使用std::bind将函数和参数绑定到packaged_task中
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
            
        // 获取与packaged_task关联的future对象
        std::future<return_type> res = task->get_future();
        
        // 临界区开始：保护任务队列的修改
        {
            // 获取队列互斥锁
            std::unique_lock<std::mutex> lock(queue_mutex);
            
            // 检查线程池是否已停止，如果已停止则不能再提交任务
            if(stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            
            // 将任务包装成lambda函数并添加到任务队列
            // lambda函数捕获task并在执行时调用packaged_task
            tasks.emplace([task](){ (*task)(); });
        }
        // 临界区结束：锁被自动释放
        
        // 通知一个等待的工作线程有新任务可执行
        condition.notify_one();
        
        // 返回future对象，调用者可以通过它获取任务执行结果
        return res;
    }

    /**
     * @brief 线程池析构函数，负责清理资源并等待所有线程结束
     */
    ~ThreadPool() {
        // 临界区开始：设置停止标志
        {
            // 获取队列互斥锁
            std::unique_lock<std::mutex> lock(queue_mutex);
            // 设置停止标志为true，告知所有工作线程停止工作
            stop = true;
        }
        // 临界区结束：锁被自动释放
        
        // 通知所有等待的工作线程检查停止条件
        condition.notify_all();
        
        // 等待所有工作线程完成当前任务并退出
        for (std::thread &worker : workers) {
            // join()会阻塞直到对应的线程执行完毕
            worker.join();
        }
    }

private:
    std::vector<std::thread> workers;           // 工作线程向量，存储所有工作线程
    std::queue<std::function<void()>> tasks;    // 任务队列，存储待执行的任务
    std::mutex queue_mutex;                     // 互斥锁，保护任务队列的并发访问
    std::condition_variable condition;          // 条件变量，用于线程间的同步通信
    bool stop;                                  // 停止标志，指示线程池是否应该停止工作
};