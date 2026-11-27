// ColorMath.h - colour helpers without any toolkit dependency.
//
// Why this exists: FLTK's text widget passes every style colour through
// fl_contrast() against the editor background. If a colour's luminance is within
// about 100 of the background's, FLTK silently replaces it with plain white or
// black. Solarized's base03 background is so dark that most of its accent colours
// (orange, blue, cyan, violet, magenta, red, base01) fall inside that range and
// would all be drawn white; on a light background the same happens with black.
// ensure_contrast() keeps the hue and moves the colour just far enough to survive.
#ifndef COLOR_MATH_H
#define COLOR_MATH_H

namespace ColorMath {

// Luminance as FLTK computes it (0..255).
inline int luminance(unsigned rgb) {
    return static_cast<int>((31u * ((rgb >> 16) & 255u) + 61u * ((rgb >> 8) & 255u) + 8u * (rgb & 255u)) / 100u);
}

// a*(1-t) + b*t per channel, both 0xRRGGBB.
inline unsigned blend(unsigned a, unsigned b, double t) {
    unsigned out = 0;
    for (int shift = 16; shift >= 0; shift -= 8) {
        const double ca = static_cast<double>((a >> shift) & 255u), cb = static_cast<double>((b >> shift) & 255u);
        int v = static_cast<int>(ca + (cb - ca) * t + 0.5);
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        out |= static_cast<unsigned>(v) << shift;
    }
    return out;
}

// Returns `rgb` (0xRRGGBB) adjusted so that FLTK keeps it on background `bg`: its
// luminance must differ from the background's by at least `min_diff`. On a dark
// background the colour is blended towards white, on a light one towards black,
// so the hue is kept. Colours that already pass are returned unchanged.
// FLTK's own threshold is about 99; the default leaves a safety margin.
inline unsigned ensure_contrast(unsigned rgb, unsigned bg, int min_diff = 112) {
    const int l = luminance(rgb), lb = luminance(bg);
    if (l >= lb + min_diff || l <= lb - min_diff) return rgb;
    unsigned out = 0;
    if (lb < 128) {                                   // dark background: lighten
        const int target = lb + min_diff;
        if (target >= 255) return rgb;
        // the weights sum to 100%, so luminance moves linearly with the blend factor
        const double t = static_cast<double>(target - l) / static_cast<double>(255 - l);
        for (int shift = 16; shift >= 0; shift -= 8) {
            const int c = static_cast<int>((rgb >> shift) & 255u);
            int v = static_cast<int>(c + (255 - c) * t + 0.999);   // round up: never fall short
            if (v > 255) v = 255;
            out |= static_cast<unsigned>(v) << shift;
        }
    } else {                                          // light background: darken
        const int target = lb - min_diff;
        if (target <= 0 || l <= 0) return rgb;
        const double t = static_cast<double>(l - target) / static_cast<double>(l);
        for (int shift = 16; shift >= 0; shift -= 8) {
            const int c = static_cast<int>((rgb >> shift) & 255u);
            out |= static_cast<unsigned>(static_cast<int>(c * (1.0 - t))) << shift;   // round down
        }
    }
    return out;
}

}  // namespace ColorMath

#endif
