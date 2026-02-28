# Terminal Text Editor (C++ + ncurses)

A lightweight terminal-based text editor built from scratch in C++ using ncurses.

This project is designed to explore low-level text editing architecture, terminal rendering, and systems-style programming.

---

## Features

- Line numbers
- Arrow key navigation
- Horizontal & vertical scrolling
- File save support
- Search with highlighting
- Find next match
- Undo / redo system
- Delete & backspace handling
- Status bar with cursor info
- Unsaved changes warning on exit
- Page up / page down navigation
- Clean terminal UI

---

## Controls

| Key | Action |
|---|---|
| ESC | Exit editor |
| CTRL + S | Save file |
| F2 | Search text |
| F3 | Find next match |
| CTRL + Z | Undo |
| CTRL + Y | Redo |
| Arrow keys | Move cursor |
| Enter | New line |
| Backspace | Delete left |
| Delete | Delete right |
| Page Up / Down | Fast scroll |
| Home / End | Line start / end |

---

## Build Instructions

### Requirements

- Linux or WSL
- g++
- ncurses
- make

Install dependencies:

```bash
sudo apt update
sudo apt install g++ libncurses5-dev libncursesw5-dev make
