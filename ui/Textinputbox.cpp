#include "TextInputBox.hpp"

namespace ui {

TextInputBox::TextInputBox(const sf::Font& font, sf::Vector2f position, sf::Vector2f size)
    : display_(font, "", 16)
{
    box_.setPosition(position);
    box_.setSize(size);
    box_.setFillColor(sf::Color(30, 30, 30));
    box_.setOutlineThickness(1.f);
    box_.setOutlineColor(sf::Color(100, 100, 100));

    display_.setCharacterSize(16);
    display_.setFillColor(sf::Color::White);
    display_.setPosition({position.x + 6.f, position.y + (size.y - 16.f) / 2.f - 2.f});

    cursor_.setSize({1.f, static_cast<float>(display_.getCharacterSize())});
    cursor_.setFillColor(sf::Color::White);

    refreshDisplay();
}

void TextInputBox::setOnSubmit(std::function<void(const std::string&)> callback)
{
    onSubmit_ = std::move(callback);
}

void TextInputBox::setText(const std::string& text)
{
    text_ = text.substr(0, maxLength_);
    cursorPos_ = text_.size();
    refreshDisplay();
}

void TextInputBox::clear()
{
    text_.clear();
    cursorPos_ = 0;
    refreshDisplay();
}

void TextInputBox::setFocused(bool focused)
{
    focused_ = focused;
    cursorVisible_ = true;
    blinkClock_.restart();
    box_.setOutlineColor(focused_ ? sf::Color(80, 160, 255) : sf::Color(100, 100, 100));
    refreshDisplay();
}

bool TextInputBox::isPrintableAscii(char32_t unicode)
{
    // Printable ASCII range, excluding DEL. Deliberately ASCII-only for now
    // -- full UTF-8 editing (multi-byte cursor movement) is future work.
    return unicode >= 0x20 && unicode < 0x7F;
}

void TextInputBox::handleEvent(const sf::Event& event)
{
    if (!visible_ || !enabled_) return;

    if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (pressed->button != sf::Mouse::Button::Left) return;
        sf::Vector2f mousePos(static_cast<float>(pressed->position.x),
                               static_cast<float>(pressed->position.y));
        setFocused(box_.getGlobalBounds().contains(mousePos));
        return;
    }

    if (!focused_) return;

    if (const auto* textEntered = event.getIf<sf::Event::TextEntered>())
    {
        if (isPrintableAscii(textEntered->unicode) && text_.size() < maxLength_)
        {
            text_.insert(text_.begin() + static_cast<long>(cursorPos_),
                         static_cast<char>(textEntered->unicode));
            ++cursorPos_;
            cursorVisible_ = true;
            blinkClock_.restart();
            refreshDisplay();
        }
        return;
    }

    if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>())
    {
        using Key = sf::Keyboard::Key;
        switch (keyPressed->code)
        {
            case Key::Backspace:
                if (cursorPos_ > 0)
                {
                    text_.erase(text_.begin() + static_cast<long>(cursorPos_) - 1);
                    --cursorPos_;
                    refreshDisplay();
                }
                break;
            case Key::Delete:
                if (cursorPos_ < text_.size())
                {
                    text_.erase(text_.begin() + static_cast<long>(cursorPos_));
                    refreshDisplay();
                }
                break;
            case Key::Left:
                if (cursorPos_ > 0) --cursorPos_;
                updateCursorShape();
                break;
            case Key::Right:
                if (cursorPos_ < text_.size()) ++cursorPos_;
                updateCursorShape();
                break;
            case Key::Home:
                cursorPos_ = 0;
                updateCursorShape();
                break;
            case Key::End:
                cursorPos_ = text_.size();
                updateCursorShape();
                break;
            case Key::Enter:
                if (onSubmit_) onSubmit_(text_);
                break;
            default:
                break;
        }
        cursorVisible_ = true;
        blinkClock_.restart();
        return;
    }
}

void TextInputBox::update()
{
    // Blink the cursor every 0.5s of real time while focused.
    if (blinkClock_.getElapsedTime().asSeconds() >= 0.5f)
    {
        cursorVisible_ = !cursorVisible_;
        blinkClock_.restart();
    }
}

void TextInputBox::refreshDisplay()
{
    if (text_.empty() && !focused_)
    {
        display_.setString(placeholder_);
        display_.setFillColor(sf::Color(140, 140, 140));
    }
    else
    {
        display_.setString(text_);
        display_.setFillColor(sf::Color::White);
    }
    updateCursorShape();
}

void TextInputBox::updateCursorShape()
{
    // Measure the width of the text up to cursorPos_ by finding the glyph
    // position sf::Text would place there: build the substring and read
    // its local bounds width (simple and correct for a monospace-ish UI
    // font; kerning-perfect placement isn't needed for a chat input box).
    sf::Text measure = display_;
    measure.setString(text_.substr(0, cursorPos_));
    float textWidth = measure.getLocalBounds().size.x;
    if (!text_.substr(0, cursorPos_).empty())
    {
        // getLocalBounds() on SFML doesn't include trailing advance width,
        // so nudge using the full string advance instead when at the end.
        textWidth = measure.getLocalBounds().position.x + measure.getLocalBounds().size.x;
    }

    sf::Vector2f boxPos = box_.getPosition();
    cursor_.setPosition({boxPos.x + 6.f + textWidth, boxPos.y + 4.f});
    cursor_.setSize({1.f, box_.getSize().y - 8.f});
}

void TextInputBox::draw(sf::RenderTarget& target) const
{
    if (!visible_) return;
    target.draw(box_);
    target.draw(display_);
    if (focused_ && cursorVisible_)
    {
        target.draw(cursor_);
    }
}

void TextInputBox::setPosition(sf::Vector2f position)
{
    sf::Vector2f delta = position - box_.getPosition();
    box_.setPosition(position);
    display_.setPosition(display_.getPosition() + delta);
    updateCursorShape();
}

sf::FloatRect TextInputBox::getBounds() const
{
    return box_.getGlobalBounds();
}

} // namespace ui
