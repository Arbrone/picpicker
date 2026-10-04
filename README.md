# PicPicker

![banner](./assets/picpicker-banner.png)

Are you a photographer shooting in **RAW + JPG** and dreading the double effort of sorting both formats? Say goodbye to that hassle! PicPicker is here to streamline your workflow, letting you sort your photos once and apply those changes to both RAW and JPG files—no extra work needed.

## Install (C++ version, recommended)

The native version in `cpp/` is much faster. It never develops the RAW: it shows the JPG, or the preview JPEG embedded in the RAF when there is no JPG. Images ahead of the current one are prefetched, and thumbnails are cached in `~/.cache/picpicker`.

```shell
sudo apt install qt6-base-dev libraw-dev cmake g++ pkg-config
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build -j
./cpp/build/picpicker /path/to/photos   # folder argument is optional
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

## Install (legacy Python version)

```shell
pip install -r requirements.txt
python3 picpicker/main.py
```

With PicPicker, you can easily choose which format to view, make your selections, and effortlessly sort them into different directories—all in one seamless process.

![demo](assets/demo.gif)

---

## TODO

[ ] make a real package

[ ] manage different raw formats

[ ] get and display EXIF

[ ] sort image by selection
