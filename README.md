# Cryptic Chat

A secure multi-user chat application developed in C++ using SFML, OpenSSL, SQLite, TCP sockets, and multithreading.

## Overview

Cryptic Chat is a client-server messaging application designed to demonstrate secure communication, user authentication, networking, concurrency, database integration, and graphical user interface development.

The project combines several computer science concepts including:

- Object-Oriented Programming
- Socket Programming
- Cryptography
- Database Management
- Concurrent Programming
- GUI Development

---

## Features

### User Authentication
- User registration
- User login
- PBKDF2-SHA256 password hashing
- Random salt generation
- Secure credential storage

### Secure Communication
- RSA key exchange
- AES-256 message encryption
- Session-based encryption keys
- Encrypted message transmission

### Networking
- TCP client-server architecture
- Multiple simultaneous clients
- Reliable message delivery
- Message serialization and deserialization

### Concurrency
- Dedicated network receive thread
- Responsive GUI thread
- Thread-safe message passing
- Mutex and condition variable synchronization

### Graphical User Interface
- Built using SFML
- Authentication window
- Chat window
- Scrollable chat history
- Message input controls

### Database
- SQLite integration
- User account management
- Secure storage of salts and password hashes

---

## Technologies Used

| Component | Technology |
|------------|------------|
| Language | C++17 |
| GUI | SFML |
| Networking | TCP Sockets |
| Cryptography | OpenSSL |
| Database | SQLite3 |
| Concurrency | std::thread |
| Synchronization | std::mutex, std::condition_variable |
| Build System | CMake |

---

## Project Structure

```text
Cryptic-Chat/
│
├── auth/
│   └── AuthManager.*
│
├── client/
│   ├── client_main.cpp
│   └── ClientSession.*
│
├── concurrency/
│   └── MessageQueue.hpp
│
├── crypto/
│   ├── Cipher.*
│   ├── RSAKeyExchange.*
│   └── PasswordHasher.*
│
├── db/
│   ├── Database.*
│   ├── UserRepository.*
│   └── ChatHistoryRepository.*
│
├── network/
│   ├── Connection.*
│   ├── Message.*
│   └── Socket.*
│
├── server/
│   ├── server_main.cpp
│   └── Server.*
│
├── ui/
│   ├── AuthWindow.*
│   ├── ChatWindow.*
│   ├── Button.*
│   ├── TextInputBox.*
│   └── ScrollableTextArea.*
│
└── CMakeLists.txt
```

---

## Security Features

### Password Security

Passwords are protected using:

- PBKDF2-SHA256
- Unique random salts
- Binary hash storage
- Binary salt storage

Plaintext passwords are never stored in the database.

### Encryption

The application uses:

- RSA for secure key exchange
- AES-256 for message encryption

RSA is used only during connection setup, while AES is used for chat communication due to its higher performance.

---

## Networking Implementation

The networking layer is built on TCP sockets.

Key functionality includes:

- Creating sockets
- Binding server ports
- Listening for connections
- Accepting clients
- Sending data
- Receiving data
- Message serialization
- Message deserialization

The server supports multiple connected clients simultaneously.

---

## Concurrency Implementation

The client application separates GUI operations from networking operations using multiple threads.

Synchronization is achieved through:

- `std::mutex`
- `std::lock_guard`
- `std::condition_variable`

A thread-safe `MessageQueue<T>` is used to transfer messages safely between the network thread and GUI thread.

---

## Database Design

SQLite is used for local data storage.

The database stores:

- Usernames
- Password salts
- Password hashes

Prepared statements are used to reduce SQL injection risks.

---

## Build Requirements

### Windows (MSYS2 UCRT64)

Install required packages:

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc
pacman -S mingw-w64-ucrt-x86_64-cmake
pacman -S mingw-w64-ucrt-x86_64-ninja
pacman -S mingw-w64-ucrt-x86_64-openssl
pacman -S mingw-w64-ucrt-x86_64-sqlite3
pacman -S mingw-w64-ucrt-x86_64-sfml
```

---

## Building

```bash
cmake -B build
cmake --build build
```

---

## Running the Server

```bash
server.exe
```

Specify a custom port:

```bash
server.exe 5555
```

---

## Running the Client

```bash
client.exe
```

Or specify font, host, and port:

```bash
client.exe "C:\Windows\Fonts\arial.ttf" 127.0.0.1 5555
```

---

## Concepts Demonstrated

- Object-Oriented Programming
- Templates
- RAII
- Move Semantics
- TCP/IP Networking
- Socket Programming
- Multithreading
- Producer-Consumer Pattern
- Thread Synchronization
- Cryptography
- Database Integration
- GUI Development
- Client-Server Architecture

---

## Future Improvements

- Group chats
- Message persistence
- File transfer
- End-to-end encryption
- User presence tracking
- Voice communication
- Asynchronous networking
- Thread pools

---

## Authors

Developed as an academic project demonstrating secure communication, networking, cryptography, concurrency, database management, and GUI development using modern C++.