#pragma once
// Dog-ear marker for the EPUB reader (added by the crosspoint-dogear patch).
// One mark per book, drawn as a folded page corner instead of the small status-bar icon.
//
// Memory: no heap at all. The corner is recomputed with integer math on each render pass;
// that is a few milliseconds even on the ESP32-C3, so nothing needs to be kept in RAM.

#include <BitmapHelpers.h>
#include <GfxRenderer.h>

#include "../../BookmarkEntry.h"
#include "../ActivityResult.h"
#include "DogEarPaint.h"

namespace DogEar {

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
  const bool bw = mode == GfxRenderer::BW;
  const bool msb = mode == GfxRenderer::GRAYSCALE_MSB;
  const bool absolute = !bw && renderer.grayPlanesAreAbsolute();
  paint(renderer.getScreenWidth(), renderer.getScreenHeight(), grayPassActive(),
        [&](const int x, const int y, const int level, const bool force) {
          if (bw) {
            if (level < 3) {
              renderer.drawPixel(x, y, true);  // gray pixels start black; the gray pass lightens them
            } else if (force) {
              renderer.drawPixel(x, y, false);
            }
            return;
          }
          if (level == 3 && !force) return;
          const auto gp = grayPlanePixel(static_cast<uint8_t>(level), msb, absolute);
          if (gp.write) {
            renderer.drawPixel(x, y, gp.black);
          } else if (!absolute) {
            renderer.drawPixel(x, y, true);  // overlay plane: clear any gray left by text underneath
          }
        });
}

inline bool hitsCorner(const GfxRenderer& renderer, const int x, const int y) {
  return hitsCorner(renderer.getScreenWidth(), renderer.getScreenHeight(), x, y);
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
