#pragma once

#include <cstdint>

namespace twinvq {

class BitReader {
public:
    BitReader(const uint8_t* data, int size_bytes)
        : data_(data), size_bytes_(size_bytes) {}

    int get(int n) {
        unsigned v = 0;
        for (int i = 0; i < n; i++)
            v = (v << 1) | get1();
        return static_cast<int>(v);
    }

    int get1() {
        if (bit_index_ >= size_bytes_ * 8)
            return 0;
        const int byte = data_[bit_index_ >> 3];
        const int bit = 7 - (bit_index_ & 7);
        bit_index_++;
        return (byte >> bit) & 1;
    }

    void skip(int n) { bit_index_ += n; }

    int bits_read() const { return bit_index_; }

private:
    const uint8_t* data_;
    int size_bytes_;
    int bit_index_ = 0;
};

} // namespace twinvq
