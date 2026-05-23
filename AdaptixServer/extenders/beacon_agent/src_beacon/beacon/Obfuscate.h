#pragma once

// Compile-time string obfuscation.
// Strings are XOR-encrypted at compile time and decrypted at first access.

namespace obf {

template <unsigned int N, char KEY>
class String {
    mutable char data_[N];
    mutable bool dec_;

public:
    constexpr String(const char (&str)[N]) : data_{}, dec_(false) {
        for (unsigned int i = 0; i < N; ++i)
            data_[i] = str[i] ^ KEY;
    }

    operator const char*() const {
        if (!dec_) {
            for (unsigned int i = 0; i < N; ++i)
                data_[i] ^= KEY;
            dec_ = true;
        }
        return data_;
    }
};

constexpr char keygen(const char* f, int l) {
    return static_cast<char>((f[0] * 7 + l * 13 + 0x5A) & 0xFF) | 1;
}

} // namespace obf

#define OBF(str) (obf::String<sizeof(str), obf::keygen(__FILE__, __LINE__)>(str))
