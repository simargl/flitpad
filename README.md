# Flitpad 📝

**Flitpad** is a lightweight, fast text editor for developers, written in C++ with FLTK. It uses the colours of GtkSourceView's **`solarized-dark`** scheme.

---

## ✨ Features

* **Multi-tab editing** – open several files at once; middle-click a tab to close it.
* **Syntax highlighting** for C, C++, C#, Java, JavaScript, TypeScript, Go, Rust, Python, Nim, PHP, SQL, Shell, Makefile, CMake, HTML/XML/SVG (with embedded `<script>`/`<style>`), CSS/SCSS, JSON, YAML, TOML, INI, diff and **Markdown**.
* **Markdown highlighting** – headings (ATX and setext), **bold**, *italic*, ~~strikethrough~~, `inline code`, fenced code blocks highlighted in their own language (` ```cpp `), links, images, bare URLs, lists and task lists, block quotes, tables, horizontal rules, HTML comments and YAML front matter. Markdown and plain-text files soft-wrap.
* **Smart editing** – auto-indent, extra indent after `{ ( [` (and `:` in Python/YAML), `{|}` splits into an indented block, Tab/Shift+Tab indent and outdent whole selections, Markdown lists/quotes continue on Enter (Enter on an empty item ends the list), comment toggle for every language that has line comments.
* **File browser sidebar** – open, create, rename (`F2`) and delete (`Delete`) files; `../` navigates up. Keys only act on the sidebar while it has focus.
* **Find & Replace** – Enter = find next, `F3` / `Shift+F3` repeat the search, the dialog is pre-filled from your selection, Replace All is a single fast edit.
* **Symlink-safe** – a file is identified by its real path, so opening it through a symlinked directory (or a symlink to the file) never creates a second tab; the same applies to session restore.
* **Safe file handling** – CRLF files stay CRLF, binary files are refused, new files ask for a name on first save, unsaved-changes prompts on close/quit.
* **Session restoration** – tabs, active file and window geometry come back after a restart.
* **Status bar** – line/column, selection size, line ending, language and path; follows the cursor on every key and click.
* **Single instance** – opening a file from a file manager hands it to the running window and raises it.
* **Zoom** – `Ctrl +` / `Ctrl -` / `Ctrl 0`. The font size is saved on exit and restored on start.
* **Icon glyphs** – FontAwesome and Nerd Font icons (Unicode private-use characters) are drawn with an installed icon font, in the colour of the surrounding syntax.

---

## 🛠️ Requirements

* A C++11 compiler (`g++`, `clang++`)
* FLTK 1.3+ development libraries (`fltk-config` in `PATH`)
* Cairo / X11 development headers (Linux)

## 🚀 Building

```bash
git clone https://github.com/simargl/flitpad.git
cd flitpad
make            # build ./flitpad
make test       # unit tests for the highlighter and editing logic (no FLTK needed)
sudo make install
```

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| **Ctrl + N** | New file |
| **Ctrl + O** | Open file(s) |
| **Ctrl + S** | Save (asks for a name for new files) |
| **Ctrl + Shift + S** | Save As |
| **Ctrl + W** / middle-click tab | Close tab |
| **Ctrl + Q** | Quit |
| **Ctrl + F** / **Ctrl + H** | Find / Replace |
| **F3** / **Shift + F3** | Find next / previous |
| **Ctrl + G** | Go to line |
| **Ctrl + /** | Toggle line comment |
| **Tab** / **Shift + Tab** | Indent / outdent (selection aware) |
| **Ctrl + Tab** / **Ctrl + Shift + Tab** | Next / previous tab |
| **Ctrl + =** / **Ctrl + -** / **Ctrl + 0** | Zoom in / out / reset |
| **F2** *(sidebar)* | Rename selected file |
| **Delete** *(sidebar)* | Delete selected file |

---

## ⚙️ Settings

`~/.flitpad_config` is a plain `key=value` file, written on exit:

| Key | Meaning |
| :--- | :--- |
| `font_size` | Editor font size (9–40), saved automatically |
| `icon_font` | *(optional)* exact name of the font to use for icon glyphs, e.g. `icon_font=Font Awesome 6 Free` |

Without `icon_font`, Flitpad picks the first installed of: FontAwesome 4, Font Awesome 5/6 Free (Solid), Symbols Nerd Font, any "… Nerd Font". The font is looked up the first time a file containing icon glyphs is shown; the status bar tells you which one was chosen. This needs an FLTK build with Xft (the default on most Linux distributions).

## 🎨 Changing the theme

All colours are in `Theme.h` (palette + a few role functions) and the style table at the top of `TextEditorApp.cxx`.

How the scheme's roles map onto the highlighter: `def:statement` (orange) = keywords, `def:type` (yellow) = type names and variables, `def:identifier` (blue) = functions and object keys, `def:constant` (cyan) = strings, `def:preprocessor` (violet) = `#include`, decorators, annotations, `def:special-char` / `def:operator` (green) = escape sequences and operators, `def:comment` (base01, italic) = comments. Numbers and `true`/`null`/`None` are magenta.

Markdown (not defined by the scheme) uses the rest of the palette: a different colour per heading level (orange, yellow, green, cyan, blue, violet), magenta bold, green italic, yellow code, blue link text with cyan destinations, red list/quote/table markers, violet block quotes.

**Why some colours are slightly lighter than the scheme's values:** FLTK's text widget replaces any style colour whose luminance is too close to the background's with plain white. On Solarized's very dark `base03`, that would turn orange, blue, cyan, violet, magenta, red and the comment grey all white. `Theme::syntax()` (see `ColorMath.h`) keeps each hue and brightens it just enough to survive; colours that are already bright enough are untouched.

Not implemented (FLTK 1.3 has no support for them): current-line highlight, bracket matching colours, and the scheme's selection *text* colour.

## 🧱 Source layout

| File | Purpose |
| :--- | :--- |
| `TextEditorApp.*` | Main window, tabs, sidebar, file I/O, key handling |
| `Highlighter.*` | Language detection and syntax highlighting (toolkit independent) |
| `EditHelpers.*` | Auto-indent, list continuation, comment toggle (toolkit independent) |
| `Theme.h` | Colour palette (Solarized Dark) |
| `Settings.*`, `IconFont.*` | `~/.flitpad_config` handling; icon-font selection (toolkit independent) |
| `PathUtil.*` | Canonical (symlink-resolved) file identity (toolkit independent) |
| `FindReplaceDialog.*` | Find / Replace window |
| `CustomTabs.h`, `TabData.h` | Tab widget and per-tab state |
| `tests/` | `make test`; `tests/show_styles` prints highlighter output for a file |

## 📄 License

Distributed under the MIT License. See `LICENSE` for more information.
