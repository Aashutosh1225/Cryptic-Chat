#pragma once

#include "Widget.hpp"
#include <SFML/System/Clock.hpp>
#include <functional>
#include <string>

namespace ui {

// A single-line text entry box: click to focus, type to enter text,
// Backspace/Delete/Left/Right/Home/End for editing, Enter to submit.
class TextInputBox : public Widget {
public:
    TextInputBox(const sf::Font& font, sf::Vector2f position, sf::Vector2f size);

    // Fired when Enter is pressed while focused. Does NOT clear the text
    // automatically -- the caller decides (e.g. clear() after reading it).
    void setOnSubmit(std::function<void(const std::string&)> callback);

    void handleEvent(const sf::Event& event) override;
    void update() override; // advances the cursor blink timer
    void draw(sf::RenderTarget& target) const override;
    void setPosition(sf::Vector2f position) override;
    sf::FloatRect getBounds() const override;

    const std::string& getText() const { return text_; }
    void setText(const std::string& text);
    void clear();

    bool isFocused() const { return focused_; }
    void setFocused(bool focused);

    void setMaxLength(std::size_t maxLength) { maxLength_ = maxLength; }
    void setPlaceholder(const std::string& placeholder) { placeholder_ = placeholder; }
    void setPasswordMode(bool passwordMode);

private:
    sf::RectangleShape box_;
    sf::Text display_;      // shows text_, or placeholder_ when empty+unfocused
    sf::RectangleShape cursor_;
    std::string text_;
    std::string placeholder_;
    std::size_t cursorPos_ = 0;    // index into text_ (UTF-8 byte offset; ASCII-only for now)
    std::size_t maxLength_ = 512;
    bool focused_ = false;
    bool passwordMode_ = false;

    sf::Clock blinkClock_;  // real-time clock; toggles cursorVisible_ every ~0.5s
    bool cursorVisible_ = true;

    std::function<void(const std::string&)> onSubmit_;

    void refreshDisplay();
    void updateCursorShape();
    static bool isPrintableAscii(char32_t unicode);
};

} // namespace ui
