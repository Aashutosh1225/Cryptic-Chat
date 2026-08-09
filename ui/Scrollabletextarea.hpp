#pragma once

#include "Widget.hpp"
#include <deque>
#include <string>

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
    void clear();
    std::size_t lineCount() const { return lines_.size(); }

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

    std::deque<std::string> lines_;
    unsigned int characterSize_ = 15;
    float lineHeight_;

    float scrollOffset_ = 0.f; // pixels scrolled down from the top of content
    bool pinnedToBottom_ = true; // auto-scroll to newest message while true

    static constexpr float kScrollStepPx = 40.f;
    static constexpr std::size_t kMaxLines = 5000; // cap to bound memory
};

} // namespace ui
