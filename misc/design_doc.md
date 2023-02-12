# Loading Screen Game Design Document

## Basic Concept
Loading takes N seconds but each new window slows down the loading speed.
Close windows as quickly as possible to load the project quicker.

## Projects (Difficulty Options)
3 projects of varying difficulties:
1. Small Project:
  - Base Loading Time        : 30 seconds
  - Max Distraction Interval : 8 seconds
2. Medium Project:
  - Base Loading Time        : 60 seconds
  - Max Distraction Interval : 6 seconds
2. Large Project:
  - Base Loading Time        : 120 seconds
  - Max Distraction Interval : 5 seconds

## Closing Distractions ("Gameplay")
Having more windows open has a significant cost on load times since the loading speed is divided
by the square of the number of open windows.

Types of distracting windows:
- **Project Already Open**: If the project is loading, a window shows up saying the project is already open.
- **Pop-ups**: Infuriating ads that pop up randomly on screen for no apparent reason.
- **Flack Message**: Your co-worker "Chaudhury" really wants to talk to you today.
- **Click-bait**: Like the other pop-ups but with an open button. If you click that, foogle rome opens.
- **Foogle Rome**: This opens if you fall for the click-bait and press open. There's no content here. Just misery.

Moving the mouse also halves the loading speed. Hence the player must close the additional
windows as quickly as possible.

## Stuff to package

### Art Assets
game_icon.ico
game_icon.png
power_button.png
shortcut_icon_notes.png
shortcut_icon_project.png
wallpaper_default.png

### Fonts
assistant-medium-font.bytes

