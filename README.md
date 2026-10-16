# Flitpad 📝

**Flitpad** is a lightweight, fast, and feature-rich text editor designed for developers and power users. Built from scratch in C++ using the FLTK (Fast Light Toolkit) library, it provides a clean, distraction-free coding environment with a modern dark theme inspired by Atom's One Dark.

---

## ✨ Features

* **Multi-Tab Interface:** Easily open and manage multiple files simultaneously with dedicated tabs.
* **Syntax Highlighting:** Real-time syntax coloring for C, C++, Python, JavaScript, HTML/XML, CSS, JSON, Markdown, and more.
* **File Browser Sidebar:** Navigate your project directory, open files, create new files, rename, or delete files directly from the sidebar.
* **Auto-Refresh Sidebar:** The sidebar automatically updates when files are saved or modified.
* **Quick Delete & Rename:** Select a file in the sidebar and press `Delete` to remove it from disk (with confirmation prompt), or press `F2` to rename.
* **Session Restoration:** Automatically remembers your open tabs and window size/position when you restart the app.
* **Find & Replace:** Full-featured search, replace, and replace-all capabilities with case-sensitivity options.
* **Status Bar:** Real-time line/column cursor tracking, character encoding, and active file path indicators.
* **Custom App Icon & Theme:** Custom embedded 32x32 application icon and a polished dark color scheme.

---

## 🛠️ Requirements & Dependencies

To compile and run Flitpad, you need:
* A C++ compiler supporting C++11 (`g++`, `clang++`)
* FLTK 1.3+ development libraries
* Cairo / X11 development headers (Linux)

---

## 🚀 Building from Source

1. Clone the repository:
   ```bash
   git clone https://github.com/simargl/flitpad.git
   cd flitpad
   ```

2. Compile using the provided Makefile:
   ```bash
   make
   ```

3. Run the application:
   ```bash
   ./flitpad
   ```

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| **Ctrl + N** | Open new file |
| **Ctrl + S** | Save current file |
| **Ctrl + W** | Close current tab |
| **Ctrl + Q** | Quit application |
| **Ctrl + F** | Show Find dialog |
| **Ctrl + H** | Show Replace dialog |
| **Ctrl + Tab** | Switch to next tab |
| **Ctrl + Shift + Tab** | Switch to previous tab |
| **F2** *(sidebar)* | Rename selected file |
| **Delete** *(sidebar)* | Delete selected file from disk |

---

## 📄 License

Distributed under the MIT License. See `LICENSE` for more information.
