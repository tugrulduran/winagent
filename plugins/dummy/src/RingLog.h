#pragma once
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <mutex>
#include <vector>

namespace dummy {

    class RingLog {
    public:
        explicit RingLog(size_t capacity = 64);

        void setCapacity(size_t cap);
        size_t capacity() const;

        void push(const QString& level, const QString& msg, const QJsonObject& fields = {});
        void clear();

        // dump all entries as JSON array
        QJsonArray dump() const;

        // small summary (count, capacity)
        QJsonObject stats() const;

    private:
        struct Entry {
            QString level;
            QString msg;
            QJsonObject fields;
        };

        mutable std::mutex mu_;
        std::vector<Entry> buf_;
        size_t cap_ = 64;
        size_t head_ = 0;
        size_t size_ = 0;
    };

} // namespace dummy
