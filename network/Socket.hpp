#pragma once
#include <cstdint>
#include <string>
#include <cstddef>

// ---- Platform-specific includes & type aliases ----
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")
    using socket_t = SOCKET;
    constexpr socket_t INVALID_SOCKET_VALUE = INVALID_SOCKET;
        using ssize_t = long long;   // MSVC has no ssize_t by default
        #define _SSIZE_T_DEFINED

// Socket: RAII wrapper around a raw TCP socket handle.
// Works identically on Linux/Mac (POSIX) and Windows (Winsock) —
// callers never touch socket_t directly.
class Socket {
public:
    Socket();                                  // creates a TCP socket
    explicit Socket(socket_t existingHandle);   // wrap an already-accepted handle
    ~Socket();                                  // RAII: closes handle automatically

    // Non-copyable (a socket handle should have one owner)
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // Movable (ownership can transfer, e.g. out of accept())
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    bool bind(uint16_t port);
    bool listen(int backlog = 10);
    Socket accept();                            // blocks until a client connects
    bool connectTo(const std::string& host, uint16_t port);

    ssize_t send(const uint8_t* data, size_t len);
    ssize_t receive(uint8_t* buffer, size_t maxLen);

    bool isValid() const;
    void close();

    // One-time global setup/teardown for Winsock (no-op on Linux/Mac).
    // Call WinsockGuard once at the very start of main() on either platform.
    class WinsockGuard {
    public:
        WinsockGuard();
        ~WinsockGuard();
    };

private:
    socket_t handle_;
};
