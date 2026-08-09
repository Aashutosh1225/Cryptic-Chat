#pragma once

#include "Widget.hpp"
#include <deque>
#include <string>
#include <vector>

namespace ui {

// A scrollable, clipped list of text lines -- used for chat history.
//
// Clipping/scrolling in SFML (both 2.x and 3.x) is done with a dedicated
// sf::View whose viewport is set to the widget's on-screen rectangle
// (as a fraction of the render target's size) and whose center is offset
// by the current scroll amount. This part of the design is unaffected by
// the SFML 3 event/Rect changes -- sf::View's own API (setSize, setCenter,
// setViewport) did not change between 2.6.1 and 3.x.
class ScrollableTextArea : public Widget {
public:
    ScrollableTextArea(const sf::Font& font, sf::Vector2f position, sf::Vector2f size);

    void addLine(const std::string& line);
    void addMessage(const std::string& author, const std::string& text, bool ownMessage = false);
    void addSystemMessage(const std::string& text);
    void clear();
    std::size_t lineCount() const { return messages_.size(); }

    void handleEvent(const sf::Event& event) override;
    void draw(sf::RenderTarget& target) const override;
    void setPosition(sf::Vector2f position) override;
    sf::FloatRect getBounds() const override;

    // Exposed mainly for testing scroll math without a real render target.
    float getScrollOffset() const { return scrollOffset_; }
    float getMaxScroll() const;
    bool isPinnedToBottom() const { return pinnedToBottom_; }
    void scrollBy(float delta); // positive = scroll down (toward newest)

private:
    const sf::Font& font_;
    sf::Vector2f position_;
    sf::Vector2f size_;
    sf::RectangleShape background_;

    struct ChatMessage {
        std::string author;
        std::string text;
        std::string timestamp;
        bool own = false;
        bool system = false;
    };

    std::deque<ChatMessage> messages_;
    unsigned int characterSize_ = 17;
    float lineHeight_ = 22.f;

    float scrollOffset_ = 0.f; // pixels scrolled down from the top of content
    bool pinnedToBottom_ = true; // auto-scroll to newest message while true

    static constexpr float kScrollStepPx = 40.f;
    static constexpr std::size_t kMaxLines = 5000; // cap to bound memory

    float messageHeight(const ChatMessage& message) const;
    std::vector<std::string> wrapText(const std::string& text) const;
    static std::string timeNow();
    static sf::Color avatarColor(const std::string& author);
};

} // namespace ui
