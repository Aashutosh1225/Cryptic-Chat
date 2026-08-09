// server_main.cpp -- Phase 14: the real server executable.
//
// Wires together everything built in Phases 1-13 into one running
// process:
//   Database + UserRepository + AuthManager   (Phases 6-7, login gate)
//   Server<Socket>                            (Phase 13, roster + broadcast)
//   Socket (listener)                         (Phase 1, accept() loop -- new here)
//
// server_main.cpp's own job is deliberately thin: everything about HOW a
// client is handled once accepted lives in Server<SocketT> (Phase 13),
// so this file is just "open a database, bind a port, and for every
// accepted connection spawn a thread that calls handleClient()." This
// mirrors exactly the shape TestServerHarness uses in
// server/test_server_win.cpp, which is deliberately why that test
// harness looks the way it does -- it's rehearsing this exact loop.
//
// Build (from an MSYS2 UCRT64 shell, in the project root -- one line):
//
//   g++ -std=c++17 -Wall -Wextra -I. server/server_main.cpp network/Message.cpp network/Socket.cpp crypto/Cipher.cpp crypto/RSAKeyExchange.cpp crypto/PasswordHasher.cpp db/Database.cpp db/UserRepository.cpp db/ChatHistoryRepository.cpp auth/AuthManager.cpp -lssl -lcrypto -lsqlite3 -lws2_32 -o server.exe
//
//   ./server.exe [port]        (defaults to 5555 if omitted)
//
// NOT wired up here (see server/README_phase13.md "Honest gaps" for why):
//   - ChatHistoryRepository persistence (session-scoped keys make stored
//     ciphertext undecryptable after a reconnect -- needs a room-key
//     redesign first, so it's left out rather than half-implemented).

#include "Server.hpp"
#include "../network/Socket.hpp"
#include "../db/Database.hpp"
#include "../db/UserRepository.hpp"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

// Set by the Ctrl+C handler; polled by the accept loop so the process
// can shut down cleanly instead of only via a hard kill. Client threads
// already spawned are detached (see main()) and will exit on their own
// once their socket errors/closes -- this flag only stops NEW
// connections from being accepted.
std::atomic<bool> shuttingDown{false};

void handleSigint(int) {
    shuttingDown.store(true);
}

constexpr uint16_t kDefaultPort = 5555;
constexpr int kListenBacklog = 16;
constexpr const char* kDatabasePath = "chatapp.db";

} // namespace

int main(int argc, char** argv) {
    uint16_t port = kDefaultPort;
    if (argc >= 2) {
        int parsed = std::atoi(argv[1]);
        if (parsed <= 0 || parsed > 65535) {
            std::cerr << "Invalid port '" << argv[1] << "', using default " << kDefaultPort << "\n";
        } else {
            port = static_cast<uint16_t>(parsed);
        }
    }

    // One-time global Winsock init/teardown for the whole process --
    // must happen before any Socket is constructed. RAII: WSACleanup()
    // runs automatically when main() returns, including on early return
    // paths below.
    Socket::WinsockGuard winsockGuard;

    Database db(kDatabasePath);
    if (!db.isOpen()) {
        std::cerr << "Failed to open database at '" << kDatabasePath << "'\n";
        return 1;
    }

    UserRepository users(db);
    if (!users.createTableIfNotExists()) {
        std::cerr << "Failed to initialize users table\n";
        return 1;
    }

    AuthManager auth(users);
    Server<Socket> server(auth);

    Socket listener;
    if (!listener.bind(port)) {
        std::cerr << "Failed to bind to port " << port << " -- already in use?\n";
        return 1;
    }
    if (!listener.listen(kListenBacklog)) {
        std::cerr << "Failed to listen on port " << port << "\n";
        return 1;
    }

    std::signal(SIGINT, handleSigint);

    std::cout << "Server listening on port " << port << " (Ctrl+C to stop)\n";

    // Accept loop: each accepted client gets its own detached thread
    // running Server::handleClient(), which blocks for that client's
    // entire connection lifetime (handshake, auth gate, then
    // receive-and-broadcast) and returns on its own once the client
    // disconnects. Detached rather than tracked in a joinable vector
    // here -- Server's own internal roster (clients_, mutex-protected)
    // is already the source of truth for "who's connected"; main()
    // doesn't need a second bookkeeping structure just to join threads
    // it has no other use for.
    while (!shuttingDown.load()) {
        Socket clientSocket = listener.accept();
        if (!clientSocket.isValid()) {
            // accept() can return an invalid socket if the listener was
            // closed (e.g. during shutdown) or on a transient error;
            // either way, loop back around and check shuttingDown again
            // rather than spinning tightly on a hard failure.
            if (shuttingDown.load()) break;
            continue;
        }

        std::thread([&server, sock = std::move(clientSocket)]() mutable {
            server.handleClient(std::move(sock));
        }).detach();
    }

    std::cout << "Shutting down.\n";
    return 0;
}
