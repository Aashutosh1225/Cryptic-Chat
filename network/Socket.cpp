#include "Socket.hpp"
#include <cstring>
#include <utility>
#include <iostream>

// ---------------- WinsockGuard ----------------
// Windows requires a one-time WSAStartup()/WSACleanup() call per process.
// On Linux/Mac this class does nothing — safe to construct on both platforms.
Socket::WinsockGuard::WinsockGuard() {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << "\n";
    }
#endif
}

Socket::WinsockGuard::~WinsockGuard() {
#ifdef _WIN32
    WSACleanup();
#endif
}

// ---------------- Construction / Destruction ----------------
Socket::Socket() {
    handle_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (!isValid()) {
        std::cerr << "Failed to create socket\n";
    }
}

Socket::Socket(socket_t existingHandle) : handle_(existingHandle) {}

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept : handle_(other.handle_) {
    other.handle_ = INVALID_SOCKET_VALUE;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        other.handle_ = INVALID_SOCKET_VALUE;
    }
    return *this;
}

bool Socket::isValid() const {
    return handle_ != INVALID_SOCKET_VALUE;
}

void Socket::close() {
    if (isValid()) {
#ifdef _WIN32
        closesocket(handle_);
#else
        ::close(handle_);
#endif
        handle_ = INVALID_SOCKET_VALUE;
    }
}

// ---------------- Server-side ----------------
bool Socket::bind(uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;   // listen on all interfaces
    addr.sin_port = htons(port);

    int result = ::bind(handle_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    return result == 0;
}

bool Socket::listen(int backlog) {
    return ::listen(handle_, backlog) == 0;
}

Socket Socket::accept() {
    sockaddr_in clientAddr{};
 #ifdef _WIN32
    int addrLen = sizeof(clientAddr);
 #else
    socklen_t addrLen = sizeof(clientAddr);
 #endif
    socket_t clientHandle = ::accept(handle_, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
    return Socket(clientHandle);   // moved out via move constructor
}

// ---------------- Client-side ----------------
bool Socket::connectTo(const std::string& host, uint16_t port) {
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    unsigned long addr = inet_addr(host.c_str());
    if (addr == INADDR_NONE) {
        std::cerr << "Invalid address: " << host << "\n";
        return false;
    }
    serverAddr.sin_addr.s_addr = addr;
    return ::connect(handle_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == 0;
}

// ---------------- Data transfer ----------------
ssize_t Socket::send(const uint8_t* data, size_t len) {
#ifdef _WIN32
    return ::send(handle_, reinterpret_cast<const char*>(data), static_cast<int>(len), 0);
#else
    return ::send(handle_, data, len, 0);
#endif
}

ssize_t Socket::receive(uint8_t* buffer, size_t maxLen) {
#ifdef _WIN32
    return ::recv(handle_, reinterpret_cast<char*>(buffer), static_cast<int>(maxLen), 0);
#else
    return ::recv(handle_, buffer, maxLen, 0);
#endif
}
