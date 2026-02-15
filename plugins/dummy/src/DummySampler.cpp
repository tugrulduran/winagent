#include "DummySampler.h"
#include <QtGlobal>
#include <cmath>

namespace dummy {

    uint64_t XorShift64::next() {
        uint64_t x = s_;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        s_ = x;
        return x * 2685821657736338717ull;
    }

    double XorShift64::next01() {
        // take top 53 bits for double mantissa
        const uint64_t r = next();
        const uint64_t v = (r >> 11) & ((1ull << 53) - 1);
        return (double)v / (double)(1ull << 53);
    }

    void XorShift64::reseed(uint64_t seed) {
        s_ = seed ? seed : 1;
    }

    bool DummySampler::init(QString& err) {
        Q_UNUSED(err);
        // Nothing external; in real plugin you could open handles here.
        rng_.reseed(seed_);
        seq_ = 0;
        phase_ = 0.0;
        return true;
    }

    void DummySampler::configure(double noise, uint64_t seed) {
        noise_ = qBound(0.0, noise, 10.0);
        seed_ = seed ? seed : 1;
        rng_.reseed(seed_);
    }

    void DummySampler::reseed(uint64_t seed) {
        seed_ = seed ? seed : 1;
        rng_.reseed(seed_);
    }

    Sample DummySampler::sample() {
        seq_++;

        // Phase goes 0..1 repeating
        phase_ += 0.035; // ~28 ticks per full cycle
        if (phase_ >= 1.0) phase_ -= 1.0;

        // Base wave: mix sine + saw-ish
        const double s = std::sin(phase_ * 2.0 * M_PI);
        const double saw = (phase_ * 2.0 - 1.0); // -1..+1
        const double base = 50.0 + 35.0 * s + 15.0 * saw; // around 0..100-ish

        // Jitter around base
        const double r = (rng_.next01() * 2.0 - 1.0); // -1..+1
        const double jitter = r * (noise_ * 10.0);

        Sample out;
        out.value = base + jitter;
        out.jitter = jitter;
        out.phase = phase_;
        out.seq = seq_;
        return out;
    }

} // namespace dummy
