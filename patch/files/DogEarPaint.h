#pragma once
// Dog-ear (folded page corner) painter. Hardware-free so it can be previewed on a PC.
// Draws in the top-right corner of a screen that is `screenWidth` pixels wide.
//
// px(x, y, level, force):
//   level 0 = black, 1 = dark gray, 2 = light gray, 3 = white (same scale as the renderer).
//   force = the pixel belongs to the corner itself and must be written even when white
//   (this wipes the text under it). Shadow pixels only add ink (force = false).

#include <algorithm>
#include <cstdint>

namespace DogEar {

// Look of the dog-ear. All values can be tuned on the device in /dogear.json (see README).
struct Look {
  int size = 22;            // fold width along the top edge, % of the screen's short side
  int crease = 145;         // how far down the right edge the crease ends, % of the fold width (100 = 45 degrees)
  int tipX = 120;           // folded tip position, % of the fold width (from the right edge)
  int tipY = 93;            // folded tip position, % of the fold width (from the top edge)
  int background = 1;       // uncovered corner: 1 = dark gray, 2 = light gray (one flat tone)
  int flapShade = 25;       // gray on the flap towards the tip, 0 (none) .. 255
  int edge = 0;             // flap outline: 0 = black, 1 = dark gray, 2 = light gray, 3 = none
  int shadowWidth = 10;     // how far the tip's shadow falls downwards, % of the fold width (0 = none)
  int shadowStrength = 100; // shadow darkness at its darkest, 0 .. 100 (100 = black)
  int shadowSoft = 60;      // soft fade around the shadow, % of the shadow drop (0 = razor sharp)
  int shadowShift = 80;     // sideways offset of the tip's shadow, % of the drop (+ = right, towards the crease)

  void clampAll() {
    size = std::clamp(size, 5, 45);
    crease = std::clamp(crease, 50, 250);
    tipX = std::clamp(tipX, 40, 250);
    tipY = std::clamp(tipY, 20, 250);
    background = std::clamp(background, 1, 2);
    flapShade = std::clamp(flapShade, 0, 255);
    edge = std::clamp(edge, 0, 3);
    shadowWidth = std::clamp(shadowWidth, 0, 50);
    shadowStrength = std::clamp(shadowStrength, 0, 100);
    shadowShift = std::clamp(shadowShift, -200, 200);
    shadowSoft = std::clamp(shadowSoft, 0, 200);
  }
};

namespace detail {

inline int bayer(const int x, const int y) {  // 0..63
  static constexpr uint8_t m[8][8] = {
      {0, 32, 8, 40, 2, 34, 10, 42},   {48, 16, 56, 24, 50, 18, 58, 26}, {12, 44, 4, 36, 14, 46, 6, 38},
      {60, 28, 52, 20, 62, 30, 54, 22}, {3, 35, 11, 43, 1, 33, 9, 41},   {51, 19, 59, 27, 49, 17, 57, 25},
      {15, 47, 7, 39, 13, 45, 5, 37},   {63, 31, 55, 23, 61, 29, 53, 21}};
  return m[y & 7][x & 7];
}

// Darkness 0..255 -> renderer level 0..3. With real gray tones the dither only blends
// neighbouring tones (smooth); without them it falls back to black/white dots.
inline int toLevel(const int x, const int y, const int darkness, const bool grayTones) {
  const int threshold = bayer(x, y) * 4 + 2;  // 2..254
  if (!grayTones) return darkness > threshold ? 0 : 3;
  const int scaled = darkness * 3;  // 0..765, one gray step per 255
  int dark = scaled / 255;
  if ((scaled % 255) > threshold) ++dark;
  return 3 - std::min(dark, 3);
}

// Which side of the line a->b the point p is on (sign only matters).
inline long side(const long ax, const long ay, const long bx, const long by, const long px, const long py) {
  return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

// Distance (whole pixels) from p to segment a-b, and where along it the closest point is (0..1024).
inline void segDistance(const int ax, const int ay, const int bx, const int by, const int px, const int py,
                        int& dist, int& along1024) {
  const long dx = bx - ax, dy = by - ay;
  const long len2 = dx * dx + dy * dy;
  long t = len2 > 0 ? ((px - ax) * dx + (py - ay) * dy) * 1024 / len2 : 0;
  t = std::clamp(t, 0L, 1024L);
  const long ex = px * 1024L - (ax * 1024L + dx * t);
  const long ey = py * 1024L - (ay * 1024L + dy * t);
  const long d2 = (ex / 64) * (ex / 64) + (ey / 64) * (ey / 64);  // in (1/16 px)^2
  long d = 0;
  while ((d + 1) * (d + 1) <= d2) ++d;  // integer sqrt; values stay small (corner only)
  dist = static_cast<int>(d / 16);
  along1024 = static_cast<int>(t);
}

}  // namespace detail

// Corner geometry in local coordinates: u = pixels from the right edge, v = pixels from the top.
//   A = crease meets the top edge, B = crease meets the right edge, T = the folded-over tip.
struct Shape {
  int ax, ay, bx, by, tx, ty;
  int sx, sy;  // where the tip's shadow lands (the shadow is triangle A, B, S)
  int soft;    // width of the soft fade around the shadow, pixels
};

inline Shape shape(const Look& look, const int screenWidth, const int screenHeight) {
  const int s = std::max(16, std::min(screenWidth, screenHeight) * look.size / 100);
  const int tx = s * look.tipX / 100;
  const int ty = s * look.tipY / 100;
  const int drop = s * look.shadowWidth / 100;
  return {s, 0, 0, s * look.crease / 100, tx, ty, tx - drop * look.shadowShift / 100, ty + drop,
          drop * look.shadowSoft / 100};
}

// grayTones: true when the page gets a grayscale pass (real gray tones available).
template <typename Px>
void paint(const Look& look, const int screenWidth, const int screenHeight, const bool grayTones, Px&& px) {
  const Shape sh = shape(look, screenWidth, screenHeight);
  const int right = screenWidth - 1;
  const int extentU = std::max({sh.ax, sh.tx, sh.sx}) + sh.soft + 2;
  const int extentV = std::max({sh.by, sh.ty, sh.sy}) + sh.soft + 2;
  // Winding of the flap triangle, so the inside test works either way round.
  const long flapSign = detail::side(sh.ax, sh.ay, sh.bx, sh.by, sh.tx, sh.ty) > 0 ? 1 : -1;
  const long shadowSign = detail::side(sh.ax, sh.ay, sh.bx, sh.by, sh.sx, sh.sy) > 0 ? 1 : -1;
  const int maxDark = 255 * look.shadowStrength / 100;

  for (int v = 0; v <= extentV; ++v) {
    for (int u = 0; u <= extentU; ++u) {
      const int x = right - u;
      const int y = v;
      if (x < 0 || y >= screenHeight) continue;

      // 1) Uncovered corner: one flat gray tone, no gradient (u/ax + v/by < 1).
      if (static_cast<long>(u) * sh.by + static_cast<long>(v) * sh.ax < static_cast<long>(sh.ax) * sh.by) {
        px(x, y, grayTones ? look.background : detail::toLevel(x, y, 255 - 85 * look.background, false), true);
        continue;
      }

      // 2) The folded flap: triangle A, B, T.
      const long s1 = detail::side(sh.ax, sh.ay, sh.bx, sh.by, u, v) * flapSign;
      const long s2 = detail::side(sh.bx, sh.by, sh.tx, sh.ty, u, v) * flapSign;
      const long s3 = detail::side(sh.tx, sh.ty, sh.ax, sh.ay, u, v) * flapSign;
      if (s1 >= 0 && s2 >= 0 && s3 >= 0) {
        int dTop = 0, dBottom = 0, along = 0;
        detail::segDistance(sh.ax, sh.ay, sh.tx, sh.ty, u, v, dTop, along);
        detail::segDistance(sh.tx, sh.ty, sh.bx, sh.by, u, v, dBottom, along);
        if ((dTop == 0 || dBottom == 0) && look.edge < 3) {
          // Fine outline, so the white flap reads on a white page.
          px(x, y, grayTones ? look.edge : (look.edge < 2 ? 0 : detail::toLevel(x, y, 85, false)), true);
          continue;
        }
        // Back of the page: white, with just a breath of gray towards the tip.
        int dCrease = 0;
        detail::segDistance(sh.ax, sh.ay, sh.bx, sh.by, u, v, dCrease, along);
        const int darkness = std::min(look.flapShade, look.flapShade * dCrease / std::max(1, sh.tx));
        px(x, y, detail::toLevel(x, y, darkness, grayTones), true);
        continue;
      }

      // 3) Drop shadow of the lifted flap: triangle A, B, S (S = the tip moved down and sideways).
      //    Solid inside, soft fade just outside its free edges. Only adds ink, the text stays.
      if (s1 < 0) continue;  // the shadow never reaches the uncovered corner side of the crease
      if (look.shadowShift >= 0 && s3 < 0) continue;  // shadow falls right: nothing left of the flap
      const long q2 = detail::side(sh.bx, sh.by, sh.sx, sh.sy, u, v) * shadowSign;
      const long q3 = detail::side(sh.sx, sh.sy, sh.ax, sh.ay, u, v) * shadowSign;
      int darkness = 0;
      if (q2 >= 0 && q3 >= 0) {
        darkness = maxDark;
      } else if (sh.soft > 0) {
        int d = 1 << 20, along = 0, dEdge = 0;
        if (q2 < 0) {
          detail::segDistance(sh.bx, sh.by, sh.sx, sh.sy, u, v, dEdge, along);
          d = std::min(d, dEdge);
        }
        if (q3 < 0) {
          detail::segDistance(sh.sx, sh.sy, sh.ax, sh.ay, u, v, dEdge, along);
          d = std::min(d, dEdge);
        }
        if (d < sh.soft) {
          const int f = (sh.soft - d) * 256 / sh.soft;  // 256 at the shadow, 0 at the end of the fade
          darkness = maxDark * f * f / (256 * 256);
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
inline bool hitsCorner(const Look& look, const int screenWidth, const int screenHeight, const int x, const int y) {
  const Shape sh = shape(look, screenWidth, screenHeight);
  const int zone = std::max(sh.tx, sh.by) * 5 / 4;
  return x >= screenWidth - zone && y < zone;
}

}  // namespace DogEar
