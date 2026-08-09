#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

#include "concurrency/MessageQueue.hpp"
#include "ui/Button.hpp"
#include "ui/ChatWindow.hpp"
#include "ui/ScrollableTextArea.hpp"
#include "ui/TextInputBox.hpp"

namespace {

sf::Font loadFont()
{
    sf::Font font;

    const std::vector<std::string> candidates = {
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf"
    };

    for (const std::string& path : candidates)
    {
        if (font.openFromFile(path))
        {
            return font;
        }
    }

    throw std::runtime_error("Unable to load a font for the UI test.");
}

} // namespace

int main()
{
    try
    {
        sf::Font font = loadFont();
        concurrency::MessageQueue<std::string> incoming;

        ui::Button button(font, "Test", {10.f, 10.f}, {90.f, 28.f});
        ui::TextInputBox input(font, {10.f, 50.f}, {220.f, 28.f});
        ui::ScrollableTextArea area(font, {10.f, 90.f}, {300.f, 120.f});
        ui::ChatWindow window("UI Test", {420u, 320u}, font, incoming);

        button.setOnClick([] {});
        input.setPlaceholder("Type here...");
        input.setFocused(true);
        area.addLine("Hello from the test harness");
        window.appendSystemMessage("System message");
        window.setStatus("Ready");

        std::cout << "All UI components were instantiated successfully." << std::endl;
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::string userInput;
        std::getline(std::cin, userInput);
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "UI test failed: " << ex.what() << std::endl;
        return 1;
    }
}
