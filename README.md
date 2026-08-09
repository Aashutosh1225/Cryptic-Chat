# Cryptic-Chat
CrypticChat is a GUI-based encrypted multi-user chat app built in C++ with SFML. It uses client-server architecture, RSA key exchange, AES-256 encryption, and PBKDF2-SHA256 password security. It supports authentication, SQLite chat history, multithreading, and demonstrates networking, cryptography, OOP, databases, and GUI development.

## Build and run

Cryptic-Chat has two executables: `server` handles accounts and message
broadcasting; `client` provides the SFML chat window. Start the server first,
then one client process per chat user.

### Linux / Kali WSL

In Kali, install the compiler and libraries:

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build libssl-dev libsqlite3-dev libsfml-dev fonts-dejavu-core
```

Build from the repository root:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

Run the server in one terminal:

```bash
./build/server
```

Run a client in another terminal:

```bash
./build/client
```

The client opens a GUI sign-in screen. Choose **Create an account** for a new
user, or sign in with an existing account; the chat screen opens after a
successful authentication. WSL 2 needs GUI support: use WSLg on
Windows 11, or configure an X server on Windows. If the default font is not
available, pass a `.ttf` path as the first argument, for example
`./build/client /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf`.

### Windows / MSYS2 UCRT64

Install the UCRT64 versions of the C++ toolchain, CMake, Ninja, OpenSSL,
SQLite, and SFML through MSYS2. Then run the same CMake commands above from
the MSYS2 UCRT64 terminal. Start `./build/server.exe` first and
`./build/client.exe` in another terminal.
