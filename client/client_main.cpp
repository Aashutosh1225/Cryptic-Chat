#include "ClientSession.hpp"
#include "../concurrency/MessageQueue.hpp"
#include "../network/Socket.hpp"
#include "../ui/AuthWindow.hpp"
#include "../ui/ChatWindow.hpp"

#include <SFML/Graphics.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {

constexpr uint16_t kDefaultPort = 5555;
constexpr const char* kDefaultHost = "127.0.0.1";
#ifdef _WIN32
constexpr const char* kDefaultFontPath = "C:\\Windows\\Fonts\\arial.ttf";
#else
constexpr const char* kDefaultFontPath = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
#endif

std::unique_ptr<ClientSession<Socket>> attemptAuthentication(
    const std::string& host, uint16_t port, bool registerMode,
    const std::string& username, const std::string& password,
    concurrency::MessageQueue<std::string>& incoming, std::string& outStatus)
{
    Socket socket;
    if (!socket.connectTo(host, port)) {
        outStatus = "Could not connect to " + host + ":" + std::to_string(port) + ".";
        return nullptr;
    }

    auto session = std::make_unique<ClientSession<Socket>>(std::move(socket), incoming);
    std::string reply;
    if (!session->connectAndAuthenticate(Connection<Socket>::Role::Client,
                                         registerMode ? "REGISTER" : "LOGIN",
                                         username, password, reply)) {
        outStatus = reply;
        return nullptr;
    }

    outStatus = "Signed in successfully.";
    return session;
}

} // namespace

int main(int argc, char** argv)
{
    std::string fontPath = (argc >= 2) ? argv[1] : kDefaultFontPath;
    std::string host = (argc >= 3) ? argv[2] : kDefaultHost;
    uint16_t port = kDefaultPort;
    if (argc >= 4) {
        int parsed = std::atoi(argv[3]);
        if (parsed > 0 && parsed <= 65535) port = static_cast<uint16_t>(parsed);
    }

    Socket::WinsockGuard winsockGuard;

    sf::Font font;
    if (!font.openFromFile(fontPath)) {
        std::cerr << "Failed to load font from '" << fontPath << "'.\n";
        return 1;
    }

    concurrency::MessageQueue<std::string> incoming;
    std::unique_ptr<ClientSession<Socket>> session;
    std::string username;

    ui::AuthWindow authWindow(font, host, port);
    authWindow.setOnAuthenticate([&](bool registerMode, const std::string& enteredUsername,
                                     const std::string& password, std::string& status) {
        std::unique_ptr<ClientSession<Socket>> candidate = attemptAuthentication(
            host, port, registerMode, enteredUsername, password, incoming, status);
        if (!candidate) return false;
        username = enteredUsername;
        session = std::move(candidate);
        return true;
    });

    if (!authWindow.run() || !session) return 0;

    session->start();
    ui::ChatWindow chatWindow("Cryptic-Chat | " + username, {960, 640}, font, incoming);
    chatWindow.setCurrentUser(username);
    chatWindow.setStatus("Connected as " + username + "  •  " + host + ":" + std::to_string(port));
    chatWindow.appendSystemMessage("You are connected. Send a message with Enter or the Send button.");
    chatWindow.setOnSend([&session](const std::string& text) {
        if (!session->send(text)) std::cerr << "Failed to send message (connection may have dropped)\n";
    });
    chatWindow.run();
    session->stop();
    return 0;
}
