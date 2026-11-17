#pragma once
// Dog-ear marker for the EPUB reader (added by the crosspoint-dogear patch).
// One mark per book, drawn as a folded page corner instead of the small status-bar icon.

#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <Logging.h>
#include <PersistableStore.h>

#include "../ActivityResult.h"
#include <BitmapHelpers.h>
#include "../../BookmarkEntry.h"
#include "DogEarPaint.h"

namespace DogEar {

// Optional tuning file on the SD card. Missing file or missing keys = built-in defaults.
constexpr char kLookFile[] = "/dogear.json";

inline Look& look() {
  static Look current;
  return current;
}

// Re-reads /dogear.json (called when a book is opened, so changes show without a reboot).
inline void reloadLook() {
  Look fresh;
  JsonDocument doc;
  if (PersistableStoreBase::readDocFromFile(kLookFile, doc)) {
    fresh.size = doc["size"] | fresh.size;
    fresh.crease = doc["crease"] | fresh.crease;
    fresh.tipX = doc["tipX"] | fresh.tipX;
    fresh.tipY = doc["tipY"] | fresh.tipY;
    fresh.background = doc["background"] | fresh.background;
    fresh.flapShade = doc["flapShade"] | fresh.flapShade;
    fresh.edge = doc["edge"] | fresh.edge;
    fresh.shadowWidth = doc["shadowWidth"] | fresh.shadowWidth;
    fresh.shadowStrength = doc["shadowStrength"] | fresh.shadowStrength;
    fresh.shadowShift = doc["shadowShift"] | fresh.shadowShift;
    fresh.shadowSoft = doc["shadowSoft"] | fresh.shadowSoft;
    LOG_DBG("DOG", "Loaded %s", kLookFile);
  }
  fresh.clampAll();
  look() = fresh;
}

// True while a page is rendered that also gets a grayscale pass (text anti-aliasing on).
// Only then can the B/W pass draw gray pixels as black for the gray pass to lighten.
inline bool& grayPassActive() {
  static bool active = false;
  return active;
}

// Sets grayPassActive() for one page render and always resets it afterwards.
struct GrayToneScope {
  explicit GrayToneScope(const bool on) { grayPassActive() = on; }
  ~GrayToneScope() { grayPassActive() = false; }
  GrayToneScope(const GrayToneScope&) = delete;
  GrayToneScope& operator=(const GrayToneScope&) = delete;
};

// Draws the folded corner into the current render target: the B/W pass and both
// grayscale plane passes, so anti-aliased text under the corner cannot shine through.
inline void draw(const GfxRenderer& renderer) {
  const auto mode = renderer.getRenderMode();
  const bool grayTones = grayPassActive();
  if (mode == GfxRenderer::BW) {
    paint(look(), renderer.getScreenWidth(), renderer.getScreenHeight(), grayTones, [&](int x, int y, int level, bool force) {
      if (level < 3) {
        renderer.drawPixel(x, y, true);  // gray pixels start black; the gray pass lightens them
      } else if (force) {
        renderer.drawPixel(x, y, false);
      }
    });
    return;
  }
  const bool msb = mode == GfxRenderer::GRAYSCALE_MSB;
  const bool absolute = renderer.grayPlanesAreAbsolute();
  paint(look(), renderer.getScreenWidth(), renderer.getScreenHeight(), grayTones, [&](int x, int y, int level, bool force) {
    if (level == 3 && !force) return;
    const auto p = grayPlanePixel(static_cast<uint8_t>(level), msb, absolute);
    if (p.write) {
      renderer.drawPixel(x, y, p.black);
    } else if (!absolute) {
      renderer.drawPixel(x, y, true);  // overlay plane: clear any gray left by text underneath
    }
  });
}

inline bool hitsCorner(const GfxRenderer& renderer, const int x, const int y) {
  return hitsCorner(look(), renderer.getScreenWidth(), renderer.getScreenHeight(), x, y);
}

// Where to jump for a stored dog-ear (same fields the bookmark list uses).
inline ProgressChangeResult jumpTarget(const BookmarkEntry& mark, const int spineCount) {
  ProgressChangeResult result{};
  result.xpath = mark.xpath;
  result.percentage = mark.percentage;
  result.hasSavedProgress = true;
  result.hasVisibleTextOffset = mark.hasVisibleTextOffset;
  result.visibleTextOffset = mark.visibleTextOffset;
  result.spineIndex = mark.computedSpineIndex;
  if (mark.computedChapterPageCount > 0 && mark.computedChapterProgress < mark.computedChapterPageCount &&
      mark.computedSpineIndex < spineCount) {
    result.page = mark.computedChapterProgress;
    result.totalPages = mark.computedChapterPageCount;
  }
  return result;
}

}  // namespace DogEar
