#ifndef NATIVE_THEME_H
#define NATIVE_THEME_H

class Fl_Window;

// Follows the operating system's light/dark preference for the window frame.
// Everything here does nothing on systems other than Windows.
namespace NativeTheme {

// True if Windows is set to dark mode for apps (Settings > Personalization > Colors >
// "Choose your default app mode"). Always false elsewhere.
bool system_prefers_dark();

// True the first time it is called and afterwards whenever the system preference
// has changed since the previous call.
bool preference_changed();

// Gives `window` a dark or light title bar to match the system preference. The window
// must already be shown. Safe to call repeatedly and for several windows.
void apply(Fl_Window* window);

}  // namespace NativeTheme

#endif
