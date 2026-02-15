#include "RingLog.h"

#include <QJsonValue>
#include "Utils.h"

namespace dummy {

    RingLog::RingLog(size_t capacity) : cap_(capacity ? capacity : 1) {
        buf_.resize(cap_);
    }

    void RingLog::setCapacity(size_t cap) {
        if (cap == 0) cap = 1;
        std::lock_guard<std::mutex> g(mu_);
        cap_ = cap;
        buf_.assign(cap_, Entry{});
        head_ = 0;
        size_ = 0;
    }

    size_t RingLog::capacity() const {
        std::lock_guard<std::mutex> g(mu_);
        return cap_;
    }

    void RingLog::push(const QString& level, const QString& msg, const QJsonObject& fields) {
        std::lock_guard<std::mutex> g(mu_);
        if (buf_.size() != cap_) buf_.resize(cap_);

        Entry e;
        e.level = level;
        e.msg = msg;
        e.fields = fields;
        e.fields.insert("ts", isoUtcNow());

        buf_[head_] = std::move(e);
        head_ = (head_ + 1) % cap_;
        if (size_ < cap_) size_++;
    }

    void RingLog::clear() {
        std::lock_guard<std::mutex> g(mu_);
        head_ = 0;
        size_ = 0;
        for (auto& e : buf_) e = Entry{};
    }

    QJsonArray RingLog::dump() const {
        std::lock_guard<std::mutex> g(mu_);
        QJsonArray arr;
        // arr.reserve((int)size_);

        // oldest -> newest
        const size_t start = (head_ + cap_ - size_) % cap_;
        for (size_t i = 0; i < size_; ++i) {
            const Entry& e = buf_[(start + i) % cap_];
            QJsonObject o;
            o.insert("level", e.level);
            o.insert("msg", e.msg);
            // merge fields into o.fields (nest as "fields" is also OK; here flatten not ideal)
            o.insert("fields", e.fields);
            arr.append(o);
        }
        return arr;
    }

    QJsonObject RingLog::stats() const {
        std::lock_guard<std::mutex> g(mu_);
        return QJsonObject{
            {"count", (int)size_},
            {"capacity", (int)cap_}
        };
    }

} // namespace dummy
