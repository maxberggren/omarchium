// Copyright 2026 The Omarchium Authors
// Use of this source code is governed by a BSD-style license.

#include "omarchium/browser/theme/omarchy_palette.h"

#include <cstdint>
#include <map>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "ui/gfx/color_utils.h"

namespace omarchium {

namespace {

// Accepts "#rgb", "#rrggbb" and "#rrggbbaa" (alpha ignored; UI surfaces must
// be opaque).
std::optional<SkColor> ParseHexColor(std::string_view value) {
  if (!value.starts_with('#')) {
    return std::nullopt;
  }
  std::string hex(value.substr(1));
  if (hex.size() == 3) {
    hex = {hex[0], hex[0], hex[1], hex[1], hex[2], hex[2]};
  } else if (hex.size() == 8) {
    hex.resize(6);
  }
  uint32_t rgb = 0;
  if (hex.size() != 6 || !base::HexStringToUInt(hex, &rgb)) {
    return std::nullopt;
  }
  return SkColorSetA(rgb, SK_AlphaOPAQUE);
}

// Minimal TOML reader for Omarchy's flat `key = "value"` files: one pair per
// line, `#` comments, single or double quotes. Tables/arrays are skipped.
std::map<std::string, std::string, std::less<>> ParseFlatToml(
    std::string_view toml) {
  std::map<std::string, std::string, std::less<>> values;
  for (std::string_view line : base::SplitStringPiece(
           toml, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (line.starts_with('#') || line.starts_with('[')) {
      continue;
    }
    const size_t eq = line.find('=');
    if (eq == std::string_view::npos) {
      continue;
    }
    std::string_view key =
        base::TrimWhitespaceASCII(line.substr(0, eq), base::TRIM_ALL);
    std::string_view value =
        base::TrimWhitespaceASCII(line.substr(eq + 1), base::TRIM_ALL);
    if (!value.empty() && (value[0] == '"' || value[0] == '\'')) {
      const size_t close = value.find(value[0], 1);
      if (close == std::string_view::npos) {
        continue;
      }
      value = value.substr(1, close - 1);
    } else {
      // Bare value: drop a trailing comment.
      value = base::TrimWhitespaceASCII(value.substr(0, value.find('#')),
                                        base::TRIM_ALL);
    }
    values.insert_or_assign(std::string(key), std::string(value));
  }
  return values;
}

}  // namespace

// static
std::optional<OmarchyPalette> OmarchyPalette::Parse(std::string_view toml) {
  const auto values = ParseFlatToml(toml);
  auto color = [&](std::string_view key) -> std::optional<SkColor> {
    auto it = values.find(key);
    return it == values.end() ? std::nullopt : ParseHexColor(it->second);
  };

  OmarchyPalette p;
  auto background = color("background");
  auto foreground = color("foreground");
  if (!background || !foreground) {
    return std::nullopt;
  }
  p.background = *background;
  p.foreground = *foreground;

  if (auto it = values.find("mode"); it != values.end()) {
    p.dark = base::ToLowerASCII(it->second) != "light";
  } else {
    p.dark = color_utils::IsDark(p.background);
  }

  const SkColor bg = p.background;
  const SkColor fg = p.foreground;
  auto blend = [](SkColor fg_color, SkColor bg_color, float weight_of_fg) {
    return color_utils::AlphaBlend(fg_color, bg_color, weight_of_fg);
  };

  p.accent = color("accent").value_or(color("blue").value_or(fg));
  p.selection = color("selection").value_or(blend(p.accent, bg, 0.3f));
  p.muted = color("muted").value_or(blend(fg, bg, 0.35f));

  p.dark_background =
      color("dark_background").value_or(blend(SK_ColorBLACK, bg, 0.12f));
  p.darker_background =
      color("darker_background").value_or(blend(SK_ColorBLACK, bg, 0.25f));
  p.lighter_background = color("lighter_background")
                             .value_or(p.dark ? blend(SK_ColorWHITE, bg, 0.08f)
                                              : blend(SK_ColorBLACK, bg, 0.06f));

  p.dark_foreground = color("dark_foreground").value_or(blend(fg, bg, 0.5f));
  p.light_foreground = color("light_foreground").value_or(blend(fg, bg, 0.8f));

  p.red = color("red").value_or(SkColorSetRGB(0xd0, 0x4a, 0x4a));
  p.yellow = color("yellow").value_or(SkColorSetRGB(0xd8, 0xa6, 0x57));
  p.green = color("green").value_or(SkColorSetRGB(0x6a, 0xa8, 0x4f));
  p.blue = color("blue").value_or(p.accent);
  p.cyan = color("cyan").value_or(p.blue);
  p.magenta = color("magenta").value_or(p.accent);
  return p;
}

SkColor OmarchyPalette::ContrastingText(SkColor color) const {
  return color_utils::GetContrastRatio(color, foreground) >=
                 color_utils::GetContrastRatio(color, background)
             ? foreground
             : background;
}

}  // namespace omarchium
