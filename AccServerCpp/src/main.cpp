#include "protocol.hpp"

#include <mysql.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#  include <ws2tcpip.h>
using NativeSocket = SOCKET;
constexpr NativeSocket invalid_socket = INVALID_SOCKET;
void close_socket(NativeSocket s) { closesocket(s); }
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <sys/time.h>
#  include <unistd.h>
using NativeSocket = int;
constexpr NativeSocket invalid_socket = -1;
void close_socket(NativeSocket s) { close(s); }
#endif

namespace {
using Clock = std::chrono::steady_clock;

struct SocketHandle {
    NativeSocket value = invalid_socket;
    explicit SocketHandle(NativeSocket socket = invalid_socket) : value(socket) {}
    ~SocketHandle() { if (value != invalid_socket) close_socket(value); }
    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;
};
struct DbConfig {
    std::string host, user, password, name;
    unsigned int port = 3306;
};
struct Settings {
    DbConfig db;
    std::string bind_ip, world;
    unsigned short port = 9958;
};
struct World {
    std::string ip;
    std::uint16_t port = 0;
};
struct Account {
    std::string password;
    std::uint32_t id = 0;
    std::uint32_t state = 0;
};
struct MySqlClose {
    void operator()(MYSQL* db) const { if (db) mysql_close(db); }
};
struct StatementClose {
    void operator()(MYSQL_STMT* stmt) const { if (stmt) mysql_stmt_close(stmt); }
};
using DbPtr = std::unique_ptr<MYSQL, MySqlClose>;
using StmtPtr = std::unique_ptr<MYSQL_STMT, StatementClose>;
std::mutex log_mutex;

void log_line(const std::string& line) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cout << line << std::endl;
}
std::string env_or(const char* name, const char* default_value) {
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string(default_value);
}
unsigned int port_number(const std::string& text) {
    std::size_t consumed = 0;
    const unsigned long port = std::stoul(text, &consumed);
    if (consumed != text.size() || port == 0 || port > 65535)
        throw std::runtime_error("Invalid TCP port: " + text);
    return static_cast<unsigned int>(port);
}
Settings load_settings() {
    Settings s;
    s.db.host = env_or("DB_HOST", "127.0.0.1");
    s.db.port = port_number(env_or("DB_PORT", "3306"));
    s.db.name = env_or("DB_NAME", "zq");
    s.db.user = env_or("DB_USER", "root");
    s.db.password = env_or("DB_PASSWORD", "");
    s.bind_ip = env_or("AUTH_BIND", "0.0.0.0");
    s.port = static_cast<unsigned short>(port_number(env_or("AUTH_PORT", "9958")));
    // Legacy Authentication.Deserialize unconditionally sets Server = "CoPrivate".
    s.world = env_or("AUTH_WORLD", "CoPrivate");
    if (s.world.empty()) throw std::runtime_error("AUTH_WORLD cannot be empty");
    return s;
}
DbPtr open_db(const DbConfig& cfg) {
    DbPtr db(mysql_init(nullptr));
    if (!db) throw std::runtime_error("mysql_init failed");
    unsigned int timeout_seconds = 4;
    mysql_options(db.get(), MYSQL_OPT_CONNECT_TIMEOUT, &timeout_seconds);
    if (!mysql_real_connect(db.get(), cfg.host.c_str(), cfg.user.c_str(),
                            cfg.password.c_str(), cfg.name.c_str(), cfg.port,
                            nullptr, 0))
        throw std::runtime_error("Database connection: " + std::string(mysql_error(db.get())));
    return db;
}
World load_world(MYSQL* db, const std::string& selected) {
    if (mysql_query(db, "SELECT Name, IP, Port FROM servers"))
        throw std::runtime_error("Reading servers: " + std::string(mysql_error(db)));
    std::unique_ptr<MYSQL_RES, decltype(&mysql_free_result)> rows(mysql_store_result(db), &mysql_free_result);
    if (!rows) throw std::runtime_error("Reading server rows: " + std::string(mysql_error(db)));
    while (MYSQL_ROW row = mysql_fetch_row(rows.get())) {
        if (row[0] && row[1] && row[2] && selected == row[0]) {
            const auto port = port_number(row[2]);
            const std::string ip = row[1];
            if (ip.empty() || ip.size() > 16)
                throw std::runtime_error("servers.IP must contain an address of at most 16 bytes");
            return { ip, static_cast<std::uint16_t>(port) };
        }
    }
    throw std::runtime_error("World not configured in servers table: " + selected);
}
std::optional<Account> find_account(MYSQL* db, const std::string& username) {
    StmtPtr stmt(mysql_stmt_init(db));
    if (!stmt) throw std::runtime_error("Cannot initialize account query");
    constexpr char query[] = "SELECT Password, State, EntityID FROM accounts WHERE Username = ? LIMIT 1";
    if (mysql_stmt_prepare(stmt.get(), query, sizeof(query) - 1))
        throw std::runtime_error("Preparing account query: " + std::string(mysql_stmt_error(stmt.get())));
    MYSQL_BIND argument{};
    unsigned long username_len = static_cast<unsigned long>(username.size());
    argument.buffer_type = MYSQL_TYPE_STRING;
    argument.buffer = const_cast<char*>(username.data());
    argument.buffer_length = username_len;
    argument.length = &username_len;
    if (mysql_stmt_bind_param(stmt.get(), &argument) || mysql_stmt_execute(stmt.get()))
        throw std::runtime_error("Account query: " + std::string(mysql_stmt_error(stmt.get())));

    std::array<char, 33> password{};
    std::array<char, 17> state{};
    std::array<char, 33> id{};
    unsigned long lengths[3]{};
    MYSQL_BIND columns[3]{};
    columns[0].buffer_type = MYSQL_TYPE_STRING;
    columns[0].buffer = password.data();
    columns[0].buffer_length = static_cast<unsigned long>(password.size() - 1);
    columns[0].length = &lengths[0];
    columns[1].buffer_type = MYSQL_TYPE_STRING;
    columns[1].buffer = state.data();
    columns[1].buffer_length = static_cast<unsigned long>(state.size() - 1);
    columns[1].length = &lengths[1];
    columns[2].buffer_type = MYSQL_TYPE_STRING;
    columns[2].buffer = id.data();
    columns[2].buffer_length = static_cast<unsigned long>(id.size() - 1);
    columns[2].length = &lengths[2];
    if (mysql_stmt_bind_result(stmt.get(), columns))
        throw std::runtime_error("Binding account columns: " + std::string(mysql_stmt_error(stmt.get())));
    const int result = mysql_stmt_fetch(stmt.get());
    if (result == MYSQL_NO_DATA) return std::nullopt;
    if (result != 0)
        throw std::runtime_error("Fetching account: " + std::string(mysql_stmt_error(stmt.get())));
    Account account;
    account.password.assign(password.data(), lengths[0]);
    const auto state_value = std::stoul(std::string(state.data(), lengths[1]));
    const auto id_value = std::stoull(std::string(id.data(), lengths[2]));
    if (state_value > 255 || id_value == 0 || id_value > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("Account has invalid State or EntityID");
    account.state = static_cast<std::uint32_t>(state_value);
    account.id = static_cast<std::uint32_t>(id_value);
    return account;
}
bool same_password(const std::string& expected, const std::string& given) {
    unsigned int mismatch = static_cast<unsigned int>(expected.size() ^ given.size());
    for (std::size_t i = 0; i < 16; ++i) {
        const unsigned char a = i < expected.size() ? static_cast<unsigned char>(expected[i]) : 0;
        const unsigned char b = i < given.size() ? static_cast<unsigned char>(given[i]) : 0;
        mismatch |= a ^ b;
    }
    return mismatch == 0;
}

class LoginThrottle {
    struct Entry {
        std::deque<Clock::time_point> failures;
        Clock::time_point blocked_until{};
    };
    std::mutex mutex_;
    std::map<std::string, Entry> entries_;
    void prune(Entry& entry, Clock::time_point now) {
        while (!entry.failures.empty() && now - entry.failures.front() >= std::chrono::seconds(30))
            entry.failures.pop_front();
    }
public:
    bool allowed(const std::string& ip) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = entries_.find(ip);
        return it == entries_.end() || Clock::now() >= it->second.blocked_until;
    }
    void failed(const std::string& ip) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto now = Clock::now();
        auto& entry = entries_[ip];
        prune(entry, now);
        entry.failures.push_back(now);
        if (entry.failures.size() >= 5)
            entry.blocked_until = now + std::chrono::seconds(10);
        if (entries_.size() > 10000) entries_.clear();
    }
    void succeeded(const std::string& ip) {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.erase(ip);
    }
};
bool read_exact(NativeSocket socket, std::uint8_t* dst, std::size_t count) {
    while (count) {
        const int n = recv(socket, reinterpret_cast<char*>(dst),
                           static_cast<int>(count), 0);
        if (n <= 0) return false;
        dst += n;
        count -= static_cast<std::size_t>(n);
    }
    return true;
}
bool send_all(NativeSocket socket, const std::uint8_t* src, std::size_t count) {
    while (count) {
        const int n = send(socket, reinterpret_cast<const char*>(src),
                           static_cast<int>(count), 0);
        if (n <= 0) return false;
        src += n;
        count -= static_cast<std::size_t>(n);
    }
    return true;
}
void set_socket_timeout(NativeSocket socket) {
#ifdef _WIN32
    DWORD timeout = 6000;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    timeval timeout{6, 0};
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
}
struct Permit {
    std::atomic<unsigned int>& active;
    ~Permit() { --active; }
};
void handle_client(NativeSocket socket, const std::string& client_ip,
                   const Settings& settings, const World& world,
                   LoginThrottle& throttle, std::atomic<unsigned int>& active) {
    SocketHandle client(socket);
    Permit permit{active};
    set_socket_timeout(socket);
    // MySQL C API needs thread-local initialization for each worker.
    if (mysql_thread_init() != 0) return;
    struct MySqlThreadEnd { ~MySqlThreadEnd() { mysql_thread_end(); } } thread_end;
    coauth::AuthCipher cipher;
    std::array<std::uint8_t, 276> packet{};
    if (!read_exact(socket, packet.data(), 2)) return;
    cipher.decrypt(packet.data(), 2);
    if (coauth::read16(packet.data()) != packet.size()) return;
    if (!read_exact(socket, packet.data() + 2, packet.size() - 2)) return;
    cipher.decrypt(packet.data() + 2, packet.size() - 2);
    auto login = coauth::parse_login(packet.data(), packet.size());

    std::uint32_t id = 0;
    std::uint32_t status = static_cast<std::uint32_t>(coauth::ForwardCode::InvalidInfo);
    std::string host;
    std::uint16_t port = 0;
    if (!login || !throttle.allowed(client_ip)) {
        if (login) log_line("Temporarily rate-limited login from " + client_ip);
        else throttle.failed(client_ip);
    } else {
        try {
            DbPtr db = open_db(settings.db);
            auto account = find_account(db.get(), login->username);
            if (account && account->state == 1) {
                status = static_cast<std::uint32_t>(coauth::ForwardCode::Banned);
            } else if (account && same_password(account->password, login->password)) {
                // Match the old Forward packet: UID at 4 and account State at 8.
                id = account->id;
                status = account->state;
                host = world.ip;
                port = world.port;
                throttle.succeeded(client_ip);
                log_line("Login accepted: " + login->username + " from " + client_ip);
            }
        } catch (const std::exception& e) {
            log_line(std::string("Login database error: ") + e.what());
        }
        if (!id) throttle.failed(client_ip);
    }
    auto response = coauth::make_forward(id, status, host, port);
    cipher.encrypt(response.data(), response.size());
    send_all(socket, response.data(), response.size());
}
} // namespace

int main() {
    try {
#ifdef _WIN32
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
            throw std::runtime_error("WSAStartup failed");
#endif
        const Settings settings = load_settings();
        if (mysql_library_init(0, nullptr, nullptr) != 0)
            throw std::runtime_error("mysql_library_init failed");
        auto startup_db = open_db(settings.db);
        const World world = load_world(startup_db.get(), settings.world);
        startup_db.reset();
        SocketHandle server(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        if (server.value == invalid_socket) throw std::runtime_error("Cannot create TCP socket");
        int reuse = 1;
        setsockopt(server.value, SOL_SOCKET, SO_REUSEADDR,
#ifdef _WIN32
                   reinterpret_cast<const char*>(&reuse),
#else
                   &reuse,
#endif
                   sizeof(reuse));
        sockaddr_in listen_address{};
        listen_address.sin_family = AF_INET;
        listen_address.sin_port = htons(settings.port);
        if (inet_pton(AF_INET, settings.bind_ip.c_str(), &listen_address.sin_addr) != 1)
            throw std::runtime_error("AUTH_BIND must be an IPv4 address");
        if (::bind(server.value, reinterpret_cast<const sockaddr*>(&listen_address),
                   sizeof(listen_address)) != 0 || listen(server.value, SOMAXCONN) != 0)
            throw std::runtime_error("Cannot bind/listen on authentication port");
        log_line("Native C++ AccountServer listening on " + settings.bind_ip + ":" +
                 std::to_string(settings.port));
        log_line("World " + settings.world + " -> " + world.ip + ":" + std::to_string(world.port));
        LoginThrottle throttle;
        std::atomic<unsigned int> active{0};
        constexpr unsigned int max_clients = 128;
        for (;;) {
            sockaddr_in remote{};
#ifdef _WIN32
            int remote_length = sizeof(remote);
#else
            socklen_t remote_length = sizeof(remote);
#endif
            const NativeSocket client = accept(server.value,
                reinterpret_cast<sockaddr*>(&remote), &remote_length);
            if (client == invalid_socket) continue;
            if (active.fetch_add(1) >= max_clients) {
                --active;
                close_socket(client);
                continue;
            }
            char address[INET_ADDRSTRLEN]{};
            inet_ntop(AF_INET, &remote.sin_addr, address, sizeof(address));
            try {
                std::thread(handle_client, client, std::string(address),
                            std::cref(settings), std::cref(world),
                            std::ref(throttle), std::ref(active)).detach();
            } catch (...) {
                --active;
                close_socket(client);
                throw;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "AccountServer failed: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
