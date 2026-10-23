# StarsOfDeduce Windhawk Mods

A small collection of personal [Windhawk](https://windhawk.net/) mods for refining the Windows desktop, Start menu, and File Explorer.

These mods run inside Windows shell processes through Windhawk. Review the source, compatibility notes, and per-file licensing before installing or redistributing them.

## Mods

### ZenDesktop: Desktop Icon Toggle and Auto-Hide

| Field | Value |
| :--- | :--- |
| File | [`zen-desktop-toggle-icons.cpp`](zen-desktop-toggle-icons.cpp) |
| Mod ID | `zen-desktop-toggle-icons` |
| Version | `3.4.0` |
| Author metadata | `StarsOfDeduce` |
| Target | `explorer.exe` on x86-64 Windows |

- Double-click empty desktop space to hide or show desktop icons.
- Leaves normal double-click behavior intact when the pointer is over an icon.
- Optionally hides icons after 3–60 seconds of system-wide inactivity.
- Restores automatically hidden icons on mouse or keyboard input without changing icons that were hidden manually.
- Reattaches when Explorer or the desktop view is recreated.
- Pauses the inactivity behavior around full-screen windows to avoid distracting visual changes.

### ZenDesktop: Start Menu Acrylic Styler

| Field | Value |
| :--- | :--- |
| File | [`zen-startmenu-acrylic.cpp`](zen-startmenu-acrylic.cpp) |
| Mod ID | `zen-startmenu-acrylic` |
| Version | `3.4.0` |
| Author metadata | `StarsOfDeduce` |
| Targets | `StartMenuExperienceHost.exe`, `SearchHost.exe`, and `SearchApp.exe` on x86-64 Windows |

- Adds configurable acrylic and frosted-glass styling to the Windows 11 Start menu.
- Provides blur, tint, opacity, and color presets, including light, dark, blue, cyan, and liquid-glass-inspired looks.
- Includes layout and label-visibility controls for supported Start menu variants.
- Builds on m417z's Start Menu Styler and its community theme system.

### Paste Clipboard Images as Files in Explorer

| Field | Value |
| :--- | :--- |
| File | [`paste_image_explorer_mod.cpp`](paste_image_explorer_mod.cpp) |
| Mod ID | `explorer-paste-clipboard-images` |
| Version | `1.0.0` |
| Author metadata | `Antigravity` |
| Target | `explorer.exe` |

- Detects PNG, DIB, and bitmap image data on the clipboard.
- Lets Explorer paste copied images as timestamped PNG files with `Ctrl` + `V` or the Paste command.
- Works with common screenshot sources such as messaging applications and browsers.
- Generates names such as `PastedImage_20261023_142530.png` and adds a numeric suffix when needed.
- Runs through the Explorer integration rather than a separate background executable.

## Installation

1. Install and open [Windhawk](https://windhawk.net/).
2. Open the advanced tools and create a new mod.
3. Copy the complete contents of the desired `.cpp` file into the Windhawk editor.
4. Save and compile the mod.
5. Review the mod's settings and enable it when ready.

Windhawk downloads the required build components and loads the compiled mod into the process named in its metadata. If compilation fails after a Windows or Windhawk update, disable the mod and check the source or issue tracker before retrying.

## Safety notes

- These are native process-injection mods. A defect can restart or destabilize Explorer, the Start menu, or Search.
- Test one mod at a time and keep a way to open Windhawk and disable it.
- Compatibility can change with Windows updates because shell implementation details are not a stable public API.
- The clipboard mod writes image data to the destination selected in Explorer; check that location before pasting sensitive screenshots.

## Attribution and licensing

- [Windhawk](https://windhawk.net/) is created by m417z / Ramen Software.
- The desktop interaction approach draws on work by [Lanbo](https://github.com/Liset999) and m417z.
- `zen-startmenu-acrylic.cpp` states that its source is published under the GNU General Public License v3.0 and contains upstream attribution.
- This repository does not declare one repository-wide license. Review the header and upstream terms of each source file before redistribution.
