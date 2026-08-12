#include "ScrollableTextArea.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace ui {

ScrollableTextArea::ScrollableTextArea(const sf::Font& font, sf::Vector2f position, sf::Vector2f size)
    : font_(font), position_(position), size_(size)
{
    background_.setPosition(position);
    background_.setSize(size);
    background_.setFillColor(sf::Color(30, 31, 35));
}

void ScrollableTextArea::addLine(const std::string& line)
{
    if (line.rfind("* ", 0) == 0 || line.rfind("[System]", 0) == 0) {
        addSystemMessage(line.rfind("* ", 0) == 0 ? line.substr(2) : line.substr(8));
        return;
    }
    const std::size_t separator = line.find(": ");
    if (separator != std::string::npos) {
        addMessage(line.substr(0, separator), line.substr(separator + 2));
        return;
    }
    addSystemMessage(line);
}

void ScrollableTextArea::addMessage(const std::string& author, const std::string& text, bool ownMessage)
{
    messages_.push_back({author, text, timeNow(), ownMessage, false});
    while (messages_.size() > kMaxLines) messages_.pop_front();
    if (pinnedToBottom_) scrollOffset_ = getMaxScroll();
}

void ScrollableTextArea::addSystemMessage(const std::string& text)
{
    messages_.push_back({"", text, "", false, true});
    while (messages_.size() > kMaxLines) messages_.pop_front();
    if (pinnedToBottom_) scrollOffset_ = getMaxScroll();
}

void ScrollableTextArea::clear()
{
    messages_.clear();
    scrollOffset_ = 0.f;
    pinnedToBottom_ = true;
}

float ScrollableTextArea::messageHeight(const ChatMessage& message) const
{
    if (message.system) return 34.f;
    return 58.f + static_cast<float>(wrapText(message.text).size() - 1) * lineHeight_;
}

float ScrollableTextArea::getMaxScroll() const
{
    float contentHeight = 14.f;
    for (const ChatMessage& message : messages_) contentHeight += messageHeight(message);
    return std::max(0.f, contentHeight - size_.y);
}

void ScrollableTextArea::scrollBy(float delta)
{
    float maxScroll = getMaxScroll();
    scrollOffset_ = std::clamp(scrollOffset_ + delta, 0.f, maxScroll);
    pinnedToBottom_ = (scrollOffset_ >= maxScroll - 0.5f);
}

void ScrollableTextArea::handleEvent(const sf::Event& event)
{
    if (!visible_ || !enabled_) return;
    if (const auto* scrolled = event.getIf<sf::Event::MouseWheelScrolled>()) {
        if (scrolled->wheel != sf::Mouse::Wheel::Vertical) return;
        sf::Vector2f mousePos(static_cast<float>(scrolled->position.x), static_cast<float>(scrolled->position.y));
        if (sf::FloatRect(position_, size_).contains(mousePos)) scrollBy(-scrolled->delta * kScrollStepPx);
    }
}

void ScrollableTextArea::draw(sf::RenderTarget& target) const
{
    if (!visible_) return;
    target.draw(background_);
    const sf::Vector2u targetSize = target.getSize();
    if (targetSize.x == 0 || targetSize.y == 0) return;

    const sf::View savedView = target.getView();
    sf::View clipView({position_.x + size_.x / 2.f, position_.y + size_.y / 2.f + scrollOffset_}, size_);
    clipView.setViewport(sf::FloatRect({position_.x / targetSize.x, position_.y / targetSize.y},
                                       {size_.x / targetSize.x, size_.y / targetSize.y}));
    target.setView(clipView);

    float y = position_.y + 14.f;
    for (const ChatMessage& message : messages_) {
        const float height = messageHeight(message);
        if (y + height >= position_.y + scrollOffset_ && y <= position_.y + scrollOffset_ + size_.y) {
            if (message.system) {
                sf::Text text(font_, message.text, 14);
                text.setFillColor(sf::Color(150, 157, 171));
                text.setStyle(sf::Text::Italic);
                text.setPosition({position_.x + 18.f, y + 6.f});
                target.draw(text);
            } else {
                sf::CircleShape avatar(20.f);
                avatar.setPosition({position_.x + 16.f, y + 4.f});
                avatar.setFillColor(avatarColor(message.author));
                target.draw(avatar);

                const std::string initials(1, static_cast<char>(std::toupper(static_cast<unsigned char>(message.author.empty() ? '?' : message.author[0]))));
                sf::Text initial(font_, initials, 18);
                initial.setFillColor(sf::Color::White);
                const sf::FloatRect initialBounds = initial.getLocalBounds();
                initial.setOrigin(initialBounds.position + initialBounds.size / 2.f);
                initial.setPosition({position_.x + 36.f, y + 24.f});
                target.draw(initial);

                sf::Text author(font_, message.author, 17);
                author.setFillColor(message.own ? sf::Color(91, 181, 255) : sf::Color(244, 245, 248));
                author.setPosition({position_.x + 66.f, y + 2.f});
                target.draw(author);

                const sf::FloatRect authorBounds = author.getLocalBounds();
                sf::Text timestamp(font_, message.timestamp, 12);
                timestamp.setFillColor(sf::Color(151, 156, 168));
                timestamp.setPosition({position_.x + 72.f + authorBounds.size.x, y + 7.f});
                target.draw(timestamp);

                const std::vector<std::string> wrapped = wrapText(message.text);
                for (std::size_t index = 0; index < wrapped.size(); ++index) {
                    sf::Text body(font_, wrapped[index], characterSize_);
                    body.setFillColor(sf::Color(220, 221, 222));
                    body.setPosition({position_.x + 66.f, y + 26.f + static_cast<float>(index) * lineHeight_});
                    target.draw(body);
                }
            }
        }
        y += height;
    }
    target.setView(savedView);
}

std::vector<std::string> ScrollableTextArea::wrapText(const std::string& text) const
{
    constexpr std::size_t kCharactersPerLine = 82;
    std::vector<std::string> lines;
    std::istringstream words(text);
    std::string word, line;
    while (words >> word) {
        if (!line.empty() && line.size() + word.size() + 1 > kCharactersPerLine) {
            lines.push_back(line);
            line.clear();
        }
        if (!line.empty()) line += ' ';
        line += word;
    }
    if (!line.empty()) lines.push_back(line);
    if (lines.empty()) lines.push_back("");
    return lines;
}

std::string ScrollableTextArea::timeNow()
{
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream out;
    out << std::put_time(&local, "%H:%M");
    return out.str();
}

sf::Color ScrollableTextArea::avatarColor(const std::string& author)
{
    const std::size_t hash = std::hash<std::string>{}(author);
    const sf::Color colors[] = {sf::Color(88, 101, 242), sf::Color(87, 159, 99), sf::Color(222, 99, 99), sf::Color(196, 123, 61), sf::Color(142, 88, 198)};
    return colors[hash % (sizeof(colors) / sizeof(colors[0]))];
}

void ScrollableTextArea::setPosition(sf::Vector2f position)
{
    position_ = position;
    background_.setPosition(position);
}

sf::FloatRect ScrollableTextArea::getBounds() const
{
    return sf::FloatRect(position_, size_);
}

} // namespace ui
