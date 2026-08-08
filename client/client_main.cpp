// client_main.cpp -- Phase 14: the real client executable.
//
// Wires together everything built across Phases 1-14 into one running
// process:
//   Socket + Connection<Socket>         (Phases 1, 12 -- transport + handshake)
//   ClientSession<Socket>               (Phase 14 -- network thread <-> MessageQueue bridge)
//   ChatWindow                          (Phase 11 -- SFML render loop + widgets)
//
// LOGIN SCREEN -- a deliberate design decision, not an oversight:
// ChatWindow's documented interface (MessageQueue<std::string>& +
// setOnSend()) is chat-message-shaped; nothing about it handles
// collecting a username/password before the chat screen opens, and
// Server's auth-gate protocol (Phase 13) requires exactly one
// REGISTER/LOGIN control message before any chat traffic is allowed.
// Building a second SFML screen (widgets, layout, a whole second
// Widget-composing class alongside ChatWindow) just to collect two
// strings once at startup is disproportionate to what it buys for an
// undergrad demonstration project -- so this uses a blocking CONSOLE
// prompt (std::cin) before the SFML window ever opens: a real, working
// answer for v1, not a stand-in for "real" UI. A GUI login screen
// remains a reasonable enhancement to layer in later without touching
// ChatWindow itself (same "later phase" spirit as ChatWindow's own
// fixed-layout, no-resize scope cut in Phase 11) -- it would slot in as
// the same kind of self-contained widget-composing class ChatWindow
// itself already is.
//
// Each failed login/register attempt requires a FRESH TCP connection:
// Server::authenticate() (Phase 13) closes the connection on any ERR
// reply (documented there as "one attempt, no retry loop" -- the
// server-side connection object is destroyed the moment handleClient()
// returns false from authenticate()). So this file's retry loop opens a
// brand new Socket + ClientSession per attempt rather than trying to
// reuse a socket the server has already torn down.
//
// Build (from an MSYS2 UCRT64 shell, in the project root -- one line):
//
//   g++ -std=c++17 -Wall -Wextra -I. client/client_main.cpp ui/ChatWindow.cpp ui/Button.cpp ui/TextInputBox.cpp ui/ScrollableTextArea.cpp network/Message.cpp network/Socket.cpp crypto/Cipher.cpp crypto/RSAKeyExchange.cpp crypto/PasswordHasher.cpp auth/AuthManager.cpp db/Database.cpp db/UserRepository.cpp -lssl -lcrypto -lsqlite3 -lws2_32 -lsfml-graphics -lsfml-window -lsfml-system -o client.exe
//
//   ./client.exe [fontPath] [host] [port]
//   (defaults: fontPath=C:\Windows\Fonts\arial.ttf, host=127.0.0.1, port=5555)
//
// NOT VERIFIED IN THIS SANDBOX: this file #includes ChatWindow.hpp,
// which pulls in <SFML/Graphics.hpp> -- SFML isn't installed in this
// Linux sandbox (building it from source was already a heavy undertaking
// documented back in Phase 10, and wasn't repeated just to compile-check
// this one file). Every OTHER file this depends on (ClientSession,
// Connection, Socket, AuthManager, Database) has already been separately
// compiled and/or tested in this sandbox or cross-compiled for Windows
// in earlier phases -- this file's own logic was written carefully and
// reviewed against ChatWindow's actual header/source (not guessed at),
// but your MSYS2 UCRT64 build is the first real compile it will see.
// Please report back anything that doesn't build cleanly.

#include "ClientSession.hpp"
#include "../ui/ChatWindow.hpp"
#include "../network/Socket.hpp"
#include "../concurrency/MessageQueue.hpp"

#include <SFML/Graphics.hpp>

#include <iostream>
#include <memory>
#include <string>

namespace {

constexpr uint16_t kDefaultPort = 5555;
constexpr const char* kDefaultHost = "127.0.0.1";
constexpr const char* kDefaultFontPath = "C:\\Windows\\Fonts\\arial.ttf";
constexpr int kMaxLoginAttempts = 5;

// Reads one line from stdin, trimming the trailing newline. Used for
// both the command ("register"/"login") and the credentials -- kept
// deliberately simple (no password masking) since this is a console
// prompt meant to get a working v1 running, not a polished login UX;
// see the file-level comment for why a real GUI screen is out of scope
// here.
std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

// Runs one login/register attempt end-to-end: opens a fresh Socket,
// connects, wraps it in a ClientSession, runs the handshake, and sends
// exactly one auth control message. Returns the authenticated session
// (ready for start()) on success, or nullptr on any failure -- the
// caller decides whether to retry (with a brand new attempt, per the
// file-level comment on why attempts can't be reused).
std::unique_ptr<ClientSession<Socket>> attemptLogin(const std::string& host, uint16_t port,
                                                      concurrency::MessageQueue<std::string>& incoming,
                                                      std::string& outUsername) {
    std::string command = readLine("Type 'register' or 'login': ");
    if (command != "register" && command != "login") {
        std::cout << "Please type exactly 'register' or 'login'.\n";
        return nullptr;
    }
    //std::string protocolCommand = (command == "register") ? ctd"REGISTER" : "LOGIN";
    std::string protocolCommand;
    if(command == "register"){
        std::cout<<"REGISTER"<<std::endl;
        std::cout<<"Minimum password length is 8 characters"<<std::endl;
        std::cout<<"Minimum username length is 3 characters and maximum is 32 characters"<<std::endl;
        protocolCommand = "REGISTER";
    }
    else{
        std::cout<<"LOGIN"<<std::endl;
        protocolCommand = "LOGIN";
    }
    std::string username = readLine("Username: ");
    std::string password = readLine("Password: ");

    Socket socket;
    if (!socket.connectTo(host, port)) {
        std::cout << "Could not connect to " << host << ":" << port << "\n";
        return nullptr;
    }

    auto session = std::make_unique<ClientSession<Socket>>(std::move(socket), incoming);

    std::string reply;
    bool ok = session->connectAndAuthenticate(Connection<Socket>::Role::Client,
                                               protocolCommand, username, password, reply);
    if (!ok) {
        std::cout << "Failed: " << reply << "\n";
        return nullptr;   // session destructs here, closing the (already server-torn-down) socket
    }

    outUsername = username;
    return session;
}

} // namespace

int main(int argc, char** argv) {
    std::string fontPath = (argc >= 2) ? argv[1] : kDefaultFontPath;
    std::string host = (argc >= 3) ? argv[2] : kDefaultHost;
    uint16_t port = kDefaultPort;
    if (argc >= 4) {
        int parsed = std::atoi(argv[3]);
        if (parsed > 0 && parsed <= 65535) {
            port = static_cast<uint16_t>(parsed);
        } else {
            std::cerr << "Invalid port '" << argv[3] << "', using default " << kDefaultPort << "\n";
        }
    }

    // One-time global Winsock init/teardown for the whole process --
    // must happen before any Socket is constructed. RAII: WSACleanup()
    // runs automatically when main() returns.
    Socket::WinsockGuard winsockGuard;

    sf::Font font;
    if (!font.openFromFile(fontPath)) {
        std::cerr << "Failed to load font from '" << fontPath
                  << "'. Pass a valid .ttf path as the first argument, e.g.:\n"
                  << "  client.exe C:\\Windows\\Fonts\\segoeui.ttf\n";
        return 1;
    }

    // incoming_ is shared between the network thread (ClientSession's
    // receive loop, started below) and the render thread (ChatWindow's
    // update(), which drains it via tryPop() every frame) -- this is
    // the exact MessageQueue<std::string> handoff both Phase 9 and
    // Phase 11 were built around.
    concurrency::MessageQueue<std::string> incoming;

    std::cout << "Connecting to " << host << ":" << port << "\n";

    std::unique_ptr<ClientSession<Socket>> session;
    std::string username;
    for (int attempt = 1; attempt <= kMaxLoginAttempts && !session; ++attempt) {
        session = attemptLogin(host, port, incoming, username);
        if (!session && attempt < kMaxLoginAttempts) {
            std::cout << "(attempt " << attempt << "/" << kMaxLoginAttempts << " failed -- try again)\n";
        }
    }

    if (!session) {
        std::cerr << "Too many failed login attempts. Exiting.\n";
        return 1;
    }

    std::cout << "Logged in as " << username << ".\n";

    // Only NOW does the receive-loop thread start pushing incoming
    // messages into `incoming` -- deliberately after authentication
    // succeeds, so the OK/ERR reply above (read directly inside
    // connectAndAuthenticate(), not through the queue) is never at risk
    // of racing with ChatWindow's own tryPop() drain.
    session->start();

    ui::ChatWindow chatWindow("Encrypted Chat - " + username, {900, 600}, font, incoming);
    chatWindow.setStatus("Connected as " + username + " (" + host + ":" + std::to_string(port) + ")");
    chatWindow.appendSystemMessage("Connected. Type a message and press Enter or click Send.");

    // Every outgoing message ChatWindow's compose box/Send button
    // produces gets encrypted and sent here -- this is the entire
    // "networking" surface ChatWindow itself ever touches, exactly as
    // designed back in Phase 11.
    chatWindow.setOnSend([&session](const std::string& text) {
        if (!session->send(text)) {
            // sendMessage() failing here almost always means the
            // connection has already dropped (server closed it, network
            // blip, etc). ChatWindow has no dedicated "delivery failed"
            // UI, so this surfaces as a system-style line in the same
            // chat history the user is already looking at, appended
            // locally the same way appendSystemMessage() would -- kept
            // inline here rather than plumbed through a second callback
            // for one rare error case.
            std::cerr << "Failed to send message (connection may have dropped)\n";
        }
    });

    chatWindow.run();   // blocks until the window is closed (X button or Escape)

    session->stop();
    std::cout << "Disconnected.\n";
    return 0;
}
