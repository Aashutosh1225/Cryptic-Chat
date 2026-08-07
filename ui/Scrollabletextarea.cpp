#include "ScrollableTextArea.hpp"
#include <algorithm>

namespace ui {

ScrollableTextArea::ScrollableTextArea(const sf::Font& font, sf::Vector2f position, sf::Vector2f size)
    : font_(font), position_(position), size_(size)
{
    background_.setPosition(position);
    background_.setSize(size);
    background_.setFillColor(sf::Color(20, 20, 20));
    background_.setOutlineThickness(1.f);
    background_.setOutlineColor(sf::Color(100, 100, 100));

    // characterSize_ * 1.3 approximates SFML's default line spacing factor
    // for most fonts; good enough for a chat log without needing to
    // query Font::getLineSpacing() per line.
    lineHeight_ = static_cast<float>(characterSize_) * 1.3f;
}

void ScrollableTextArea::addLine(const std::string& line)
{
    lines_.push_back(line);
    while (lines_.size() > kMaxLines)
    {
        lines_.pop_front();
    }
    if (pinnedToBottom_)
    {
        scrollOffset_ = getMaxScroll();
    }
}

void ScrollableTextArea::clear()
{
    lines_.clear();
    scrollOffset_ = 0.f;
    pinnedToBottom_ = true;
}

float ScrollableTextArea::getMaxScroll() const
{
    float contentHeight = static_cast<float>(lines_.size()) * lineHeight_;
    return std::max(0.f, contentHeight - size_.y);
}

void ScrollableTextArea::scrollBy(float delta)
{
    float maxScroll = getMaxScroll();
    scrollOffset_ = std::clamp(scrollOffset_ + delta, 0.f, maxScroll);
    // If the user scrolls back to (or stays at) the very bottom, resume
    // auto-scrolling on new messages; otherwise they've scrolled up to
    // read history and new messages shouldn't yank them back down.
    pinnedToBottom_ = (scrollOffset_ >= maxScroll - 0.5f);
}

void ScrollableTextArea::handleEvent(const sf::Event& event)
{
    if (!visible_ || !enabled_) return;

    if (const auto* scrolled = event.getIf<sf::Event::MouseWheelScrolled>())
    {
        if (scrolled->wheel != sf::Mouse::Wheel::Vertical) return;

        sf::Vector2f mousePos(static_cast<float>(scrolled->position.x),
                               static_cast<float>(scrolled->position.y));
        sf::FloatRect bounds({position_.x, position_.y}, {size_.x, size_.y});
        if (!bounds.contains(mousePos)) return;

        // SFML: positive delta == wheel scrolled up/away from user, which
        // in a chat log should move the view toward older messages (up).
        scrollBy(-scrolled->delta * kScrollStepPx);
    }
}

void ScrollableTextArea::draw(sf::RenderTarget& target) const
{
    if (!visible_) return;

    target.draw(background_);

    sf::Vector2u targetSizePx = target.getSize();
    if (targetSizePx.x == 0 || targetSizePx.y == 0) return; // nothing to project onto

    sf::View savedView = target.getView();

    sf::View clipView({position_.x + size_.x / 2.f, position_.y + size_.y / 2.f + scrollOffset_},
                       {size_.x, size_.y});
    clipView.setViewport(sf::FloatRect(
        {position_.x / static_cast<float>(targetSizePx.x), position_.y / static_cast<float>(targetSizePx.y)},
        {size_.x / static_cast<float>(targetSizePx.x), size_.y / static_cast<float>(targetSizePx.y)}));
    target.setView(clipView);

    // Only draw lines whose row falls within the visible scrolled window --
    // avoids constructing/drawing thousands of off-screen sf::Text objects
    // once chat history grows long.
    float viewTop = scrollOffset_;
    float viewBottom = scrollOffset_ + size_.y;
    std::size_t firstVisible = static_cast<std::size_t>(std::max(0.f, viewTop / lineHeight_));
    std::size_t lastVisible = static_cast<std::size_t>(viewBottom / lineHeight_) + 1;
    lastVisible = std::min(lastVisible, lines_.empty() ? 0 : lines_.size() - 1);

    for (std::size_t i = firstVisible; i <= lastVisible && i < lines_.size(); ++i)
    {
        sf::Text text(font_, lines_[i], characterSize_);
        text.setFillColor(sf::Color::White);
        text.setPosition({position_.x + 6.f, position_.y + static_cast<float>(i) * lineHeight_});
        target.draw(text);
    }

    target.setView(savedView);
}

void ScrollableTextArea::setPosition(sf::Vector2f position)
{
    position_ = position;
    background_.setPosition(position);
}

sf::FloatRect ScrollableTextArea::getBounds() const
{
    return sf::FloatRect({position_.x, position_.y}, {size_.x, size_.y});
}

} // namespace ui
