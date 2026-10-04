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

Each RAF + JPG pair with the same name counts as one shot, so a mark always applies to both files. Marks are saved to `.picpicker.json` in the folder as you go. **Apply…** moves the files into `selected/` and `rejected/`, or sends rejected shots to the Trash if you tick that option.

| Key | Viewer | Grid |
|---|---|---|
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
