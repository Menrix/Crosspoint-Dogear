#pragma once
// Dog-ear (folded page corner) painter. Hardware-free so it can be previewed on a PC.
// Draws in the top-right corner of a screen that is `screenWidth` pixels wide.
//
// px(x, y, level, force):
//   level 0 = black, 1 = dark gray, 2 = light gray, 3 = white (same scale as the renderer).
//   force = the pixel belongs to the corner itself and must be written even when white
//   (this wipes the text under it). Shadow pixels only add ink (force = false).
//
// Integer math only: the ESP32-C3 (X3/X4) has no floating-point unit.

#include <algorithm>
#include <cstdint>

namespace DogEar {

// The look is fixed on purpose. All lengths are % of the fold width unless noted.
namespace look {
constexpr int kSize = 22;            // fold width along the top edge, % of the screen's short side
constexpr int kCrease = 145;         // crease meets the right edge this far down (100 = 45 degrees)
constexpr int kTipX = 120;           // folded tip, distance from the right edge
constexpr int kTipY = 93;            // folded tip, distance from the top edge
constexpr int kBackground = 200;     // uncovered corner darkness 0..256 (flat, drawn as a fine pattern)
constexpr int kFlapShade = 25;       // gray on the flap towards the tip, 0..255
constexpr int kEdgeLevel = 0;        // flap outline: 0 = black
constexpr int kShadowDrop = 14;      // how far the tip's shadow falls down
constexpr int kShadowShift = 80;     // sideways offset of that shadow, % of the drop (+ = right)
constexpr int kShadowSoft = 0;       // soft fade around the shadow, % of the drop (0 = solid black, crisp)
constexpr int kShadowDarkness = 255; // shadow core darkness 0..255
}  // namespace look

namespace detail {

inline int bayer(const int x, const int y) {  // 0..63
  static constexpr uint8_t m[8][8] = {
      {0, 32, 8, 40, 2, 34, 10, 42},   {48, 16, 56, 24, 50, 18, 58, 26}, {12, 44, 4, 36, 14, 46, 6, 38},
      {60, 28, 52, 20, 62, 30, 54, 22}, {3, 35, 11, 43, 1, 33, 9, 41},   {51, 19, 59, 27, 49, 17, 57, 25},
      {15, 47, 7, 39, 13, 45, 5, 37},   {63, 31, 55, 23, 61, 29, 53, 21}};
  return m[y & 7][x & 7];
}

// Darkness 0..256 -> renderer level 0..3. With real gray tones the dither only blends
// neighbouring tones (smooth); without them it falls back to black/white dots.
inline int toLevel(const int x, const int y, const int darkness, const bool grayTones) {
  const int threshold = bayer(x, y) * 4 + 2;  // 2..254
  if (!grayTones) return darkness > threshold ? 0 : 3;
  const int scaled = darkness * 3;  // one gray step per 255
  int dark = scaled / 255;
  if ((scaled % 255) > threshold) ++dark;
  return 3 - std::min(dark, 3);
}

// Which side of the line a->b the point p is on; |value| / length = distance in pixels.
inline long side(const long ax, const long ay, const long bx, const long by, const long px, const long py) {
  return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

inline long isqrt(const long n) {  // once per edge, not per pixel
  long r = 0;
  while ((r + 1) * (r + 1) <= n) ++r;
  return std::max(1L, r);
}

inline long length(const long ax, const long ay, const long bx, const long by) {
  return isqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
}

}  // namespace detail

// Corner geometry in local coordinates: u = pixels from the right edge, v = pixels from the top.
//   A = crease meets the top edge, B = crease meets the right edge, T = the folded-over tip,
//   S = where the tip's shadow lands (the shadow is triangle A, B, S).
struct Shape {
  int ax, ay, bx, by, tx, ty, sx, sy;
  int soft;           // width of the soft fade around the shadow, pixels
  int width, height;  // area the dog-ear can touch, from the top-right corner
};

inline Shape shape(const int screenWidth, const int screenHeight) {
  using namespace look;
  const int s = std::max(16, std::min(screenWidth, screenHeight) * kSize / 100);
  Shape sh{};
  sh.ax = s;
  sh.ay = 0;
  sh.bx = 0;
  sh.by = s * kCrease / 100;
  sh.tx = s * kTipX / 100;
  sh.ty = s * kTipY / 100;
  const int drop = s * kShadowDrop / 100;
  sh.sx = sh.tx - drop * kShadowShift / 100;
  sh.sy = sh.ty + drop;
  sh.soft = drop * kShadowSoft / 100;
  sh.width = std::min(screenWidth, std::max({sh.ax, sh.tx, sh.sx}) + sh.soft + 3);
  sh.height = std::min(screenHeight, std::max({sh.by, sh.ty, sh.sy}) + sh.soft + 3);
  return sh;
}

// grayTones: true when the page gets a grayscale pass (real gray tones available).
template <typename Px>
void paint(const int screenWidth, const int screenHeight, const bool grayTones, Px&& px) {
  using namespace look;
  using detail::side;
  const Shape sh = shape(screenWidth, screenHeight);
  const int right = screenWidth - 1;

  // Winding of the triangles, so the inside tests work either way round.
  const long flapSign = side(sh.ax, sh.ay, sh.bx, sh.by, sh.tx, sh.ty) > 0 ? 1 : -1;
  const long shadowSign = side(sh.ax, sh.ay, sh.bx, sh.by, sh.sx, sh.sy) > 0 ? 1 : -1;
  const long lenAB = detail::length(sh.ax, sh.ay, sh.bx, sh.by);
  const long lenBT = detail::length(sh.bx, sh.by, sh.tx, sh.ty);
  const long lenTA = detail::length(sh.tx, sh.ty, sh.ax, sh.ay);
  const long lenBS = detail::length(sh.bx, sh.by, sh.sx, sh.sy);
  const long lenSA = detail::length(sh.sx, sh.sy, sh.ax, sh.ay);
  const long cornerArea = static_cast<long>(sh.ax) * sh.by;
  const int flapDepth = std::max(1, sh.tx);

  for (int v = 0; v < sh.height; ++v) {
    for (int u = 0; u < sh.width; ++u) {
      const int x = right - u;
      const int y = v;

      // 1) Uncovered corner: one flat tone, always a fine pattern of pure black and white
      //    pixels (a large area of real e-ink gray lets the old page show through).
      if (static_cast<long>(u) * sh.by + static_cast<long>(v) * sh.ax < cornerArea) {
        px(x, y, detail::toLevel(x, y, kBackground, false), true);
        continue;
      }

      // 2) The folded flap: triangle A, B, T.
      const long s1 = side(sh.ax, sh.ay, sh.bx, sh.by, u, v) * flapSign;
      const long s2 = side(sh.bx, sh.by, sh.tx, sh.ty, u, v) * flapSign;
      const long s3 = side(sh.tx, sh.ty, sh.ax, sh.ay, u, v) * flapSign;
      if (s1 >= 0 && s2 >= 0 && s3 >= 0) {
        if (s2 < lenBT || s3 < lenTA) {  // within one pixel of a free edge: fine outline
          px(x, y, grayTones ? kEdgeLevel : 0, true);
          continue;
        }
        // Back of the page: white, with just a breath of gray towards the tip.
        const int dCrease = static_cast<int>(s1 / lenAB);
        const int darkness = std::min(kFlapShade, kFlapShade * dCrease / flapDepth);
        px(x, y, detail::toLevel(x, y, darkness, grayTones), true);
        continue;
      }

      // 3) Drop shadow of the lifted flap: triangle A, B, S. Solid inside, soft fade just
      //    outside its free edges. Only adds ink, the text underneath stays.
      if (s3 < 0) continue;  // shadow falls to the right: nothing left of the flap
      const long q2 = side(sh.bx, sh.by, sh.sx, sh.sy, u, v) * shadowSign;
      const long q3 = side(sh.sx, sh.sy, sh.ax, sh.ay, u, v) * shadowSign;
      int darkness = 0;
      if (q2 >= 0 && q3 >= 0) {
        darkness = kShadowDarkness;
      } else if (sh.soft > 0) {
        long d = 0;
        if (q2 < 0) d = std::max(d, -q2 / lenBS);
        if (q3 < 0) d = std::max(d, -q3 / lenSA);
        if (d < sh.soft) {
          const long f = (sh.soft - d) * 256 / sh.soft;  // 256 at the shadow, 0 at the end of the fade
          darkness = static_cast<int>(kShadowDarkness * f * f / (256 * 256));
        }
      }
      if (darkness > 0) {
        const int level = detail::toLevel(x, y, darkness, grayTones);
        if (level < 3) px(x, y, level, false);
      }
    }
  }
}

// True when a touch at (x, y) lands on the dog-ear (a little larger than the drawing).
inline bool hitsCorner(const int screenWidth, const int screenHeight, const int x, const int y) {
  const Shape sh = shape(screenWidth, screenHeight);
  const int zone = std::max(sh.tx, sh.by) * 5 / 4;
  return x >= screenWidth - zone && y < zone;
}

}  // namespace DogEar
