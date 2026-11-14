// Copyright 2026 The Omarchium Authors
// Use of this source code is governed by a BSD-style license.

#ifndef OMARCHIUM_BROWSER_THEME_OMARCHY_PALETTE_H_
#define OMARCHIUM_BROWSER_THEME_OMARCHY_PALETTE_H_

#include <optional>
#include <string_view>

#include "third_party/skia/include/core/SkColor.h"

namespace omarchium {

// The colors of the active Omarchy theme, as written by `omarchy-theme-set`
// to ~/.local/state/omarchy/current/theme/colors.toml.
//
// Only `background`, `foreground` and `accent` are required; every other
// role falls back to a blend of those, so hand-written or older themes work.
struct OmarchyPalette {
  bool dark = true;

  SkColor accent = SK_ColorTRANSPARENT;
  SkColor selection = SK_ColorTRANSPARENT;
  SkColor muted = SK_ColorTRANSPARENT;

  SkColor background = SK_ColorTRANSPARENT;
  SkColor dark_background = SK_ColorTRANSPARENT;     // a step darker
  SkColor darker_background = SK_ColorTRANSPARENT;   // two steps darker
  SkColor lighter_background = SK_ColorTRANSPARENT;  // raised surface

  SkColor foreground = SK_ColorTRANSPARENT;
  SkColor dark_foreground = SK_ColorTRANSPARENT;   // dim / subtle text
  SkColor light_foreground = SK_ColorTRANSPARENT;  // secondary text

  SkColor red = SK_ColorTRANSPARENT;
  SkColor yellow = SK_ColorTRANSPARENT;
  SkColor green = SK_ColorTRANSPARENT;
  SkColor cyan = SK_ColorTRANSPARENT;
  SkColor blue = SK_ColorTRANSPARENT;
  SkColor magenta = SK_ColorTRANSPARENT;

  // Parses colors.toml. Returns nullopt if a required color is missing.
  static std::optional<OmarchyPalette> Parse(std::string_view toml);

  // Whichever of background/foreground reads best on top of `color`.
  SkColor ContrastingText(SkColor color) const;

  bool operator==(const OmarchyPalette&) const = default;
};

}  // namespace omarchium

#endif  // OMARCHIUM_BROWSER_THEME_OMARCHY_PALETTE_H_
