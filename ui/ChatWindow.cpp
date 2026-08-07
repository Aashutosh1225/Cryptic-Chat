#include "ChatWindow.hpp"

namespace ui {

ChatWindow::ChatWindow(const std::string& title, sf::Vector2u size, const sf::Font& font,
                        concurrency::MessageQueue<std::string>& incoming)
    : window_(sf::VideoMode(size), title, sf::Style::Titlebar | sf::Style::Close)
    , font_(font)
    , incoming_(incoming)
    , history_(font, historyPosition(), historySize(size))
    , input_(font, inputPosition(size), inputSize(size))
    , sendButton_(font, "Send", sendButtonPosition(size), sendButtonSize())
    , statusText_(font, "", 14)
{
    window_.setVerticalSyncEnabled(true);

    input_.setPlaceholder("Type a message...");
    input_.setMaxLength(2000);

    statusText_.setFillColor(sf::Color(160, 160, 160));
    statusText_.setPosition({kPadding, kPadding / 2.f});

    // Wire the widgets to each other: Enter in the input box, or clicking
    // Send, both go through the same sendCurrentMessage() path.
    input_.setOnSubmit([this](const std::string&) { sendCurrentMessage(); });
    sendButton_.setOnClick([this] { sendCurrentMessage(); });

    // A window is more useful to compose in immediately, so focus it.
    input_.setFocused(true);
}

void ChatWindow::setOnSend(std::function<void(const std::string&)> callback)
{
    onSend_ = std::move(callback);
}

void ChatWindow::appendSystemMessage(const std::string& text)
{
    history_.addLine(text);
}

void ChatWindow::setStatus(const std::string& status)
{
    statusText_.setString(status);
}

bool ChatWindow::isOpen() const
{
    return window_.isOpen();
}

void ChatWindow::sendCurrentMessage()
{
    std::string text = input_.getText();
    if (text.empty()) return;

    // Show it locally immediately -- don't wait for a network round-trip
    // (there may not even be a Connection wired up yet in this phase).
    history_.addLine("You: " + text);

    if (onSend_)
    {
        onSend_(text);
    }

    input_.clear();
}

void ChatWindow::handleEvent(const sf::Event& event)
{
    if (event.is<sf::Event::Closed>())
    {
        window_.close();
        return;
    }

    if (const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        if (key->code == sf::Keyboard::Key::Escape)
        {
            window_.close();
            return;
        }
    }

    // Every other event (mouse move/click, text entry, scroll, other
    // keys) is fanned out to all three widgets. Each widget already
    // ignores events outside its own bounds or irrelevant to it, so no
    // widget-specific routing logic is needed here.
    history_.handleEvent(event);
    input_.handleEvent(event);
    sendButton_.handleEvent(event);
}

void ChatWindow::pollAndHandleEvents()
{
    while (const std::optional<sf::Event> event = window_.pollEvent())
    {
        handleEvent(*event);
    }
}

void ChatWindow::update()
{
    input_.update();

    // Drain every message currently queued -- not just one per frame --
    // so a burst of incoming messages doesn't trickle in slowly at one
    // per render tick.
    while (std::optional<std::string> message = incoming_.tryPop())
    {
        history_.addLine(*message);
    }
}

void ChatWindow::render()
{
    window_.clear(sf::Color(15, 15, 18));

    history_.draw(window_);
    input_.draw(window_);
    sendButton_.draw(window_);
    window_.draw(statusText_);

    window_.display();
}

void ChatWindow::step()
{
    pollAndHandleEvents();
    update();
    render();
}

void ChatWindow::run()
{
    while (isOpen())
    {
        step();
    }
}

// --- Layout helpers ---
// Simple fixed layout: status bar strip at top, history fills the middle,
// input box + send button pinned to the bottom. The window is created
// without the Resize style (see constructor), so this doesn't need to
// react to live resizing -- keeping widget geometry fixed avoids adding
// setSize() to every widget just for this phase.

sf::Vector2f ChatWindow::historyPosition()
{
    return {kPadding, kPadding + kStatusHeight};
}

sf::Vector2f ChatWindow::historySize(sf::Vector2u windowSize)
{
    float width = static_cast<float>(windowSize.x) - 2.f * kPadding;
    float height = static_cast<float>(windowSize.y) - 3.f * kPadding - kStatusHeight - kInputHeight;
    return {width, height};
}

sf::Vector2f ChatWindow::inputPosition(sf::Vector2u windowSize)
{
    float y = static_cast<float>(windowSize.y) - kPadding - kInputHeight;
    return {kPadding, y};
}

sf::Vector2f ChatWindow::inputSize(sf::Vector2u windowSize)
{
    float width = static_cast<float>(windowSize.x) - 3.f * kPadding - kSendButtonWidth;
    return {width, kInputHeight};
}

sf::Vector2f ChatWindow::sendButtonPosition(sf::Vector2u windowSize)
{
    float x = static_cast<float>(windowSize.x) - kPadding - kSendButtonWidth;
    float y = static_cast<float>(windowSize.y) - kPadding - kInputHeight;
    return {x, y};
}

sf::Vector2f ChatWindow::sendButtonSize()
{
    return {kSendButtonWidth, kInputHeight};
}

} // namespace ui
