# Flitpad 📝

**Flitpad** is a lightweight, fast text editor for developers, written in C++ with FLTK. Its default look is the dark **Monokai** colour scheme, and you can switch to any of six built-in schemes, light or dark.

---

## ✨ Features

* **Multi-tab editing** – open several files at once; middle-click a tab to close it.
* **Syntax highlighting** for C, C++, C#, Java, JavaScript, TypeScript, Go, Rust, Python, Nim, PHP, SQL, Shell, Makefile, CMake, HTML/XML/SVG (with embedded `<script>`/`<style>`), CSS/SCSS, JSON, YAML, TOML, INI, diff and **Markdown**.
* **Markdown highlighting** – headings (ATX and setext), **bold**, *italic*, ~~strikethrough~~, `inline code`, fenced code blocks highlighted in their own language (` ```cpp `), links, images, bare URLs, lists and task lists, block quotes, tables, horizontal rules, HTML comments and YAML front matter. Markdown and plain-text files soft-wrap.
* **Right-click menu** – just the essentials: Undo, Cut/Copy/Paste, Select All, Toggle Comment and **Theme...** (entries that need a selection are greyed out otherwise). Right-click moves the caret unless you click inside the selection. Everything else has a shortcut (see below).
* **Sidebar menu** (right-click) – New File, Rename, Delete, Copy Path, Refresh.
* **Tab menu** (right-click a tab) – Close, Close Others, Close All, Copy Full Path. Middle-click closes a tab.
* **Smart editing** – auto-indent, extra indent after `{ ( [` (and `:` in Python/YAML), `{|}` splits into an indented block, Tab/Shift+Tab indent and outdent whole selections, Markdown lists/quotes continue on Enter (Enter on an empty item ends the list), comment toggle for every language that has line comments.
* **File browser sidebar** – open, create, rename (`F2`) and delete (`Delete`) files; `../` navigates up. Keys only act on the sidebar while it has focus.
* **Find & Replace** – Enter = find next, `F3` / `Shift+F3` repeat the search, the dialog is pre-filled from your selection, Replace All is a single fast edit.
* **Symlink-safe** – a file is identified by its real path, so opening it through a symlinked directory (or a symlink to the file) never creates a second tab; the same applies to session restore.
* **Safe file handling** – CRLF files stay CRLF, binary files are refused, new files are saved under their own name (`untitled`, `untitled_1`, ...) without a dialog and never overwrite an existing file (use Save As to choose a name), unsaved-changes prompts on close/quit.
* **Session restoration** – tabs, active file and window geometry come back after a restart.
* **Reloads on outside changes** – if another program changes the open file, Flitpad reloads it (or asks first when you have unsaved edits).
* **Drag and drop** – drop files (or a folder) onto the editor or the sidebar to open them.
* **Status bar** – line/column, selection size, line ending, language and path; follows the cursor on every key and click.
* **Dark title bar on Windows** – when Windows is set to dark mode for apps (Settings > Personalization > Colors), the window frame of Flitpad and its dialogs is dark too, and follows the setting while Flitpad is running. (Windows 10 version 2004 or later.)
* **Window title** – shows the file name and a `*` while there are unsaved changes.
* **Single instance** – opening a file from a file manager hands it to the running window and raises it.
* **Themes** – right-click in the editor and choose *Theme...* to pick a colour scheme; it previews as you select, Cancel restores the old one. Built-in: Monokai (the default), Oblivion, Solarized Dark, Tomorrow Night, Solarized Light, Tomorrow.
* **Ctrl+Tab switcher** – a list of the open files in the middle of the window, in tab order, with full paths. Keep Ctrl held: Tab moves down, Shift+Tab moves up (the first press already moves from the current file). Release Ctrl to switch to the selected file, press Esc to cancel, or click an entry.
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
| **Ctrl + S** | Save (new files are saved as `untitled` etc., no dialog) |
| **Ctrl + Shift + S** | Save As |
| **Ctrl + W** / middle-click tab | Close tab |
| **Ctrl + Q** | Quit |
| **Ctrl + F** / **Ctrl + H** | Find / Replace |
| **F3** / **Shift + F3** | Find next / previous |
| **Ctrl + G** | Go to line |
| **Ctrl + /** | Toggle line comment |
| **Ctrl + D** / **Ctrl + K** | Duplicate / delete line(s) |
| **Alt + Up** / **Alt + Down** | Move line(s) up / down |
| **Tab** / **Shift + Tab** | Indent / outdent (selection aware) |
| **Ctrl + Tab** / **Ctrl + Shift + Tab** | Open the file switcher and move down / up; release Ctrl to switch |
| **Ctrl + =** / **Ctrl + -** / **Ctrl + 0** | Zoom in / out / reset |
| **F2** *(sidebar)* | Rename selected file |
| **Delete** *(sidebar)* | Delete selected file |

---

## ⚙️ Settings and session: `~/.flitpad_session`

Everything Flitpad remembers is in this one file, rewritten when you quit. It has two tags:

```
[settings]
active_file=/home/me/notes.md
dark_scheme=monokai
font_name=Courier
font_size=17
light_scheme=solarized-light
theme=dark
window=100,100,1200,800

[recent]
/home/me/project/main.cpp
/home/me/notes.md
```

`[recent]` lists the files that were open when you quit, in tab order; they are reopened on the next start (files that no longer exist are skipped, and new unsaved files are not stored).

| Key | Meaning |
| :--- | :--- |
| `theme` | `dark` or `light`; set by the *Theme...* dialog |
| `dark_scheme` | `monokai`, `oblivion`, `solarized-dark` or `tomorrow-night`; set by the *Theme...* dialog |
| `light_scheme` | `solarized-light` or `tomorrow`; set by the *Theme...* dialog |
| `font_name` | Font family, e.g. `DejaVu Sans Mono` (default `Courier`). Edit it while Flitpad is closed; an unknown name falls back to Courier and is kept. |
| `font_size` | 9–40, changed with `Ctrl +` / `Ctrl -` |
| `window` | `x,y,width,height` |
| `active_file` | The tab that was selected |
| `icon_font` | *(optional)* exact name of the font for icon glyphs, e.g. `Font Awesome 6 Free` |

Older files are migrated automatically: the previous `~/.flitpad_session` format and `~/.flitpad_config` (which is removed after its values have been folded in). The two small files `~/.flitpad_lock` and `~/.flitpad_commands` are not settings: they let a second `flitpad file.txt` hand the file to the running window.

Icon glyphs: without `icon_font`, Flitpad picks the first installed of FontAwesome 4, Font Awesome 5/6 Free (Solid), Symbols Nerd Font, any "… Nerd Font", the first time a file with icon glyphs is shown (the status bar says which). This needs an FLTK build with Xft, the default on most Linux distributions.

## 🎨 Colour schemes

The schemes are in `ColorSchemes.cxx` (each uses the published palette of its scheme). A scheme gives each syntax role a colour: keywords, strings, numbers, functions, types, preprocessor, escapes, operators, comments, plus the Markdown accents; add one by adding an entry to the table.

**Why some colours are slightly different from the scheme's values:** FLTK's text widget replaces any style colour whose luminance is too close to the background's with plain white (or black on a light background). `ColorMath::ensure_contrast()` keeps each hue and moves it just far enough to survive; colours that are already fine are untouched.

Not implemented (FLTK 1.3 has no support for them): current-line highlight, bracket matching colours, and the scheme's selection *text* colour.

## 🧱 Source layout

| File | Purpose |
| :--- | :--- |
| `TextEditorApp.*` | Main window, tabs, sidebar, file I/O, key handling |
| `Highlighter.*` | Language detection and syntax highlighting (toolkit independent) |
| `EditHelpers.*` | Auto-indent, list continuation, comment toggle (toolkit independent) |
| `Theme.h`, `ColorSchemes.*`, `ColorMath.h` | The active colour scheme, the scheme table, contrast maths (all but `Theme.h` toolkit independent) |
| `SessionFile.*`, `Settings.*` | `~/.flitpad_session`: `[settings]` and `[recent]` (toolkit independent) |
| `IconFont.*` | Icon-font selection (toolkit independent) |
| `TabSwitcher.h`, `SchemeDialog.*` | The Ctrl+Tab list; the Theme... dialog |
| `NativeTheme.*` | Dark title bar from the Windows app-mode setting (does nothing on other systems) |
| `TextFile.*` | Reading/writing text files: CRLF, binary and size checks, modification time (toolkit independent) |
| `PathUtil.*` | Canonical (symlink-resolved) file identity (toolkit independent) |
| `FindReplaceDialog.*` | Find / Replace window |
| `CustomTabs.h`, `TabData.h` | Tab widget and per-tab state |
| `tests/` | `make test`; `tests/show_styles` prints highlighter output for a file |

## 📄 License

Distributed under the MIT License. See `LICENSE` for more information.
