# Phase 10: UI Widgets (Widget, Button, TextInputBox, ScrollableTextArea)

## Status: DONE — rewritten for SFML 3, verified two ways

## Why this phase was redone

Phase 10 was originally built and verified against **SFML 2.6.1** (downloaded
official prebuilt Windows binaries for the cross-compile check). When the
user tried building in their real MSYS2 environment, `pacman -S
mingw-w64-x86_64-sfml` installed **SFML 3.x**, and the 2.6.1 code failed to
compile with real errors. This document covers the SFML 3 rewrite.

To avoid repeating that mistake, this rewrite was checked against SFML 3
source directly, not against memory of the API or third-party blog posts:

```
git clone --depth 1 --branch 3.0.2 https://github.com/SFML/SFML.git
```

3.0.2 was confirmed to be a real, existing tag on the official SFML repo
before any code was written. Every API detail listed below was grep'd out
of the actual installed headers (`Rect.hpp`, `Event.hpp`, `Text.hpp`,
`Transformable.hpp`, `Mouse.hpp`, `Keyboard.hpp`, `View.hpp`,
`RenderTarget.hpp`) before being used, rather than assumed.

## Confirmed SFML 3 API surface used by these widgets

- `sf::Rect<T>` has public `.position` / `.size` members (both `Vector2<T>`),
  constructed as `Rect(Vector2<T> position, Vector2<T> size)`. No more
  `.left/.top/.width/.height`.
- `Transformable::setPosition(Vector2f)` / `setOrigin(Vector2f)` — single
  vector argument only, the two-float overloads are gone.
- `sf::Text` has **no default constructor**. Must be built with a font:
  `Text(const Font& font, String string = "", unsigned int characterSize = 30)`.
  Any class holding an `sf::Text` member must initialize it in the
  constructor's member-init list (all three widgets below do this).
- `sf::Font::openFromFile(...)` — `loadFromFile` was renamed.
- Event handling is the `std::optional<sf::Event>` + `getIf<T>()` pattern:
  ```cpp
  while (const std::optional<sf::Event> event = window.pollEvent())
      if (const auto* pressed = event->getIf<sf::Event::MouseButtonPressed>())
          ...
  ```
  Confirmed subtype field names from `Event.hpp`:
  - `TextEntered { char32_t unicode; }`
  - `MouseButtonPressed`/`Released` { `Mouse::Button button; Vector2i position;` }
  - `MouseWheelScrolled` { `Mouse::Wheel wheel; float delta; Vector2i position;` }
  - `MouseMoved` { `Vector2i position;` }
  - `KeyPressed`/`Released` { `Keyboard::Key code; Keyboard::Scancode scancode; bool alt/control/shift/system;` }
- `sf::Mouse::Wheel` is a scoped enum: `Wheel::Vertical` / `Wheel::Horizontal`
  (not the old `sf::Mouse::VerticalWheel`).
- `sf::Rect<T>::contains(Vector2<T> point)` — single-vector overload (the
  old two-float `contains(x, y)` overload doesn't exist in 3.x's public
  surface the way this code uses it).
- `RectangleShape::setSize(Vector2f)`, `RectangleShape(Vector2f size = {})`.
- `sf::View::setViewport(const FloatRect&)`, `setSize`, `setCenter` —
  unchanged from 2.6.1, used as-is for `ScrollableTextArea`'s clipping.

### A genuine (non-obvious) SFML behavior found via testing, not assumed

`Shape::getGlobalBounds()` **includes the outline thickness** — a
`RectangleShape` constructed at position `{100,100}` size `{80,30}` with a
1px outline reports global bounds `{99,99}` / `{82,32}`, not `{100,100}` /
`{80,30}`. This was confirmed with a standalone throwaway program
(`/tmp/boundstest.cpp` during development) before being written into the
test suite as an explicit, documented expectation — not silently patched
around. `Button::getBounds()` and `TextInputBox::getBounds()` both return
`getGlobalBounds()`, so callers (`ChatWindow` in Phase 11) should be aware
hit-testing bounds are outline-inclusive.

## Files

```
ui/Widget.hpp                Abstract base: handleEvent/update/draw/setPosition/getBounds
ui/Button.hpp / .cpp         Rectangular button, hover/press/click states, onClick callback
ui/TextInputBox.hpp / .cpp   Single-line text entry: focus, cursor, insert/backspace/delete,
                              arrow-key navigation, Enter-to-submit callback
ui/ScrollableTextArea.hpp/.cpp  Scrollable chat-log view; clips via a dedicated sf::View
                              whose viewport is set to the widget's on-screen rect; pins to
                              bottom on new messages unless the user has scrolled up to read
                              history (standard chat-app behavior)
ui/test_widgets.cpp          50 test cases across all three widgets + a real-RenderWindow
                              draw-call smoke test
```

`Widget::handleEvent` itself did **not** need to change — it still takes a
plain `const sf::Event&`. Only the *callers* (the eventual `ChatWindow`
render loop, and this phase's own test harness) needed to switch to the
`getIf<T>()` pattern when constructing/dispatching events.

## Design notes

- **TextInputBox** is ASCII-only for now (`isPrintableAscii` filters
  `TextEntered::unicode` to the 0x20–0x7E range). Full UTF-8 editing with
  correct multi-byte cursor arithmetic is flagged as future work — not
  needed for the login/chat use case where usernames/messages are
  overwhelmingly ASCII, and doing it properly (grapheme-aware cursor
  movement) is a distraction from the crypto/networking goals of this
  project.
- **TextInputBox** cursor x-position is computed by measuring a substring's
  `getLocalBounds()` width — good enough for the UI font used here; it is
  not kerning-perfect for arbitrary fonts, which is an acceptable
  simplification for a course project chat box.
- **ScrollableTextArea** caps stored history at 5000 lines
  (`kMaxLines`) to bound memory; older lines are dropped from the front.
  This is separate from the *persisted* chat history in the DB
  (`ChatHistoryRepository`, Phase 8) — the widget only bounds what's held
  in memory for scrolling, not what's stored.
- **Button** click firing uses standard press-inside/release-inside
  semantics: pressing down and dragging the mouse off the button before
  releasing does **not** fire `onClick` (verified in
  `test_widgets.cpp`) — this matches native UI toolkit behavior and
  prevents accidental sends.

## Build commands (MSYS2 MinGW64 shell, for the user's real environment)

```bash
g++ -std=c++17 -Wall -Wextra -I/mingw64/include \
    Button.cpp TextInputBox.cpp ScrollableTextArea.cpp test_widgets.cpp \
    -L/mingw64/lib -lsfml-graphics -lsfml-window -lsfml-system \
    -o test_widgets.exe
```

(No `-DSFML_STATIC` — `pacman`'s `mingw-w64-x86_64-sfml` package ships
shared libs, matching how this was cross-compiled and verified below.)

## Verification

### 1. Native Linux build (proves logic correctness)

SFML 3.0.2 was built from source for native Linux
(`-DSFML_BUILD_AUDIO=OFF -DSFML_BUILD_NETWORK=OFF`, shared libs) via CMake
+ Ninja, installed to a local prefix, and linked against directly — not
apt's SFML package, which is still 2.6.1 on this distro (confirmed via
`apt-cache policy libsfml-dev` → `2.6.1+dfsg-2build2`).

```
g++ -std=c++17 -Wall -Wextra -I<sfml-3-install>/include \
    Button.cpp TextInputBox.cpp ScrollableTextArea.cpp test_widgets.cpp \
    -L<sfml-3-install>/lib -lsfml-graphics -lsfml-window -lsfml-system \
    -Wl,-rpath,<sfml-3-install>/lib -o test_widgets
```

Compiled clean, zero warnings under `-Wall -Wextra`.

Run headlessly under a real Xvfb X server (`Xvfb :99 -screen 0
1024x768x24`), including a genuine `sf::RenderWindow` + real `draw()`
calls for all three widgets:

```
=== UI Widget Tests (SFML 3 API) ===

-- Button tests --
  [PASS] Button stores its label text
  [PASS] Button bounds position matches construction (offset by outline thickness)
  [PASS] Button bounds size matches construction (expanded by outline thickness)
  [PASS] Button starts unhovered
  [PASS] Mouse move inside bounds sets hovered
  [PASS] Mouse move outside bounds clears hovered
  [PASS] Press inside bounds sets pressed
  [PASS] Callback not fired on press alone
  [PASS] Press+release inside bounds fires callback
  [PASS] Pressed clears after release
  [PASS] Press inside + release outside does NOT fire callback
  [PASS] Right-click press does not set pressed state
  [PASS] Disabled button ignores clicks
  [PASS] setPosition updates bounds position

-- TextInputBox tests --
  [PASS] TextInputBox starts empty
  [PASS] TextInputBox starts unfocused
  [PASS] Click inside bounds focuses the box
  [PASS] Typed characters accumulate while focused
  [PASS] Backspace removes last character at end-of-text cursor
  [PASS] Delete at Home position removes first character
  [PASS] Character insertion happens at cursor position, not always at end
  [PASS] Click outside bounds unfocuses the box
  [PASS] Typed characters are ignored while unfocused
  [PASS] Enter key fires onSubmit callback
  [PASS] onSubmit callback receives current text
  [PASS] Text is NOT auto-cleared by Enter (caller's responsibility)
  [PASS] clear() empties the text
  [PASS] Text input respects maxLength
  [PASS] Non-printable control characters are filtered out of text entry

-- ScrollableTextArea tests --
  [PASS] ScrollableTextArea starts empty
  [PASS] No scroll needed when content fits viewport
  [PASS] Starts pinned to bottom
  [PASS] 50 lines were added
  [PASS] Enough lines overflow the viewport and require scrolling
  [PASS] Still pinned to bottom after adding lines while pinned
  [PASS] Pinned area auto-scrolls to max (newest) offset on new lines
  [PASS] Scrolling up away from the bottom un-pins auto-scroll
  [PASS] scrollBy(negative) actually moves the offset up
  [PASS] New line does not move scroll position while not pinned to bottom
  [PASS] Scrolling back to the bottom re-pins auto-scroll
  [PASS] Offset is clamped to max scroll, never overshoots
  [PASS] Offset is clamped to 0, never goes negative
  [PASS] Mouse wheel event outside widget bounds is ignored
  [PASS] Mouse wheel event inside widget bounds scrolls it (negative delta = down)
  [PASS] clear() empties the line list
  [PASS] clear() resets scroll offset
  [PASS] clear() resets pinned-to-bottom state

-- Draw-call smoke test (real sf::RenderWindow via Xvfb) --
  [PASS] Headless RenderWindow opens successfully under Xvfb
  [PASS] All three widgets draw() without throwing or crashing against a real RenderTarget
  [PASS] Window closes cleanly

=== 50/50 tests passed ===
```

Two intentional assertion fixes were needed during this run (documented
inline in `test_widgets.cpp`) once the outline-bounds behavior above was
discovered — not silent corrections, both are commented explaining why the
expected values include the 1px outline offset.

### 2. Windows cross-compile (proves it links & produces a real binary)

Per project convention, this was **not** assumed to work — SFML 3.0.2 was
cross-compiled from the *same* source clone using the real MinGW-w64
toolchain (`g++-mingw-w64-x86-64`, GCC 13.2.0, win32-threading variant),
producing genuine PE32+ DLLs:

```
$ file sfml-graphics-3.dll
sfml-graphics-3.dll: PE32+ executable (DLL) (console) x86-64, for MS Windows, 21 sections
```

(SFML's build also auto-fetched and cross-compiled its own vendored
FreeType dependency for MinGW as part of this — `libfreetype.a` — which is
statically linked into `sfml-graphics-3.dll`, matching how the official
SFML MinGW packages are built.)

The widget code + full test suite was then cross-compiled against these
real `.dll`/`.dll.a` import libraries:

```
x86_64-w64-mingw32-g++ -std=c++17 -Wall -Wextra -I<sfml-mingw-install>/include \
    Button.cpp TextInputBox.cpp ScrollableTextArea.cpp test_widgets.cpp \
    -L<sfml-mingw-install>/lib -lsfml-graphics -lsfml-window -lsfml-system \
    -o test_widgets.exe
```

Clean compile, zero warnings. Verified as a genuine Windows binary:

```
$ file test_widgets.exe
test_widgets.exe: PE32+ executable (console) x86-64, for MS Windows, 19 sections

$ objdump -p test_widgets.exe | grep "DLL Name"
	DLL Name: sfml-graphics-3.dll
	DLL Name: sfml-system-3.dll
	DLL Name: sfml-window-3.dll
	DLL Name: KERNEL32.dll
	DLL Name: msvcrt.dll
	DLL Name: libgcc_s_seh-1.dll
	DLL Name: libstdc++-6.dll
```

The three SFML DLLs plus the MinGW runtime DLLs (`libgcc_s_seh-1.dll`,
`libstdc++-6.dll`, `libwinpthread-1.dll`) are bundled in `deps-windows/`
alongside `test_widgets.exe` in this phase's zip so the binary is
self-contained.

### 3. Honest gap — what couldn't be verified in this sandbox

`test_widgets.exe` was **not** actually run on real Windows — this
sandbox can produce and statically inspect the PE32+ binary (file type,
import table, linkage) but cannot execute a Win32 GUI binary. The
draw-call and event-handling logic itself *was* fully exercised natively
on Linux (item 1 above, including a real windowing system via Xvfb), so
the only unverified-on-real-Windows risk is Windows-specific windowing/GL
context creation quirks (e.g. DPI scaling, WGL context creation edge
cases) — not the widget logic in this file, which is platform-agnostic
C++ against SFML's cross-platform API.
