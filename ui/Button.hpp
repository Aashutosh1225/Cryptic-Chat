#pragma once

#include "Widget.hpp"
#include <functional>
#include <string>

namespace ui {

// A clickable rectangular button with a centered text label.
class Button : public Widget {
public:
    // font must outlive the Button (sf::Text in SFML 3 has no default
    // constructor and holds a reference to its font internally).
    Button(const sf::Font& font, const std::string& label, sf::Vector2f position, sf::Vector2f size);

    void setOnClick(std::function<void()> callback);

    void handleEvent(const sf::Event& event) override;
    void draw(sf::RenderTarget& target) const override;
    void setPosition(sf::Vector2f position) override;
    sf::FloatRect getBounds() const override;

    void setLabel(const std::string& text);
    const std::string& getLabel() const { return labelText_; }
    void setPrimary(bool primary);

    bool isHovered() const { return hovered_; }
    bool isPressed() const { return pressed_; }

private:
    sf::RectangleShape shape_;
    sf::Text label_;
    std::string labelText_;
    std::function<void()> onClick_;
    bool hovered_ = false;
    bool pressed_ = false;
    bool primary_ = false;

    void centerLabel();
    void updateColors();
};

} // namespace ui
