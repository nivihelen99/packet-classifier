#include "gtest/gtest.h"
#include "utils/threading.h" // Adjust path as necessary
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <functional> // For std::bind
#include <set> // For checking unique thread IDs

// --- ReadWriteLock Tests ---
TEST(ReadWriteLockTest, SingleThreadWriteLock) {
    ReadWriteLock rwl;
    WriteLockGuard guard(rwl);
    // If we reach here, lock was acquired and released.
    SUCCEED();
}

TEST(ReadWriteLockTest, SingleThreadReadLock) {
    ReadWriteLock rwl;
    ReadLockGuard guard(rwl);
    SUCCEED();
}

TEST(ReadWriteLockTest, RecursiveWriteLock) {
    ReadWriteLock rwl;
    rwl.writeLock();
    // This should not deadlock if write lock is recursive
    EXPECT_NO_THROW(rwl.writeLock()); 
    rwl.writeUnlock(); // First unlock
    rwl.writeUnlock(); // Second unlock
}

TEST(ReadWriteLockTest, TryWriteLock) {
    ReadWriteLock rwl;
    EXPECT_TRUE(rwl.tryWriteLock());
    EXPECT_FALSE(rwl.tryWriteLock()); // Should fail as it's already write-locked by this thread (non-recursive try)
                                      // Note: The custom ReadWriteLock IS recursive for writeLock() but tryWriteLock() is not.
                                      // This behavior is a bit nuanced. Standard std::mutex try_lock would also fail.
                                      // If it were a recursive_mutex, try_lock would succeed.
                                      // The current tryWriteLock doesn't check for current thread ownership.
    rwl.writeUnlock();
}

TEST(ReadWriteLockTest, TryReadLock) {
    ReadWriteLock rwl;
    EXPECT_TRUE(rwl.tryReadLock());
    EXPECT_TRUE(rwl.tryReadLock()); // Multiple read locks by same thread (if supported, usually is)
    rwl.readUnlock();
    rwl.readUnlock();

    // Test tryReadLock when write lock is held
    rwl.writeLock();
    EXPECT_FALSE(rwl.tryReadLock());
    rwl.writeUnlock();
}


TEST(ReadWriteLockTest, MultipleReadersRobust) {
    ReadWriteLock rwl;
    std::atomic<int> inside_critical_section(0);
    std::atomic<int> max_concurrent_readers(0);
    std::vector<std::thread> threads;
    int num_readers = 5;
    std::mutex check_mutex; // To update max_concurrent_readers safely

    for (int i = 0; i < num_readers; ++i) {
        threads.emplace_back([&]() {
            ReadLockGuard guard(rwl);
            int current_inside = inside_critical_section.fetch_add(1) + 1;
            {
                std::lock_guard<std::mutex> lk(check_mutex);
                if (current_inside > max_concurrent_readers.load(std::memory_order_relaxed)) { // relaxed ok for this non-critical max update
                    max_concurrent_readers.store(current_inside, std::memory_order_relaxed);
                }
            }
            // Stagger sleep slightly to give more opportunity for overlap,
            // but also to ensure not all threads hit the peak check at the exact same microsecond.
            // Increased base sleep time significantly
            std::this_thread::sleep_for(std::chrono::milliseconds(150 + i * 10)); 
            inside_critical_section.fetch_sub(1);
        });
    }
    for (auto& t : threads) t.join();
    EXPECT_EQ(max_concurrent_readers.load(), num_readers);
    EXPECT_EQ(inside_critical_section.load(), 0);
}

TEST(ReadWriteLockTest, WriterExcludesReaders) {
    ReadWriteLock rwl;
    std::atomic<bool> writer_finished(false);
    std::atomic<bool> reader_started_during_write(false);

    std::thread writer_thread([&]() {
        WriteLockGuard guard(rwl);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        writer_finished = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Ensure writer gets lock first

    std::thread reader_thread([&]() {
        ReadLockGuard guard(rwl);
        if (!writer_finished) {
            reader_started_during_write = true;
        }
    });

    writer_thread.join();
    reader_thread.join();
    EXPECT_FALSE(reader_started_during_write);
}

TEST(ReadWriteLockTest, ReaderExcludesWriter) {
    ReadWriteLock rwl;
    std::atomic<bool> reader_finished(false);
    std::atomic<bool> writer_started_during_read(false);

    std::thread reader_thread([&]() {
        ReadLockGuard guard(rwl);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        reader_finished = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Ensure reader gets lock

    std::thread writer_thread([&]() {
        WriteLockGuard guard(rwl);
        if (!reader_finished) {
            writer_started_during_read = true;
        }
    });
    
    reader_thread.join();
    writer_thread.join();
    EXPECT_FALSE(writer_started_during_read);
}

TEST(ReadWriteLockTest, WriterExcludesWriter) {
    ReadWriteLock rwl;
    std::atomic<int> writers_active(0);
    std::atomic<bool> race_condition(false);
    int num_writers = 2;
    std::vector<std::thread> threads;

    for(int i=0; i < num_writers; ++i) {
        threads.emplace_back([&](){
            WriteLockGuard guard(rwl);
            int current_writers = writers_active.fetch_add(1) + 1;
            if (current_writers > 1) {
                race_condition = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            writers_active.fetch_sub(1);
        });
    }
    for(auto& t : threads) t.join();
    EXPECT_FALSE(race_condition);
}

// --- SimpleThreadPool Tests ---
TEST(SimpleThreadPoolTest, CreateAndDestroy) {
    RcuUtils::SimpleThreadPool pool(2);
    SUCCEED(); // Constructor and destructor run
}

TEST(SimpleThreadPoolTest, EnqueueAndExecuteTask) {
    RcuUtils::SimpleThreadPool pool(1);
    std::atomic<bool> task_executed(false);
    
    pool.enqueue([&task_executed]() {
        task_executed = true;
    });

    // Wait for task to execute - pool destructor will join threads
    // Or add a condition variable / future for more robust check
    pool.stop(); // Ensure tasks are processed
    EXPECT_TRUE(task_executed.load());
}

TEST(SimpleThreadPoolTest, MultipleTasksOnMultipleThreads) {
    const int num_threads = 4;
    const int num_tasks = 20;
    RcuUtils::SimpleThreadPool pool(num_threads);
    std::atomic<int> tasks_completed(0);
    std::mutex id_mutex;
    std::set<std::thread::id> thread_ids;

    for (int i = 0; i < num_tasks; ++i) {
        pool.enqueue([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simulate work
            {
                std::lock_guard<std::mutex> lock(id_mutex);
                thread_ids.insert(std::this_thread::get_id());
            }
            tasks_completed++;
        });
    }

    pool.stop();
    EXPECT_EQ(tasks_completed.load(), num_tasks);
    // Check if more than one thread was used (likely, but not strictly guaranteed for very short tasks)
    // For this test, with sleep, it's very likely.
    if (num_threads > 1 && num_tasks > num_threads) {
         EXPECT_GT(thread_ids.size(), 1);
         EXPECT_LE(thread_ids.size(), num_threads);
    } else if (num_threads == 1) {
         EXPECT_EQ(thread_ids.size(), 1);
    }
    std::cout << "Tasks executed on " << thread_ids.size() << " threads." << std::endl;
}

TEST(SimpleThreadPoolTest, StopPool) {
    RcuUtils::SimpleThreadPool pool(2);
    std::atomic<int> counter(0);
    pool.enqueue([&]() { counter++; std::this_thread::sleep_for(std::chrono::milliseconds(50)); });
    pool.enqueue([&]() { counter++; std::this_thread::sleep_for(std::chrono::milliseconds(50)); });
    
    pool.stop(); // Should wait for tasks to complete
    EXPECT_EQ(counter.load(), 2);

    // Test enqueue on stopped pool (should ideally not crash, might no-op or throw)
    // The current implementation returns if stop_ is true.
    EXPECT_NO_THROW(pool.enqueue([&](){ counter++; /* This should not run */ }));
    EXPECT_EQ(counter.load(), 2); // Counter should not have incremented
}


// --- RCU Utils (Simplified) Tests ---
TEST(RcuUtilsTest, CallRcuAndProcessCallbacks) {
    // RCU utils are global / static within namespace, so state persists.
    // This test is very basic due to simplified RCU.
    std::atomic<int> callback_count(0);
    
    RcuUtils::callRcu([&]() {
        callback_count++;
    });
    RcuUtils::callRcu([&]() {
        callback_count++;
    });

    // In this simplified RCU, synchronizeRcu also processes callbacks.
    RcuUtils::synchronizeRcu(); 
    EXPECT_EQ(callback_count.load(), 2);

    // Check if callbacks are cleared
    callback_count = 0;
    RcuUtils::processRcuCallbacks(); // Should do nothing as they were processed
    EXPECT_EQ(callback_count.load(), 0);

    RcuUtils::callRcu([&]() {
        callback_count++;
    });
    RcuUtils::processRcuCallbacks(); // Process without full synchronize
    EXPECT_EQ(callback_count.load(), 1);
}

TEST(RcuUtilsTest, SynchronizeRcuEpochIncrement) {
    uint64_t epoch_before = RcuUtils::rcu_global_epoch.load();
    RcuUtils::synchronizeRcu();
    uint64_t epoch_after = RcuUtils::rcu_global_epoch.load();
    EXPECT_EQ(epoch_after, epoch_before + 1);
}


// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }
