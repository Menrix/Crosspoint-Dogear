#!/usr/bin/env python3
"""Adds the dog-ear feature to a CrossPoint Reader source tree.

Usage: python3 apply_dogear.py <path-to-crosspoint-reader> [owner/repo]

With owner/repo, the on-device "Check for updates" asks that GitHub repository's latest
release instead of the official one, so updates keep the dog-ear.

Every change is anchored to a piece of the original code. If CrossPoint changes one of
those places, the script stops with a clear message instead of building something broken.
"""

import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent


def fail(msg: str) -> None:
    print(f"DOG-EAR PATCH FAILED: {msg}", file=sys.stderr)
    sys.exit(1)


def replace_once(text: str, old: str, new: str, what: str) -> str:
    count = text.count(old)
    if count != 1:
        fail(f"'{what}': expected the anchor exactly once, found it {count} times. "
             f"CrossPoint probably changed this spot; the patch needs an update.")
    return text.replace(old, new, 1)


def main() -> None:
    if len(sys.argv) not in (2, 3):
        fail("usage: apply_dogear.py <crosspoint-reader folder> [owner/repo]")
    root = Path(sys.argv[1])
    reader_dir = root / "src" / "activities" / "reader"
    target = reader_dir / "EpubReaderActivity.cpp"
    if not target.is_file():
        fail(f"{target} not found")

    for name in ("DogEar.h", "DogEarPaint.h"):
        shutil.copy(HERE / "files" / name, reader_dir / name)

    src = target.read_text(encoding="utf-8")
    if '#include "DogEar.h"' in src:
        fail("patch is already applied to this tree")

    # 1) Make the drawing code available.
    src = replace_once(src, '#include "EpubReaderBookmarksActivity.h"\n',
                       '#include "EpubReaderBookmarksActivity.h"\n#include "DogEar.h"\n', "include")

    # 2) Only one bookmark per book: a new one replaces the old one.
    #    (Long-pressing again on the marked page still removes it, as before.)
    src = replace_once(src, "    cachedBookmarks.insert(cachedBookmarks.begin(), entry);\n",
                       "    cachedBookmarks.clear();  // dog-ear: only one per book\n"
                       "    cachedBookmarks.insert(cachedBookmarks.begin(), entry);\n", "one bookmark per book")

    # 3) Hide the small status-bar icon and draw the dog-ear right after the status bar
    #    (B/W pass), so nothing in the status bar can paint over it.
    anchor = "textYOffset, true, currentPageBookmarked,"
    src = replace_once(src, anchor, "textYOffset, true, false /* dog-ear replaces the icon */,", "hide bookmark icon")
    pos = src.find("false /* dog-ear replaces the icon */,")
    end = src.find(");\n", pos)
    if end < 0:
        fail("'draw after status bar': end of statement not found")
    end += len(");\n")
    src = src[:end] + "  if (currentPageBookmarked) DogEar::draw(renderer);\n" + src[end:]

    # 4) Also draw it in the grayscale passes, so anti-aliased text cannot show through the flap.
    start = src.find("auto renderGrayscalePass = [&]() {")
    if start < 0:
        fail("'grayscale pass': anchor not found")
    end = src.find("\n  };\n", start)
    if end < 0:
        fail("'grayscale pass': end of block not found")
    src = src[:end] + "\n    if (currentPageBookmarked) DogEar::draw(renderer);" + src[end:]

    # 5) Real gray tones only when this page also gets a grayscale pass (text anti-aliasing).
    src = replace_once(src, "  const int fontId = SETTINGS.getReaderFontId();\n  ImageBlock::clearRenderFailures();\n",
                       "  const int fontId = SETTINGS.getReaderFontId();\n  ImageBlock::clearRenderFailures();\n"
                       "  DogEar::GrayToneScope dogEarGrayTones(SETTINGS.textAntiAliasing);\n", "gray tones")

    # 6) No 'Bookmark added' popup: the folded corner is the feedback (saves two refreshes).
    src = replace_once(src,
                       "    GUI.drawPopup(renderer, bookmarkRemoved ? tr(STR_BOOKMARK_REMOVED) : tr(STR_BOOKMARK_ADDED));\n",
                       "    // dog-ear: no popup, the folded corner is the feedback\n", "popup")
    src = replace_once(src, "    showBookmarkMessage = false;\n    requestUpdate();\n",
                       "    showBookmarkMessage = false;  // dog-ear: nothing to clear, no extra refresh\n",
                       "popup timeout")

    # 7) "Place" and "go to", the same on every device:
    #    Place  = X4 Pro: long press on the corner. X3/X4: Confirm long ('Dog-ear', stock toggle:
    #             folds here, moves an older dog-ear here, removes it on its own page).
    #    Go to  = X4 Pro: Home key action. X3/X4: Power short ('Go to Dog-ear').
    #             On another page -> jump there; on its page -> nothing.
    #             No dog-ear yet -> X4 Pro: set it here. X3/X4: nothing (no popup, no extra e-ink refresh).
    #    Home key action 'Bookmark' / long-press menu 'Bookmark': no dog-ear yet -> set it here;
    #    a dog-ear on another page -> jump there; on the dog-ear page -> nothing.
    #    Long press on the corner (touch): fold the corner here; an older dog-ear moves here.
    def long_press(indent: str) -> str:
        return (f"{indent}if (cachedBookmarks.empty()) {{  // dog-ear: 'go to' with nothing to go to\n"
                f"{indent}  if (!BoardConfig::hasTouch()) return;  // X3/X4: nothing happens, not even a redraw\n"
                f"{indent}  addBookmark();  // X4 Pro: the Home key also sets the first dog-ear\n"
                f"{indent}}} else if (!currentPageBookmarked) {{\n"
                f"{indent}  applyProgressChange(DogEar::jumpTarget(cachedBookmarks.front(), epub->getSpineItemsCount()));\n"
                f"{indent}}} else {{\n"
                f"{indent}  haptic_feedback::touchAction(true);  // already on the dog-ear page: no redraw\n"
                f"{indent}  return;\n"
                f"{indent}}}\n")

    src = replace_once(src,
                       "      case HomeButtonAction::Bookmark:\n        if (!showBookmarkMessage) {\n          addBookmark();\n",
                       "      case HomeButtonAction::Bookmark:\n        if (!showBookmarkMessage) {\n" + long_press("          "),
                       "home key action")
    src = replace_once(src, "    startClipSelection(selectionX, selectionY);\n",
                       "    if (DogEar::hitsCorner(renderer, selectionX, selectionY)) {\n"
                       "      if (!currentPageBookmarked) addBookmark();  // dog-ear: fold here (moves an older one)\n"
                       "      haptic_feedback::touchAction();\n"
                       "      return;\n"
                       "    }\n"
                       "    startClipSelection(selectionX, selectionY);\n", "corner long press")

    # 8) Tap on the dog-ear (on its own page) removes it. Checked before the tap can turn the page.
    start = src.find("  const auto touch =\n      ReaderUtils::detectTouchPageTurn(")
    if start < 0:
        fail("'tap to remove': anchor not found")
    end = src.find(";\n", start)
    if end < 0:
        fail("'tap to remove': end of statement not found")
    end += 2
    src = src[:end] + (
        "\n  if (currentPageBookmarked && mappedInput.hasTouch()) {  // dog-ear: tap it to remove it\n"
        "    int tapX = 0;\n"
        "    int tapY = 0;\n"
        "    if (mappedInput.wasScreenTapped(tapX, tapY) && DogEar::hitsCorner(renderer, tapX, tapY)) {\n"
        "      cachedBookmarks.clear();\n"
        "      currentPageBookmarked = false;\n"
        "      if (!BookmarkFile::save(epub->getPath(), cachedBookmarks)) LOG_ERR(\"ERS\", \"Failed to save bookmarks\");\n"
        "      haptic_feedback::touchAction();\n"
        "      requestUpdate();\n"
        "      return;\n"
        "    }\n"
        "  }\n") + src[end:]

    # 8b) A page range ends exactly where the next chapter's first page begins, so a mark on a
    #     chapter's first page also matched the previous chapter's last page (two dog-ears shown).
    #     A mark always knows its chapter: never match it on a page of another chapter.
    src = replace_once(src, "  const float bookmarkProgress = std::clamp(bookmark.percentage, 0.0f, 1.0f);\n",
                       "  // dog-ear: a mark from another chapter never belongs to this page (chapter boundaries touch)\n"
                       "  if (bookmark.computedChapterPageCount > 0 && bookmark.computedSpineIndex != spineIndex) return false;\n"
                       "  const float bookmarkProgress = std::clamp(bookmark.percentage, 0.0f, 1.0f);\n",
                       "chapter boundary")

    target.write_text(src, encoding="utf-8")

    # 9) Button-only boards (X3/X4): offer 'Go to Dog-ear' for the short Power click
    #    (the stock list has the bookmark shortcut only on touch boards without a home key).
    settings = root / "src" / "SettingsList.h"
    settings_src = replace_once(
        settings.read_text(encoding="utf-8"),
        "  // Touch boards without a home key have no button for these shortcuts.\n",
        "  if (!BoardConfig::hasTouch()) {  // dog-ear: button-only boards can put it on Power too\n"
        "    options.push_back({StrId::STR_GO_TO_DOGEAR, CrossPointSettings::PWR_BOOKMARK});\n"
        "  }\n"
        "  // Touch boards without a home key have no button for these shortcuts.\n",
        "power button option")
    settings.write_text(settings_src, encoding="utf-8")

    # 10) Call it what it is: 'Dog-ear' instead of 'Bookmark' in the settings and the reader menu.
    names = {
        "english.yaml": ("Dog-ear", "Toggle Dog-ear"),
        "german.yaml": ("Eselsohr", "Eselsohr setzen/entfernen"),
    }
    translations = root / "lib" / "I18n" / "translations"
    if not (translations / "english.yaml").is_file():
        fail("'translations': english.yaml not found")
    for yaml_file in sorted(translations.glob("*.yaml")):
        option, toggle = names.get(yaml_file.name, names["english.yaml"])
        text = yaml_file.read_text(encoding="utf-8")
        new_text = re.sub(r'^STR_BOOKMARK_OPTION: .*$', f'STR_BOOKMARK_OPTION: "{option}"', text, flags=re.M)
        new_text = re.sub(r'^STR_TOGGLE_BOOKMARK: .*$', f'STR_TOGGLE_BOOKMARK: "{toggle}"', new_text, flags=re.M)
        if yaml_file.name == "english.yaml" and new_text == text:
            fail("'translations': bookmark labels not found in english.yaml")
        extra = {
            "english.yaml": {"STR_GO_TO_DOGEAR": "Go to Dog-ear"},
            "german.yaml": {"STR_GO_TO_DOGEAR": "Zum Eselsohr"},
        }.get(yaml_file.name, {})  # other languages fall back to English
        for key, value in extra.items():
            if f"{key}:" not in new_text:
                new_text = new_text.rstrip("\n") + f'\n{key}: "{value}"\n'

        yaml_file.write_text(new_text, encoding="utf-8")

    # 10) On-device updates come from this repository's releases (they include the dog-ear).
    if len(sys.argv) == 3:
        repo = sys.argv[2].strip()
        if repo.count("/") != 1 or not all(part for part in repo.split("/")):
            fail(f"'{repo}' is not in owner/repo form")
        ota = root / "src" / "network" / "OtaUpdater.cpp"
        ota_src = replace_once(ota.read_text(encoding="utf-8"),
                               '"https://api.github.com/repos/crosspoint-reader/crosspoint-reader/releases/latest"',
                               f'"https://api.github.com/repos/{repo}/releases/latest"', "update source")
        ota.write_text(ota_src, encoding="utf-8")
    print("Dog-ear patch applied.")


if __name__ == "__main__":
    main()
