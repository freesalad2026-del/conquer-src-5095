#pragma once

// Wire-compatible implementation of AccServer's legacy Conquer 5095 auth protocol.
// All multi-byte packet fields are little-endian, independently of host architecture.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

namespace coauth {
inline std::uint16_t read16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (static_cast<unsigned>(p[1]) << 8));
}
inline std::uint32_t read32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}
inline void write16(std::uint8_t* p, std::uint16_t value) {
    p[0] = static_cast<std::uint8_t>(value);
    p[1] = static_cast<std::uint8_t>(value >> 8);
}
inline void write32(std::uint8_t* p, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(value >> (i * 8));
}
inline std::uint32_t rol(std::uint32_t v, std::uint32_t bits) {
    bits &= 31u;
    return bits ? ((v << bits) | (v >> (32u - bits))) : v;
}
inline std::uint32_t ror(std::uint32_t v, std::uint32_t bits) {
    bits &= 31u;
    return bits ? ((v >> bits) | (v << (32u - bits))) : v;
}

class AuthCipher {
public:
    AuthCipher() {
        std::uint8_t a = 0x9D, b = 0x62;
        for (std::size_t i = 0; i < 256; ++i) {
            key1_[i] = a;
            key2_[i] = b;
            a = static_cast<std::uint8_t>((0x0F + static_cast<std::uint8_t>(a * 0xFA)) * a + 0x13);
            b = static_cast<std::uint8_t>((0x79 - static_cast<std::uint8_t>(b * 0x5C)) * b + 0x6D);
        }
    }
    // The legacy C# source applies this exact transform in both of its methods.
    void decrypt(std::uint8_t* data, std::size_t size) { transform(data, size, decrypt_counter_); }
    void encrypt(std::uint8_t* data, std::size_t size) { transform(data, size, encrypt_counter_); }
private:
    void transform(std::uint8_t* data, std::size_t size, std::uint16_t& counter) {
        for (std::size_t i = 0; i < size; ++i) {
            auto c = static_cast<std::uint8_t>(data[i] ^ 0xABu);
            c = static_cast<std::uint8_t>((c >> 4u) | (c << 4u));
            data[i] = static_cast<std::uint8_t>(
                c ^ key1_[counter & 0xFFu] ^ key2_[counter >> 8u]);
            ++counter;
        }
    }
    std::array<std::uint8_t, 256> key1_{}, key2_{};
    std::uint16_t decrypt_counter_ = 0, encrypt_counter_ = 0;
};

class PasswordRc5 {
public:
    PasswordRc5() {
        constexpr std::array<std::uint8_t, 16> key = {
            0x3C, 0xDC, 0xFE, 0xE8, 0xC4, 0x54, 0xD6, 0x7E,
            0x16, 0xA6, 0xF8, 0x1A, 0xE8, 0xD0, 0x38, 0xBE
        };
        std::array<std::uint32_t, 4> words{};
        for (std::size_t i = 0; i < 4; ++i) words[i] = read32(key.data() + i * 4);
        subkeys_[0] = 0xB7E15163u;
        for (std::size_t i = 1; i < subkeys_.size(); ++i)
            subkeys_[i] = subkeys_[i - 1] + 0x9E3779B9u;
        std::uint32_t a = 0, b = 0;
        std::size_t i = 0, j = 0;
        for (int n = 0; n < 78; ++n) {
            a = subkeys_[i] = rol(subkeys_[i] + a + b, 3);
            b = words[j] = rol(words[j] + a + b, a + b);
            i = (i + 1) % subkeys_.size();
            j = (j + 1) % words.size();
        }
    }
    void decrypt(std::uint8_t* data) const {
        for (std::size_t offset = 0; offset < 16; offset += 8) {
            std::uint32_t a = read32(data + offset);
            std::uint32_t b = read32(data + offset + 4);
            for (int i = 12; i >= 1; --i) {
                b = ror(b - subkeys_[i * 2 + 1], a) ^ a;
                a = ror(a - subkeys_[i * 2], b) ^ b;
            }
            write32(data + offset, a - subkeys_[0]);
            write32(data + offset + 4, b - subkeys_[1]);
        }
    }
private:
    std::array<std::uint32_t, 26> subkeys_{};
};

struct Login {
    std::string username;
    std::string password;
    std::string requested_world;
};
inline std::string field(const std::uint8_t* data, std::size_t size) {
    std::string out;
    out.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        if (data[i] != 0) out.push_back(static_cast<char>(data[i]));
    }
    return out;
}
inline std::optional<Login> parse_login(const std::uint8_t* data, std::size_t size) {
    // 276 bytes, packet 1086, account[4..19], RC5 password[132..147],
    // world[260..275]. The original C# server also overrides world to CoPrivate.
    if (size != 276 || read16(data) != 276 || read16(data + 2) != 1086)
        return std::nullopt;
    Login login;
    login.username = field(data + 4, 16);
    login.requested_world = field(data + 260, 16);
    std::array<std::uint8_t, 16> password{};
    std::memcpy(password.data(), data + 132, password.size());
    PasswordRc5{}.decrypt(password.data());
    login.password = field(password.data(), password.size());
    if (login.username.empty() || login.password.empty()) return std::nullopt;
    // SQL uses bound parameters; reject binary/control characters as malformed.
    auto printable = [](const std::string& value) {
        return std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return c >= 0x20 && c <= 0x7E;
        });
    };
    if (!printable(login.username) || !printable(login.password)) return std::nullopt;
    return login;
}

enum class ForwardCode : std::uint32_t {
    InvalidInfo = 1, Banned = 25, ServersNotConfigured = 59
};
inline std::array<std::uint8_t, 32> make_forward(
    std::uint32_t identifier, std::uint32_t state_or_error,
    const std::string& ip = {}, std::uint16_t port = 0) {
    std::array<std::uint8_t, 32> packet{};
    write16(packet.data(), 32);
    write16(packet.data() + 2, 1055);
    write32(packet.data() + 4, identifier);
    // On success, offset 8 holds account State; on failure it holds ForwardCode.
    write32(packet.data() + 8, state_or_error);
    std::memcpy(packet.data() + 12, ip.data(), std::min<std::size_t>(ip.size(), 16));
    write16(packet.data() + 28, port);
    return packet;
}
} // namespace coauth
