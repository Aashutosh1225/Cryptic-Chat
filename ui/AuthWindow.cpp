#include "AuthWindow.hpp"

namespace ui {

namespace {
constexpr sf::Vector2u kWindowSize{620, 520};
constexpr float kCardLeft = 90.f;
constexpr float kFieldLeft = 140.f;
constexpr float kFieldWidth = 340.f;
}

AuthWindow::AuthWindow(const sf::Font& font, const std::string& host, uint16_t port)
    : window_(sf::VideoMode(kWindowSize), "Cryptic-Chat | Welcome", sf::Style::Titlebar | sf::Style::Close)
    , font_(font)
    , appTitle_(font, "Cryptic-Chat", 34)
    , subtitle_(font, "Private conversations, simply connected.", 16)
    , usernameLabel_(font, "Username", 15)
    , passwordLabel_(font, "Password", 15)
    , helpText_(font, "", 13)
    , statusText_(font, "", 14)
    , username_(font, {kFieldLeft, 205.f}, {kFieldWidth, 42.f})
    , password_(font, {kFieldLeft, 290.f}, {kFieldWidth, 42.f})
    , primaryButton_(font, "Sign in", {kFieldLeft, 360.f}, {kFieldWidth, 42.f})
    , switchModeButton_(font, "Create an account", {kFieldLeft, 414.f}, {kFieldWidth, 34.f})
{
    window_.setVerticalSyncEnabled(true);
    card_.setPosition({kCardLeft, 105.f});
    card_.setSize({440.f, 375.f});
    card_.setFillColor(sf::Color(29, 35, 48));
    card_.setOutlineThickness(1.f);
    card_.setOutlineColor(sf::Color(67, 83, 108));

    appTitle_.setFillColor(sf::Color(114, 190, 255));
    appTitle_.setPosition({kCardLeft, 35.f});
    subtitle_.setFillColor(sf::Color(177, 190, 210));
    subtitle_.setPosition({kCardLeft, 75.f});
    usernameLabel_.setFillColor(sf::Color(220, 226, 235));
    usernameLabel_.setPosition({kFieldLeft, 180.f});
    passwordLabel_.setFillColor(sf::Color(220, 226, 235));
    passwordLabel_.setPosition({kFieldLeft, 265.f});
    helpText_.setFillColor(sf::Color(160, 175, 197));
    helpText_.setPosition({kFieldLeft, 145.f});
    statusText_.setPosition({kFieldLeft, 462.f});

    username_.setPlaceholder("3 - 32 characters: letters, numbers, _");
    username_.setMaxLength(32);
    password_.setPlaceholder("At least 8 characters");
    password_.setPasswordMode(true);
    password_.setMaxLength(256);
    username_.setFocused(true);

    primaryButton_.setOnClick([this] { submit(); });
    primaryButton_.setPrimary(true);
    switchModeButton_.setOnClick([this] {
        registerMode_ = !registerMode_;
        updateModeText();
    });
    username_.setOnSubmit([this](const std::string&) { password_.setFocused(true); });
    password_.setOnSubmit([this](const std::string&) { submit(); });
    updateModeText();
    setStatus("Server: " + host + ":" + std::to_string(port), sf::Color(145, 170, 200));
}

void AuthWindow::setOnAuthenticate(AuthenticateCallback callback)
{
    onAuthenticate_ = std::move(callback);
}

bool AuthWindow::run()
{
    while (window_.isOpen()) {
        while (const std::optional event = window_.pollEvent()) handleEvent(*event);
        update();
        render();
    }
    return authenticated_;
}

void AuthWindow::handleEvent(const sf::Event& event)
{
    if (event.is<sf::Event::Closed>() || (event.is<sf::Event::KeyPressed>() && event.getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Escape)) {
        window_.close();
        return;
    }
    username_.handleEvent(event);
    password_.handleEvent(event);
    primaryButton_.handleEvent(event);
    switchModeButton_.handleEvent(event);
}

void AuthWindow::update()
{
    username_.update();
    password_.update();
}

void AuthWindow::render()
{
    window_.clear(sf::Color(15, 20, 30));
    window_.draw(appTitle_);
    window_.draw(subtitle_);
    window_.draw(card_);
    window_.draw(helpText_);
    window_.draw(usernameLabel_);
    window_.draw(passwordLabel_);
    username_.draw(window_);
    password_.draw(window_);
    primaryButton_.draw(window_);
    switchModeButton_.draw(window_);
    window_.draw(statusText_);
    window_.display();
}

void AuthWindow::submit()
{
    if (username_.getText().empty() || password_.getText().empty()) {
        setStatus("Enter both a username and password.", sf::Color(255, 150, 150));
        return;
    }
    if (!onAuthenticate_) return;

    setStatus("Connecting securely…", sf::Color(255, 220, 130));
    render();
    std::string status;
    if (onAuthenticate_(registerMode_, username_.getText(), password_.getText(), status)) {
        authenticated_ = true;
        window_.close();
        return;
    }
    password_.clear();
    password_.setFocused(true);
    setStatus(status.empty() ? "Could not sign in. Please try again." : status, sf::Color(255, 150, 150));
}

void AuthWindow::updateModeText()
{
    primaryButton_.setLabel(registerMode_ ? "Create account" : "Sign in");
    switchModeButton_.setLabel(registerMode_ ? "I already have an account" : "Create an account");
    helpText_.setString(registerMode_ ? "Create an account to join the chat." : "Sign in to continue to the chat.");
    password_.setPlaceholder(registerMode_ ? "At least 8 characters" : "Your password");
}

void AuthWindow::setStatus(const std::string& text, sf::Color color)
{
    statusText_.setString(text);
    statusText_.setFillColor(color);
}

} // namespace ui
