#pragma once

#include <SFML/Graphics.hpp>

namespace ui {

// Abstract base for all UI widgets.
//
// SFML 3 note: the event-dispatch pattern changed (see README_phase10.md),
// but that change lives entirely in the *callers* of handleEvent() -- the
// caller now does:
//
//     while (const std::optional<sf::Event> event = window.pollEvent())
//         widget.handleEvent(*event);
//
// and passes the dereferenced sf::Event by reference, same as before.
// Widget itself doesn't need to know about std::optional<sf::Event> or
// pollEvent() at all -- it just receives a const sf::Event& and uses
// event.getIf<T>() internally to check which subtype it is. So this
// interface is unchanged from the SFML 2.6.1 version.
class Widget {
public:
    virtual ~Widget() = default;

    // Inspect the event and react if relevant (mouse move/click, key/text
    // entry, scroll, etc). Implementations use event.getIf<sf::Event::X>()
    // to narrow the event type.
    virtual void handleEvent(const sf::Event& event) = 0;

    // Per-frame logic that isn't event-driven (e.g. cursor blink timers).
    // Default no-op so simple widgets don't need to override it.
    virtual void update() {}

    virtual void draw(sf::RenderTarget& target) const = 0;

    virtual void setPosition(sf::Vector2f position) = 0;
    virtual sf::FloatRect getBounds() const = 0;

    void setVisible(bool visible) { visible_ = visible; }
    bool isVisible() const { return visible_; }

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

protected:
    bool visible_ = true;
    bool enabled_ = true;
};

} // namespace ui
