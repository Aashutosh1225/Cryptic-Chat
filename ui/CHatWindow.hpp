#pragma once

#include "Widget.hpp"
#include "Button.hpp"
#include "TextInputBox.hpp"
#include "ScrollableTextArea.hpp"
#include "../concurrency/MessageQueue.hpp"

#include <SFML/Graphics.hpp>
#include <functional>
#include <string>

namespace ui {

// Owns the real sf::RenderWindow and the event/render loop for the chat
// client. Composes Button + TextInputBox + ScrollableTextArea into a
// single-screen chat UI: history area on top, compose box + send button
// on the bottom.
//
// ChatWindow deliberately has ZERO dependency on Connection, Socket, or
// any networking/crypto type (those don't exist yet -- Connection is
// Phase 12). Instead it's wired to the rest of the app through two plain
// interfaces:
//
//   - incoming: a concurrency::MessageQueue<std::string>& that some other
//     thread (the eventual network thread, Phase 12/13) pushes decrypted
//     plaintext messages into. ChatWindow drains it once per frame with
//     tryPop() (non-blocking -- never stalls the render loop) and appends
//     each message to the chat history.
//
//   - outgoing: a std::function<void(const std::string&)> callback fired
//     whenever the user hits Enter or clicks Send. Phase 12/13 will set
//     this to something that encrypts and sends over a live Connection;
//     until then it can be left unset (messages just show locally) or
//     pointed at a test double, which is exactly what this phase's own
//     tests do.
//
// This keeps ChatWindow fully testable and buildable *before* networking
// exists, and means Phase 12 only has to call setOnSend()/push messages
// into the queue -- it never needs to touch ChatWindow's internals.
class ChatWindow {
public:
    // font must outlive the ChatWindow. incoming must outlive the
    // ChatWindow and is expected to be shared with a network thread.
    ChatWindow(const std::string& title, sf::Vector2u size, const sf::Font& font,
               concurrency::MessageQueue<std::string>& incoming);

    void setOnSend(std::function<void(const std::string&)> callback);

    // Adds a line straight to the chat history without going through the
    // incoming queue or the send callback -- for local system messages
    // ("Connected as alice", "bob left the room", etc).
    void appendSystemMessage(const std::string& text);

    void setStatus(const std::string& status);

    bool isOpen() const;

    // Runs pollAndHandleEvents() + update() + render() in a loop until the
    // window closes. This is what client_main.cpp (Phase 14) will call.
    void run();

    // --- Building blocks of run(), exposed individually for testing ---
    // Dispatches a single event to every widget (and handles window-level
    // events like Closed/Escape itself). Tests call this directly with
    // synthetic sf::Event objects, exactly like the Phase 10 widget tests
    // -- no real OS event injection needed.
    void handleEvent(const sf::Event& event);

    // Drains real OS events from the window via pollEvent() and calls
    // handleEvent() for each. Used by run(); tests generally call
    // handleEvent() directly instead so they control exactly which events
    // fire, but this is still exercised by the real-window smoke test.
    void pollAndHandleEvents();

    // Advances widget timers (cursor blink) and drains the incoming queue
    // into the chat history.
    void update();

    // Clears, draws all widgets, displays.
    void render();

    void step(); // pollAndHandleEvents() + update() + render()

    // Test/inspection hooks -- not part of the "real" UI surface, but
    // avoids needing friend classes or exposing widget internals just to
    // verify behavior.
    const std::string& getInputText() const { return input_.getText(); }
    std::size_t getHistoryLineCount() const { return history_.lineCount(); }

private:
    sf::RenderWindow window_;
    const sf::Font& font_;
    concurrency::MessageQueue<std::string>& incoming_;

    ScrollableTextArea history_;
    TextInputBox input_;
    Button sendButton_;
    sf::Text statusText_;

    std::function<void(const std::string&)> onSend_;

    static constexpr float kPadding = 10.f;
    static constexpr float kInputHeight = 32.f;
    static constexpr float kSendButtonWidth = 80.f;
    static constexpr float kStatusHeight = 24.f;

    void sendCurrentMessage();
    static sf::Vector2f historySize(sf::Vector2u windowSize);
    static sf::Vector2f historyPosition();
    static sf::Vector2f inputPosition(sf::Vector2u windowSize);
    static sf::Vector2f inputSize(sf::Vector2u windowSize);
    static sf::Vector2f sendButtonPosition(sf::Vector2u windowSize);
    static sf::Vector2f sendButtonSize();
};

} // namespace ui
