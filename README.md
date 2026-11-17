# CrossPoint Dog-ear

A small add-on for [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)
on the **Xteink X4 Pro**: mark where you stopped reading with a realistic folded page corner
("dog-ear") instead of the tiny bookmark icon.

This repository does not contain a copy of CrossPoint. A GitHub Action takes every new official
CrossPoint release, adds the dog-ear and publishes the finished firmware under **Releases**.

## How it works on the reader

| Action | Result |
| --- | --- |
| Long press (Home key or the top-right corner) with **no dog-ear yet** | Folds the corner on the current page |
| Long press when a dog-ear **is set on another page** | Jumps to the dog-ear |
| **Tap the dog-ear** | Removes it |

- One dog-ear per book. It is stored like a CrossPoint bookmark, so it survives font, margin and orientation changes.
- Set the Home key: **Settings → Controls → Home Button Gestures → Long Press → Bookmark**.
- The corner long press and tap need **Touch Reader Controls** turned on.
- Real gray tones need **Text Anti-Aliasing** on. Without it the gray is drawn as a fine dot pattern.
- **Reader menu → Toggle Bookmark** still works too, for example without touch.

## Install

1. Open the latest release and download `crosspoint-<version>-x4pro.bin`.
2. Go to <https://crosspointreader.com/#flash-tools>, choose **X4 Pro**, then **Custom .bin**, and upload the file.

After that, **Settings → Check for Updates** on the reader installs new versions from this
repository, so updates keep the dog-ear.

## Tune the look on the device

Copy [`sdcard/dogear.json`](sdcard/dogear.json) to the root of the SD card (for example with
File Transfer), change values, then reopen the book. No new firmware is needed.

| Key | Default | Meaning |
| --- | --- | --- |
| `size` | 22 | Fold width along the top edge, % of the screen's short side (5–45) |
| `crease` | 145 | How far down the right edge the crease ends, % of the fold width (100 = 45°) |
| `tipX` | 120 | Folded tip, distance from the right edge, % of the fold width |
| `tipY` | 93 | Folded tip, distance from the top edge, % of the fold width |
| `background` | 1 | Uncovered corner: 1 = dark gray, 2 = light gray (one flat tone) |
| `flapShade` | 25 | Gray on the flap towards the tip, 0 (white) – 255 |
| `edge` | 0 | Flap outline: 0 = black, 1 = dark gray, 2 = light gray, 3 = none |
| `shadowWidth` | 10 | How far the tip's shadow falls downwards, % of the fold width (0 = no shadow) |
| `shadowShift` | 80 | Sideways offset of the tip's shadow, % of that drop (+ = right, − = left) |
| `shadowSoft` | 60 | Soft fade around the shadow, % of the drop (0 = razor sharp) |
| `shadowStrength` | 100 | Shadow darkness, 0–100 (100 = black) |

Missing keys use the default. Values outside the range are clamped.

## Updates and maintenance

- The Action runs every night. When CrossPoint publishes a new release, a matching release appears here.
- To build by hand: **Actions → Build CrossPoint with dog-ear → Run workflow**.
- If CrossPoint changes the code the patch hooks into, the build stops with a message that
  names the spot (`DOG-EAR PATCH FAILED: ...`). GitHub sends an email; then `patch/apply_dogear.py` needs an update.

## Files

- `patch/files/DogEarPaint.h`: drawing of the folded corner (no hardware code, can be previewed on a PC)
- `patch/files/DogEar.h`: connects the drawing to the CrossPoint renderer, settings file and jump
- `patch/apply_dogear.py`: adds the dog-ear to a CrossPoint source tree
- `.github/workflows/build.yml`: nightly build and release

Self-built firmware does not include the official release key for CrossPoint's LCP service
(only relevant for LCP-protected books from that service). Everything else works as in the
official release.

MIT licensed, like CrossPoint.
