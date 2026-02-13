#include <gtest/gtest.h>
#include "DummyMonitor.h"
#include "ExceptionMonitor.h"
#include "AdvancedTests.h"

TEST(BaseMonitorTest, StartStopStartWorks) {
    DummyMonitor monitor;

    monitor.start();
    monitor.stop();
    monitor.start();
    monitor.stop();

    SUCCEED();
}

TEST(BaseMonitorTest, StopWithoutStartIsNoOp) {
    DummyMonitor monitor;

    // Hiç start edilmedi
    monitor.stop();

    SUCCEED();
}

TEST(BaseMonitorTest, DestructorStopsThread) {
    {
        DummyMonitor monitor;
        monitor.start();

        // burada deliberately stop() çağırmıyoruz
        // destructor cleanup test ediliyor
    }

    SUCCEED();
}

TEST(BaseMonitorTest, StartStopStressTest) {
    DummyMonitor monitor;

    for (int i = 0; i < 100; ++i) {
        monitor.start();
        monitor.stop();
    }

    SUCCEED();
}

TEST(BaseMonitorEdgeTest, InitThrowResetsStateToStopped)
{
    InitThrowMonitor m;

    EXPECT_THROW(m.start(), std::runtime_error);

    EXPECT_EQ(m.state.load(), ThreadState::Stopped);

    EXPECT_THROW(m.start(), std::runtime_error);
}

TEST(BaseMonitorEdgeTest, StopWhileStarting)
{
    SlowInitMonitor m;

    std::thread t([&]() {
        m.start();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    m.stop();

    t.join();

    EXPECT_EQ(m.state.load(), ThreadState::Stopped);
}

TEST(BaseMonitorEdgeTest, ConcurrentStartCalls)
{
    SlowInitMonitor m;

    std::thread t1([&]() { m.start(); });
    std::thread t2([&]() { m.start(); });

    t1.join();
    t2.join();

    EXPECT_EQ(m.state.load(), ThreadState::Running);

    m.stop();
}

TEST(BaseMonitorStressTest, StartStopHammer)
{
    SlowInitMonitor m;

    for (int i = 0; i < 50; ++i) {
        std::thread t1([&]() { m.start(); });
        std::thread t2([&]() { m.stop(); });

        t1.join();
        t2.join();
    }

    m.stop();

    EXPECT_EQ(m.state.load(), ThreadState::Stopped);
}

TEST(BaseMonitorEdgeTest, DestructorAfterInitThrow)
{
    try {
        InitThrowMonitor m;
        m.start();
    }
    catch (...) {
        SUCCEED();
    }
}

TEST(BaseMonitorBehaviorTest, StopActuallyStopsThread)
{
    CounterMonitor m;

    m.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    m.stop();

    int valueAfterStop = m.counter.load();

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    EXPECT_EQ(valueAfterStop, m.counter.load());
}

TEST(BaseMonitorStressTest, RapidRestart)
{
    CounterMonitor m;

    for (int i = 0; i < 20; ++i) {
        m.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        m.stop();
    }

    EXPECT_EQ(m.state.load(), ThreadState::Stopped);
}

TEST(BaseMonitorEdgeTest, UpdateThrowDoesNotDeadlock)
{
    UpdateThrowMonitor m;

    m.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    m.stop();

    EXPECT_EQ(m.state.load(), ThreadState::Stopped);
}

TEST(BaseMonitorStressTest, ConcurrencyFuzzer)
{
    SlowInitMonitor m;

    std::vector<std::thread> threads;

    for (int i = 0; i < 20; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 10; ++j) {
                m.start();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                m.stop();
            }
        });
    }

    for (auto& t : threads)
        t.join();

    EXPECT_EQ(m.state.load(), ThreadState::Stopped);
}

TEST(BaseMonitorEdgeTest, StopDuringVerySlowUpdate)
{
    VerySlowUpdateMonitor m;

    m.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto start = std::chrono::steady_clock::now();
    m.stop();
    auto end = std::chrono::steady_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Stop 1 saniye sürmemeli
    EXPECT_LT(duration, 1000);
    EXPECT_EQ(m.state.load(), ThreadState::Stopped);
}

TEST(BaseMonitorStressTest, MassiveConcurrentStopCalls)
{
    CounterMonitor m;
    m.start();

    std::vector<std::thread> threads;

    for (int i = 0; i < 20; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 50; ++j) {
                m.stop();
            }
        });
    }

    for (auto& t : threads)
        t.join();

    EXPECT_EQ(m.state.load(), ThreadState::Stopped);
}

TEST(BaseMonitorBehaviorTest, StateNeverCorrupt)
{
    CounterMonitor m;

    for (int i = 0; i < 30; ++i) {
        m.start();

        ThreadState s = m.state.load();
        EXPECT_TRUE(
            s == ThreadState::Running ||
            s == ThreadState::Starting
        );

        m.stop();

        EXPECT_EQ(m.state.load(), ThreadState::Stopped);
    }
}

TEST(BaseMonitorEdgeTest, DestructorWhileRunningUnderLoad)
{
    {
        CounterMonitor m;
        m.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    SUCCEED();
}

TEST(BaseMonitorStressTest, MediumDurationFuzzer)
{
    SlowInitMonitor m;

    for (int i = 0; i < 100; ++i) {
        m.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        m.stop();
    }

    EXPECT_EQ(m.state.load(), ThreadState::Stopped);
}

TEST(BaseMonitorTest, UpdateThrowDoesNotStopMonitor)
{
    class ThrowingMonitor : public BaseMonitor {
    public:
        ThrowingMonitor()
            : BaseMonitor(10) {}

        std::atomic<int> counter{0};

        void init() override {
            // test için boş
        }

        void update() override {
            counter++;

            if (counter == 1) {
                throw std::runtime_error("boom");
            }
        }
    };

    ThrowingMonitor m;
    m.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // ❗ Bu test şu an FAIL edecek
    // Çünkü update throw edince run() kırılıyor ve thread ölüyor.
    EXPECT_EQ(m.state.load(), ThreadState::Running);

    EXPECT_GT(m.counter.load(), 1);

    m.stop();
}

TEST(BaseMonitorTest, StartIsIdempotent)
{
    class SimpleMonitor : public BaseMonitor {
    public:
        SimpleMonitor() : BaseMonitor(10) {}

        std::atomic<int> counter{0};

        void init() override {}
        void update() override { counter++; }
    };

    SimpleMonitor m;

    m.start();
    m.start(); // ikinci start

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(m.state.load(), ThreadState::Running);
    EXPECT_GT(m.counter.load(), 1);

    m.stop();
}

TEST(BaseMonitorTest, StopIsIdempotent)
{
    class SimpleMonitor : public BaseMonitor {
    public:
        SimpleMonitor() : BaseMonitor(10) {}
        void init() override {}
        void update() override {}
    };

    SimpleMonitor m;

    m.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    m.stop();
    m.stop(); // ikinci stop

    EXPECT_EQ(m.state.load(), ThreadState::Stopped);
}

TEST(BaseMonitorTest, CanStartAfterStop)
{
    class SimpleMonitor : public BaseMonitor {
    public:
        SimpleMonitor() : BaseMonitor(10) {}
        std::atomic<int> counter{0};

        void init() override {}
        void update() override { counter++; }
    };

    SimpleMonitor m;

    m.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    m.stop();

    auto firstCount = m.counter.load();

    m.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    m.stop();

    EXPECT_GT(m.counter.load(), firstCount);
}

TEST(BaseMonitorTest, StopWakesSleepingThread)
{
    class SlowMonitor : public BaseMonitor {
    public:
        SlowMonitor() : BaseMonitor(1000) {}
        void init() override {}
        void update() override {}
    };

    SlowMonitor m;

    m.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto start = std::chrono::steady_clock::now();
    m.stop();
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_LT(duration, 500); // 1 saniyeyi beklememeli
}
