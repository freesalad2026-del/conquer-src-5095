#ifdef NDEBUG
#undef NDEBUG // Keep runtime assertions enabled in Release/CI protocol tests.
#endif
#include "protocol.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

int main() {
    // Fixed RC5-12 ciphertext of ASCII "123" + thirteen zero bytes, using
    // the original AccServer/Network/CORC5.cs constant 16-byte key.
    std::array<std::uint8_t, 16> ciphertext{{
        0xA1,0x90,0x55,0x70,0x4E,0x8B,0xE9,0x81,
        0x0D,0xA6,0x08,0x4D,0xF3,0x67,0xEB,0x73
    }};
    auto password = ciphertext;
    coauth::PasswordRc5{}.decrypt(password.data());
    assert(password[0] == '1' && password[1] == '2' && password[2] == '3');
    for (std::size_t i = 3; i < password.size(); ++i) assert(password[i] == 0);

    std::array<std::uint8_t, 276> packet{};
    coauth::write16(packet.data(), 276);
    coauth::write16(packet.data() + 2, 1086);
    std::memcpy(packet.data() + 4, "player", 6);
    std::memcpy(packet.data() + 132, ciphertext.data(), ciphertext.size());
    std::memcpy(packet.data() + 260, "CoPrivate", 9);
    const auto login = coauth::parse_login(packet.data(), packet.size());
    assert(login.has_value());
    assert(login->username == "player");
    assert(login->password == "123");
    assert(login->requested_world == "CoPrivate");
    assert(!coauth::parse_login(packet.data(), packet.size() - 1));
    coauth::write16(packet.data() + 2, 999);
    assert(!coauth::parse_login(packet.data(), packet.size()));

    auto ok = coauth::make_forward(1000716, 2, "127.0.0.1", 5816);
    assert(coauth::read16(ok.data()) == 32);
    assert(coauth::read16(ok.data() + 2) == 1055);
    assert(coauth::read32(ok.data() + 4) == 1000716);
    assert(coauth::read32(ok.data() + 8) == 2);
    assert(coauth::read16(ok.data() + 28) == 5816);
    assert(std::memcmp(ok.data() + 12, "127.0.0.1", 9) == 0);
    auto denied = coauth::make_forward(0, static_cast<std::uint32_t>(coauth::ForwardCode::Banned));
    assert(coauth::read32(denied.data() + 8) == 25);
    assert(coauth::read16(denied.data() + 28) == 0);

    std::array<std::uint8_t, 8> text{{0x14,0x01,0x1E,0x04,0,0,0,0}};
    const std::array<std::uint8_t, 8> expected{{0x04,0x58,0xBA,0x12,0x09,0x54,0x3F,0x2E}};
    coauth::AuthCipher encryptor;
    encryptor.encrypt(text.data(), 3);
    encryptor.encrypt(text.data() + 3, 5);
    assert(text == expected); // exact original C# AuthCryptography wire transform
    std::cout << "All legacy 5095 authentication protocol tests passed." << std::endl;
}
