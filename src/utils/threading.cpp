#include "utils/threading.h"
#include <iostream>     // For placeholder output
#include <vector>
#include <algorithm>    // For std::all_of, std::find_if
#include <chrono>       // For sleep_for

// --- ReadWriteLock Implementation ---
ReadWriteLock::ReadWriteLock() : active_readers_(0), waiting_writers_(0), writer_active_(false), writer_thread_id_(), recursive_write_count_(0) {
    // std::cout << "ReadWriteLock created." << std::endl;
}

ReadWriteLock::~ReadWriteLock() {
    // std::cout << "ReadWriteLock destroyed." << std::endl;
    // In a real scenario, might check if lock is held, but that's complex.
}

void ReadWriteLock::readLock() {
    std::unique_lock<std::mutex> lock(mutex_);
    // Wait if there's an active writer or writers are waiting (to prevent writer starvation)
    reader_cv_.wait(lock, [this] { return !writer_active_ && waiting_writers_ == 0; });
    active_readers_++;
}

void ReadWriteLock::readUnlock() {
    std::unique_lock<std::mutex> lock(mutex_);
    active_readers_--;
    if (active_readers_ == 0 && waiting_writers_ > 0) {
        // If I was the last reader and writers are waiting, notify one writer.
        writer_cv_.notify_one();
    }
    // If no readers and no waiting writers, but a writer was active and just released,
    // it would have notified readers. If no writers are waiting, readers can proceed.
    // If waiting_writers_ == 0, then reader_cv_.notify_all() could be called if desired,
    // but readLock condition check handles it.
}

bool ReadWriteLock::tryReadLock() {
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    if (lock.owns_lock() && !writer_active_ && waiting_writers_ == 0) {
        active_readers_++;
        return true;
    }
    return false;
}

void ReadWriteLock::writeLock() {
    std::thread::id current_tid = std::this_thread::get_id();
    std::unique_lock<std::mutex> lock(mutex_);

    if (writer_active_ && writer_thread_id_ == current_tid) {
        // Recursive call by the same thread
        recursive_write_count_++;
        return;
    }

    // Wait if there are active readers or another writer
    waiting_writers_++;
    writer_cv_.wait(lock, [this] { 
        return !writer_active_ && active_readers_ == 0;
    });
    waiting_writers_--;
    writer_active_ = true;
    writer_thread_id_ = current_tid;
    recursive_write_count_ = 1;
}

void ReadWriteLock::writeUnlock() {
    std::unique_lock<std::mutex> lock(mutex_);

    if (!writer_active_ || writer_thread_id_ != std::this_thread::get_id()) {
        // Not locked by this writer or not locked at all.
        // This case should ideally not happen if lock/unlock are used correctly.
        // Consider logging an error or throwing an exception for debugging.
        // For example:
        // if (writer_active_) {
        //     std::cerr << "Error: writeUnlock called by a different thread than the one holding the lock." << std::endl;
        // } else {
        //     std::cerr << "Error: writeUnlock called when lock not writer_active." << std::endl;
        // }
        return; 
    }

    recursive_write_count_--;
    if (recursive_write_count_ == 0) {
        writer_active_ = false;
        writer_thread_id_ = std::thread::id(); // Reset thread ID

        // Original notification logic: Prefer writers, then readers.
        if (waiting_writers_ > 0) {
            writer_cv_.notify_one();
        } else {
            reader_cv_.notify_all();
        }
    }
    // If recursive_write_count_ > 0, the lock is still held by this writer.
}

bool ReadWriteLock::tryWriteLock() {
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    if (lock.owns_lock() && active_readers_ == 0 && !writer_active_) {
        // No waiting_writers_ increment here as it's try_lock; if it fails, we didn't wait.
        writer_active_ = true;
        return true;
    }
    return false;
}


// --- RcuUtils Implementation ---
namespace RcuUtils {

// Thread-local epoch for when a reader started.
// 0 means not in RCU read-side critical section or uses current global epoch.
thread_local uint64_t rcu_thread_epoch = 0; 

// Global epoch counter. Incremented by writers.
std::atomic<uint64_t> rcu_global_epoch(1); // Starts at 1, 0 can mean "not set"

// For simplified grace period tracking:
// Store epochs of threads currently in read-side critical sections.
// This is NOT a scalable or performant way to do RCU, just for skeleton illustration.
// A real RCU uses per-CPU data, quiescent state detection, etc.
static std::mutex rcu_state_mutex; // Protects active_reader_epochs and deferred_callbacks
static std::vector<uint64_t> active_reader_epochs; // Epochs of currently active readers
static std::vector<std::function<void()>> deferred_callbacks;

// (rcuReadLock and rcuReadUnlock are inline in header)

void synchronizeRcu(bool force_full_wait) {
    std::cout << "RCU: Starting synchronizeRcu (grace period)..." << std::endl;
    uint64_t target_epoch = rcu_global_epoch.fetch_add(1, std::memory_order_acq_rel) + 1;
    // The new global epoch is now target_epoch.
    // We need to wait for all readers that started before or during target_epoch-1 to finish.

    // This is a highly simplified and potentially slow way to wait for grace period.
    // It simulates waiting for all threads to pass through a quiescent state.
    // A real implementation would involve checking per-CPU flags or more advanced quiescent state detection.
    
    // Heuristic: Yield a few times, then potentially sleep if force_full_wait is true.
    // This is not robust.
    for (int i = 0; i < 10; ++i) { // Try a few yields
        std::this_thread::yield();
    }

    if (force_full_wait) {
        // Artificial delay to simulate waiting for readers.
        // In a real system, this loop would check actual quiescent states of all relevant CPUs/threads.
        // The condition would be: "all threads that were active at epoch E < target_epoch have
        // indicated they are done with epoch E".
        // For this skeleton, we just simulate a wait.
        std::cout << "RCU: Simulating longer wait for grace period (force_full_wait=true)." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Arbitrary small delay
    }
    
    // After waiting, process any callbacks that were deferred until this epoch.
    // In a more complex system, callbacks are associated with specific epochs.
    // Here, we just process all pending ones after any sync.
    processRcuCallbacks();

    std::cout << "RCU: synchronizeRcu finished for epoch transition to " << target_epoch << "." << std::endl;
}


void callRcu(std::function<void()> callback) {
    if (!callback) return;
    std::lock_guard<std::mutex> lock(rcu_state_mutex);
    deferred_callbacks.push_back(std::move(callback));
    std::cout << "RCU: Queued a callback. Total pending: " << deferred_callbacks.size() << std::endl;
}

void processRcuCallbacks() {
    std::vector<std::function<void()>> callbacks_to_run;
    {
        std::lock_guard<std::mutex> lock(rcu_state_mutex);
        if (deferred_callbacks.empty()) {
            return;
        }
        callbacks_to_run.swap(deferred_callbacks);
    }

    if (!callbacks_to_run.empty()) {
        std::cout << "RCU: Processing " << callbacks_to_run.size() << " deferred callbacks..." << std::endl;
        for (const auto& cb : callbacks_to_run) {
            if (cb) {
                try {
                    cb();
                } catch (const std::exception& e) {
                    std::cerr << "RCU: Exception in deferred callback: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "RCU: Unknown exception in deferred callback." << std::endl;
                }
            }
        }
        std::cout << "RCU: Finished processing callbacks." << std::endl;
    }
}


// --- SimpleThreadPool Implementation ---
SimpleThreadPool::SimpleThreadPool(size_t num_threads) : stop_(false) {
    size_t threads_to_create = num_threads == 0 ? std::thread::hardware_concurrency() : num_threads;
    if (threads_to_create == 0) threads_to_create = 1; // At least one thread

    std::cout << "SimpleThreadPool: Creating " << threads_to_create << " worker threads." << std::endl;
    workers_.reserve(threads_to_create);
    for (size_t i = 0; i < threads_to_create; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex_);
                    this->condition_.wait(lock, [this] { return this->stop_ || !this->tasks_.empty(); });
                    if (this->stop_ && this->tasks_.empty()) {
                        return; // Exit worker thread
                    }
                    if (!this->tasks_.empty()) { // Check if tasks is empty before moving
                        task = std::move(this->tasks_.front());
                        this->tasks_.erase(this->tasks_.begin()); // Simple vector erase
                    }
                }
                if(task) {
                    try {
                        task();
                    } catch (const std::exception& e) {
                        std::cerr << "ThreadPool: Exception in task: " << e.what() << std::endl;
                    } catch (...) {
                        std::cerr << "ThreadPool: Unknown exception in task." << std::endl;
                    }
                }
            }
        });
    }
}

SimpleThreadPool::~SimpleThreadPool() {
    std::cout << "SimpleThreadPool: Destructor called. Stopping threads." << std::endl;
    stop();
}

template<class F>
void SimpleThreadPool::enqueue(F&& f) {
    if (stop_) {
        // std::cout << "SimpleThreadPool: Enqueue on stopped pool. Ignoring." << std::endl;
        // Or throw: throw std::runtime_error("enqueue on stopped ThreadPool");
        return;
    }
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        tasks_.emplace_back(std::forward<F>(f));
    }
    condition_.notify_one();
}

// Explicit instantiation for common types if needed, or keep in header for full template use.
// For this skeleton, assuming it's used with std::function<void()> or compatible lambdas.
// template void SimpleThreadPool::enqueue<std::function<void()>>(std::function<void()>&& );


void SimpleThreadPool::stop() {
    if(stop_) return; // Already stopping/stopped

    std::cout << "SimpleThreadPool: Stopping threads..." << std::endl;
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    condition_.notify_all();
    for (std::thread &worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    std::cout << "SimpleThreadPool: All threads joined." << std::endl;
}

} // namespace RcuUtils
