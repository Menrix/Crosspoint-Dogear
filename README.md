# Dog-ear for CrossPoint

*Never fold a real book: fold the page corner on your e-reader instead.*

A realistic folded page corner that marks where you stopped reading, for the
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) firmware.

![A dog-ear on an e-reader](images/dog-ear-mockup.jpg)

- **Fold the corner** – right where you stopped
- **Jump back** – one press takes you to your dog-ear
- **One per book** – a new one replaces the old one
- **Survives font and layout changes**
- **Big and clear** – instead of the tiny bookmark icon
- **Pure black and white** – crisp on e-ink, nothing shows through
- **No extra memory** – runs on the small X3/X4 chip too

**Free & open source · Ready to flash · Updates built in**

## Install

Get the file for your device from **[Releases](../../releases)**:

| Device | File |
| --- | --- |
| Xteink X4 Pro | `crosspoint-<version>-x4pro.bin` |
| Xteink X3 and X4 | `crosspoint-<version>-x3-x4.bin` |

> **Tested on the X4 Pro and on an X3 (by a user). X4 not tested yet.**

Copy it to the SD card and use **Settings → SD Card Firmware Update**. Later updates:
**Settings → Check for Updates**.

## Use it

Set the keys once in **Settings → Controls**:

| | X4 Pro (touch) | X3 / X4 (buttons) |
| --- | --- | --- |
| Setup | **Home Button Gestures → Long Press → Dog-ear** and **Touch Reader Controls → On** | **Short Power Button Click → Dog-ear** and **Long-Press Menu → Go to Dog-ear** |
| Fold the corner here | Hold the top-right corner | **Power** short press |
| Remove the dog-ear | Tap it | **Power** short press on its page |
| Jump to the dog-ear | Hold **Home** | Hold **Confirm** |

- Folding a new corner moves the dog-ear: the old one disappears.
- Nothing is removed by accident: removing always happens on the dog-ear's own page.
- If there is nothing to jump to, nothing happens. No pop-ups, no extra screen refreshes.
- **Reader menu → Toggle Dog-ear** works too, on every device.
- *Optional:* **Settings → Reader → Text Anti-Aliasing** off gives the fastest page turns. The
  dog-ear looks the same either way.

![Real photo of the dog-ear on an Xteink X4 Pro](images/dog-ear.jpg)

<!-- Hidden until Crosspoint-ToDoList and Crosspoint-Addons are public:
## More add-ons

- [Crosspoint-ToDoList](https://github.com/Menrix/Crosspoint-ToDoList): to-do and shopping lists
  you write and tick right on the reader.
- [Crosspoint-Addons](https://github.com/Menrix/Crosspoint-Addons): **all add-ons in one
  firmware**. A growing collection; install this one if you want more than one add-on.
-->

## How it is built

Every night a GitHub Action adds this to the latest official CrossPoint release and publishes
the firmware under Releases. "Check for Updates" on the reader gets these builds, so you stay on
the newest CrossPoint and keep the dog-ear.

Self-built firmware does not include the official release key for CrossPoint's LCP service (only
relevant for LCP-protected books from that service).

## License

MIT, like CrossPoint.

Made with the help of Claude.
