# PicPicker

![banner](./assets/picpicker-banner.png)

Are you a photographer shooting in **RAW + JPG** and dreading the double effort of sorting both formats? Say goodbye to that hassle! PicPicker is here to streamline your workflow, letting you sort your photos once and apply those changes to both RAW and JPG files—no extra work needed.

## Install

PicPicker is a native Qt6 app. It never develops the RAW: it shows the JPG, or the preview JPEG embedded in the RAF when there is no JPG. Images ahead of the current one are prefetched, and thumbnails are cached in `~/.cache/picpicker`.

```shell
sudo apt install qt6-base-dev libraw-dev cmake g++ pkg-config
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/picpicker /path/to/photos   # folder argument is optional
```

Each RAF + JPG pair with the same name counts as one shot, so a mark always applies to both files.

- **Bursts**: shots taken less than 1 s apart (EXIF time incl. sub-seconds) are stacked into one grid cell. Open it to go through its frames, or press **K** on the keeper to select it and reject the rest.
- **Compare**: view 2–4 frames side by side with zoom and panning in sync. Press **C** on a burst, on a selection in the grid, or in the viewer.
- **AF point**: the focus point recorded by the camera is drawn on the image, and **Z** zooms straight to it at 100%.
- **Ratings and colour labels** are written to `<name>.xmp` sidecars, which Lightroom and Capture One read. Rejected shots get rating −1. PicPicker never overwrites a sidecar created by another app.
- Marks are saved to `.picpicker.json` in the folder as you go. **Apply…** moves every file of a shot, including its sidecars, into `selected/` and `rejected/`, or sends rejected shots to the Trash.

| Key | Viewer / compare | Grid |
|---|---|---|
| ← / → | previous / next shot (bursts included) | navigate |
| Shift+← / → or PgUp / PgDn | skip the rest of a burst | |
| ↑ or S / ↓ or X | select / reject, then go to the next shot | S / X mark the selection |
| Space | clear mark | clear mark |
| 1–5 / 0 | star rating / no rating | same |
| 6–9 | red / yellow / green / blue label (toggle) | same |
| K | keep this frame, reject the rest of the burst | |
| C | compare | compare selection or burst |
| Wheel, + / − | zoom (around the cursor) | thumbnail size (Ctrl+wheel) |
| Z, double-click | fit ↔ 100% on the AF point (drag to pan) | |
| F / R | show AF point / rotate | |
| Esc | leave zoom, compare, viewer | |
| H or ? | all shortcuts | all shortcuts |
| Ctrl+Z / Tab / F11 / Ctrl+O | undo / side panel / full screen / open folder | same |

The number-row keys work by physical position, so on AZERTY you don't need Shift. A filmstrip under the viewer shows where you are, with bursts underlined; click it to jump. Recent folders appear on the start page, and you can also drop a folder onto the window.

---|---|---|
| ← / → | previous / next | navigate |
| ↑ or S / ↓ or X | select / reject, then go to the next shot | S / X mark the selection |
| Space | clear mark | clear mark |
| 1–5 / 0 | star rating / no rating | same |
| 6–9 | red / yellow / green / blue label (toggle) | same |
| Enter or B | open the burst | open |
| K | keep this frame, reject the others | |
| C | compare | compare selection or burst |
| Z, double-click | 100% zoom on the AF point (drag to pan) | |
| F / R | show AF point / rotate | |
| Esc | back (leave zoom, burst, compare) | |
| Ctrl+Z | undo last change | undo last change |

---|---|---|
| ← / → | previous / next | navigate |
| ↑ or S / ↓ or X | select / reject, then go to the next shot | S / X mark the selection |
| Space | clear mark | clear mark |
| Z, double-click | toggle 100% zoom (drag to pan) | |
| R | rotate | |
| Esc / Enter | back to grid | open |
| Ctrl+Z | undo last mark | undo last mark |

![demo](assets/demo.gif)

---

## TODO

[ ] make a real package

[ ] manage different raw formats

[ ] sort image by selection
