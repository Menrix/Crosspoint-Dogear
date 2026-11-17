#pragma once
// Dog-ear marker for the EPUB reader (added by the crosspoint-dogear patch).
// One mark per book, drawn as a folded page corner instead of the small status-bar icon.

#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <Logging.h>
#include <PersistableStore.h>

#include <vector>

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

// The corner is computed once and kept as a small pixel map; every later page render only
// copies it (the shape math is far too slow to repeat for each of the ~20 render passes).
// Rebuilt when the screen size (orientation), the look or the gray mode changes.
// Memory: one byte per pixel of the corner area, about 25 KB at the default size.
struct Cache {
  int screenW = 0, screenH = 0;
  bool grayTones = false;
  Look look;
  int x0 = 0, w = 0, h = 0;
  std::vector<uint8_t> pixels;  // 0 = untouched, else 0x80 | force << 2 | level
};

inline Cache& cache() {
  static Cache c;
  return c;
}

inline const Cache& prepared(const int screenW, const int screenH, const bool grayTones) {
  Cache& c = cache();
  if (!c.pixels.empty() && c.screenW == screenW && c.screenH == screenH && c.grayTones == grayTones &&
      c.look == look()) {
    return c;
  }
  c.screenW = screenW;
  c.screenH = screenH;
  c.grayTones = grayTones;
  c.look = look();
  extent(c.look, screenW, screenH, c.w, c.h);
  c.w = std::min(c.w, screenW);
  c.h = std::min(c.h, screenH);
  c.x0 = screenW - c.w;
  c.pixels.assign(static_cast<size_t>(c.w) * c.h, 0);
  paint(c.look, screenW, screenH, grayTones, [&](int x, int y, int level, bool force) {
    const int cx = x - c.x0;
    if (cx < 0 || cx >= c.w || y < 0 || y >= c.h) return;
    c.pixels[static_cast<size_t>(y) * c.w + cx] = static_cast<uint8_t>(0x80 | (force ? 4 : 0) | (level & 3));
  });
  return c;
}

// Draws the folded corner into the current render target: the B/W pass and both
// grayscale plane passes, so anti-aliased text under the corner cannot shine through.
inline void draw(const GfxRenderer& renderer) {
  const Cache& c = prepared(renderer.getScreenWidth(), renderer.getScreenHeight(), grayPassActive());
  const auto mode = renderer.getRenderMode();
  const bool bw = mode == GfxRenderer::BW;
  const bool msb = mode == GfxRenderer::GRAYSCALE_MSB;
  const bool absolute = !bw && renderer.grayPlanesAreAbsolute();
  const uint8_t* p = c.pixels.data();
  for (int y = 0; y < c.h; ++y) {
    for (int cx = 0; cx < c.w; ++cx, ++p) {
      const uint8_t v = *p;
      if (!(v & 0x80)) continue;
      const int x = c.x0 + cx;
      const int level = v & 3;
      const bool force = v & 4;
      if (bw) {
        if (level < 3) {
          renderer.drawPixel(x, y, true);  // gray pixels start black; the gray pass lightens them
        } else if (force) {
          renderer.drawPixel(x, y, false);
        }
        continue;
      }
      if (level == 3 && !force) continue;
      const auto gp = grayPlanePixel(static_cast<uint8_t>(level), msb, absolute);
      if (gp.write) {
        renderer.drawPixel(x, y, gp.black);
      } else if (!absolute) {
        renderer.drawPixel(x, y, true);  // overlay plane: clear any gray left by text underneath
      }
    }
  }
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
