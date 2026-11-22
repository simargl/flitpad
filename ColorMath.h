// ColorMath.h - colour helpers without any toolkit dependency.
//
// Why this exists: FLTK's text widget passes every style colour through
// fl_contrast() against the editor background. If a colour's luminance is within
// about 100 of the background's, FLTK silently replaces it with plain white or
// black. Solarized's base03 background is so dark that most of its accent colours
// (orange, blue, cyan, violet, magenta, red, base01) fall inside that range and
// would all be drawn white. lifted() keeps the hue and brightens the colour just
// enough to survive.
#ifndef COLOR_MATH_H
#define COLOR_MATH_H

namespace ColorMath {

// Luminance as FLTK computes it (0..255).
inline int luminance(unsigned rgb) {
    return static_cast<int>((31u * ((rgb >> 16) & 255u) + 61u * ((rgb >> 8) & 255u) + 8u * (rgb & 255u)) / 100u);
}

// `rgb` (0xRRGGBB) blended towards white until its luminance exceeds that of `bg`
// by at least `min_diff`. Colours that already pass are returned unchanged.
// FLTK's own threshold is about 99; the default leaves a safety margin.
inline unsigned lifted(unsigned rgb, unsigned bg, int min_diff = 112) {
    const int l = luminance(rgb), target = luminance(bg) + min_diff;
    if (l >= target || target >= 255) return rgb;
    // the weights sum to 100%, so luminance moves linearly with the blend factor
    const double t = static_cast<double>(target - l) / static_cast<double>(255 - l);
    unsigned out = 0;
    for (int shift = 16; shift >= 0; shift -= 8) {
        const int c = static_cast<int>((rgb >> shift) & 255u);
        int v = static_cast<int>(c + (255 - c) * t + 0.999);   // round up so we never fall short
        if (v > 255) v = 255;
        out |= static_cast<unsigned>(v) << shift;
    }
    return out;
}

}  // namespace ColorMath

#endif
