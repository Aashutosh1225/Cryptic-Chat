#pragma once

#include "Button.hpp"
#include "TextInputBox.hpp"

#include <SFML/Graphics.hpp>
#include <functional>
#include <string>

namespace ui {

// The first screen of the client. It keeps credentials in the GUI and hands
// one registration/login attempt to the networking layer through a callback.
class AuthWindow {
public:
    using AuthenticateCallback = std::function<bool(bool registerMode,
                                                     const std::string& username,
                                                     const std::string& password,
                                                     std::string& status)>;

    AuthWindow(const sf::Font& font, const std::string& host, uint16_t port);

    void setOnAuthenticate(AuthenticateCallback callback);
    bool run(); // true only after a successful authentication

    const std::string& username() const { return username_.getText(); }

private:
    sf::RenderWindow window_;
    const sf::Font& font_;
    sf::Text appTitle_;
    sf::Text subtitle_;
    sf::Text usernameLabel_;
    sf::Text passwordLabel_;
    sf::Text helpText_;
    sf::Text statusText_;
    sf::RectangleShape card_;
    TextInputBox username_;
    TextInputBox password_;
    Button primaryButton_;
    Button switchModeButton_;
    AuthenticateCallback onAuthenticate_;
    bool registerMode_ = false;
    bool authenticated_ = false;

    void handleEvent(const sf::Event& event);
    void update();
    void render();
    void submit();
    void updateModeText();
    void setStatus(const std::string& text, sf::Color color);
};

} // namespace ui
