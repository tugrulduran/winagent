#pragma once
#include <QString>
#include <QJsonObject>
#include <atomic>
#include <cstdint>

namespace dummy {

    // Lightweight deterministic RNG (xorshift64*)
    class XorShift64 {
    public:
        explicit XorShift64(uint64_t seed = 1) : s_(seed ? seed : 1) {}
        uint64_t next();
        double next01(); // [0,1)
        void reseed(uint64_t seed);

    private:
        uint64_t s_;
    };

    struct Sample {
        double value = 0.0;    // main metric
        double jitter = 0.0;   // noise component
        double phase = 0.0;    // [0..1)
        uint64_t seq = 0;      // tick counter
    };

    class DummySampler {
    public:
        bool init(QString& err);

        void configure(double noise, uint64_t seed);
        void reseed(uint64_t seed);

        Sample sample(); // advances internal state

        double noise() const { return noise_; }
        uint64_t seed() const { return seed_; }

    private:
        double noise_ = 0.25;
        uint64_t seed_ = 1337;
        XorShift64 rng_{1337};
        uint64_t seq_ = 0;
        double phase_ = 0.0;
    };

} // namespace dummy
