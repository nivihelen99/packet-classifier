#ifndef THREADING_UTILS_H
#define THREADING_UTILS_H

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread> // For std::this_thread::yield, std::thread::hardware_concurrency
#include <vector>
#include <functional> // For std::function

// --- Basic Read-Write Lock ---
// This is a classic RW lock implementation.
// It allows multiple readers or a single writer.
class ReadWriteLock {
public:
    ReadWriteLock();
    ~ReadWriteLock();

    void readLock();
    void readUnlock();
    bool tryReadLock(); // Non-blocking attempt

    void writeLock();
    void writeUnlock();
    bool tryWriteLock(); // Non-blocking attempt

private:
    std::mutex mutex_;
    std::condition_variable reader_cv_; // Signaled when writer releases or no writers waiting
    std::condition_variable writer_cv_; // Signaled when all readers release

    int active_readers_;    // Number of threads currently holding a read lock
    int waiting_writers_;   // Number of threads waiting for a write lock
    bool writer_active_;    // True if a writer holds the lock
};

// RAII wrapper for ReadLock
class ReadLockGuard {
public:
    explicit ReadLockGuard(ReadWriteLock& rw_lock) : lock_(rw_lock) {
        lock_.readLock();
    }
    ~ReadLockGuard() {
        lock_.readUnlock();
    }
    ReadLockGuard(const ReadLockGuard&) = delete;
    ReadLockGuard& operator=(const ReadLockGuard&) = delete;
private:
    ReadWriteLock& lock_;
};

// RAII wrapper for WriteLock
class WriteLockGuard {
public:
    explicit WriteLockGuard(ReadWriteLock& rw_lock) : lock_(rw_lock) {
        lock_.writeLock();
    }
    ~WriteLockGuard() {
        lock_.writeUnlock();
    }
    WriteLockGuard(const WriteLockGuard&) = delete;
    WriteLockGuard& operator=(const WriteLockGuard&) = delete;
private:
    ReadWriteLock& lock_;
};


// --- RCU (Read-Copy-Update) Utilities ---
// This is a simplified RCU mechanism skeleton.
// A full RCU implementation is highly complex and system-dependent.
// This version will focus on the core concepts:
// - Readers operate without locks on shared data.
// - Writers create copies, modify them, then publish.
// - Grace periods ensure old data is reclaimed safely.

// Global RCU epoch (simplified)
// In a real system, this would be more sophisticated, possibly per-CPU.
namespace RcuUtils {

// Represents a "quiescent state" counter for a thread.
// Each thread participating in RCU should increment its counter
// when it's in a "read-side critical section" (accessing RCU-protected data).
// This is a very simplified model. More robust RCU uses thread registration,
// per-CPU variables, and more complex epoch tracking.
extern thread_local uint64_t rcu_thread_epoch;
extern std::atomic<uint64_t> rcu_global_epoch;

// Call this when a thread enters an RCU read-side critical section.
inline void rcuReadLock() {
    // In this simplified model, we can just note the current global epoch.
    // A real implementation might increment a per-thread counter or mark active.
    rcu_thread_epoch = rcu_global_epoch.load(std::memory_order_acquire);
}

// Call this when a thread exits an RCU read-side critical section.
inline void rcuReadUnlock() {
    // Mark thread as quiescent for the rcu_thread_epoch it was on.
    // In a real system, this signals that the thread is no longer using data from that epoch.
    // For this skeleton, this is mostly a conceptual marker.
}

// Initiates a grace period.
// After this returns, all threads that were in an RCU read-side critical section
// *before* this call are guaranteed to have completed it.
// Any new RCU read-side critical sections will see data from the new epoch.
void synchronizeRcu(bool force_full_wait = false);

// For writers: A function to call after data has been "published" (e.g., atomic pointer swap)
// and before old data can be reclaimed. This function ensures all prior readers are done.
//
// Usage:
//   auto old_data = shared_ptr_to_data.load();
//   auto new_data = make_new_data_based_on(old_data);
//   shared_ptr_to_data.store(new_data); // Publish
//   synchronizeRcu(); // Wait for grace period
//   reclaim(old_data); // Now safe to free/recycle old_data

// Helper for managing "call RCU" callbacks (deferred reclamation)
// This is a common pattern where functions (usually for deallocation) are
// deferred until after a grace period.
void callRcu(std::function<void()> callback);

// Processes the pending RCU callbacks.
// This might be called by a dedicated RCU thread or periodically.
void processRcuCallbacks();

// --- Thread Pool (Simple) ---
// A very basic thread pool for offloading tasks, could be used for RCU callback processing
// or other background work. Not directly part of RCU but often used with it.
class SimpleThreadPool {
public:
    SimpleThreadPool(size_t num_threads = 0); // 0 means hardware_concurrency
    ~SimpleThreadPool();

    template<class F>
    void enqueue(F&& f);

    void stop();

private:
    std::vector<std::thread> workers_;
    std::vector<std::function<void()>> tasks_; // Using vector as a simple queue
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;
};

} // namespace RcuUtils

#endif // THREADING_UTILS_H
