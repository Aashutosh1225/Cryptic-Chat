#include "Button.hpp"

namespace ui {

Button::Button(const sf::Font& font, const std::string& label, sf::Vector2f position, sf::Vector2f size)
    : label_(font, label, 18)
{
    shape_.setPosition(position);
    shape_.setSize(size);
    shape_.setFillColor(sf::Color(70, 70, 70));
    shape_.setOutlineThickness(1.f);
    shape_.setOutlineColor(sf::Color(130, 130, 130));

    label_.setCharacterSize(18);
    label_.setFillColor(sf::Color::White);

    setLabel(label);
}

void Button::setLabel(const std::string& text)
{
    labelText_ = text;
    label_.setString(text);
    centerLabel();
}

void Button::setPrimary(bool primary)
{
    primary_ = primary;
    updateColors();
}

void Button::centerLabel()
{
    // Center the text's local bounds on the button's rectangle.
    sf::FloatRect textBounds = label_.getLocalBounds();
    label_.setOrigin(textBounds.position + textBounds.size / 2.f);
    label_.setPosition(shape_.getPosition() + shape_.getSize() / 2.f);
}

void Button::setOnClick(std::function<void()> callback)
{
    onClick_ = std::move(callback);
}

void Button::handleEvent(const sf::Event& event)
{
    if (!visible_ || !enabled_) return;

    if (const auto* moved = event.getIf<sf::Event::MouseMoved>())
    {
        sf::Vector2f mousePos(static_cast<float>(moved->position.x),
                               static_cast<float>(moved->position.y));
        hovered_ = shape_.getGlobalBounds().contains(mousePos);
        updateColors();
        return;
    }

    if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (pressed->button != sf::Mouse::Button::Left) return;
        sf::Vector2f mousePos(static_cast<float>(pressed->position.x),
                               static_cast<float>(pressed->position.y));
        if (shape_.getGlobalBounds().contains(mousePos))
        {
            pressed_ = true;
            updateColors();
        }
        return;
    }

    if (const auto* released = event.getIf<sf::Event::MouseButtonReleased>())
    {
        if (released->button != sf::Mouse::Button::Left) return;
        if (!pressed_) return;

        sf::Vector2f mousePos(static_cast<float>(released->position.x),
                               static_cast<float>(released->position.y));
        pressed_ = false;
        updateColors();

        // Only fire the click if the release happened back inside the
        // button (standard press-drag-release-outside == no click).
        if (shape_.getGlobalBounds().contains(mousePos) && onClick_)
        {
            onClick_();
        }
        return;
    }
}

void Button::draw(sf::RenderTarget& target) const
{
    if (!visible_) return;
    target.draw(shape_);
    target.draw(label_);
}

void Button::setPosition(sf::Vector2f position)
{
    shape_.setPosition(position);
    centerLabel();
}

sf::FloatRect Button::getBounds() const
{
    return shape_.getGlobalBounds();
}

void Button::updateColors()
{
    if (!enabled_)
        shape_.setFillColor(sf::Color(50, 50, 50));
    else if (primary_ && pressed_)
        shape_.setFillColor(sf::Color(34, 107, 184));
    else if (primary_ && hovered_)
        shape_.setFillColor(sf::Color(76, 157, 238));
    else if (primary_)
        shape_.setFillColor(sf::Color(52, 132, 220));
    else if (pressed_)
        shape_.setFillColor(sf::Color(40, 50, 68));
    else if (hovered_)
        shape_.setFillColor(sf::Color(65, 78, 102));
    else
        shape_.setFillColor(sf::Color(45, 56, 76));
}

} // namespace ui
