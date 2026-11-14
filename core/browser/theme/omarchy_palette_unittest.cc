// Copyright 2026 The Omarchium Authors
// Use of this source code is governed by a BSD-style license.

#include "omarchium/browser/theme/omarchy_palette.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace omarchium {
namespace {

constexpr char kGruvbox[] = R"(mode = "dark"

accent = "#7daea3"
selection = "#504945"   # inline comments are fine
muted = '#665c54'
background = "#282828"
dark_background = "#1e1e1e"
darker_background = "#161616"
lighter_background = "#3c3836"
foreground = "#d4be98"
dark_foreground = "#7c6f64"
light_foreground = "#bdae93"
red = "#ea6962"
[ignored.table]
magenta = "#d3869b"
)";

TEST(OmarchyPaletteTest, ParsesOmarchyColorsToml) {
  auto p = OmarchyPalette::Parse(kGruvbox);
  ASSERT_TRUE(p);
  EXPECT_TRUE(p->dark);
  EXPECT_EQ(p->accent, SkColorSetRGB(0x7d, 0xae, 0xa3));
  EXPECT_EQ(p->selection, SkColorSetRGB(0x50, 0x49, 0x45));
  EXPECT_EQ(p->muted, SkColorSetRGB(0x66, 0x5c, 0x54));
  EXPECT_EQ(p->background, SkColorSetRGB(0x28, 0x28, 0x28));
  EXPECT_EQ(p->lighter_background, SkColorSetRGB(0x3c, 0x38, 0x36));
  EXPECT_EQ(p->red, SkColorSetRGB(0xea, 0x69, 0x62));
  EXPECT_EQ(p->magenta, SkColorSetRGB(0xd3, 0x86, 0x9b));
  // Missing roles fall back sensibly.
  EXPECT_EQ(p->blue, p->accent);
  EXPECT_EQ(p->cyan, p->accent);
}

TEST(OmarchyPaletteTest, RequiresBackgroundAndForeground) {
  EXPECT_FALSE(OmarchyPalette::Parse("background = \"#000000\"\n"));
  EXPECT_FALSE(OmarchyPalette::Parse("foreground = \"#ffffff\"\n"));
  EXPECT_FALSE(OmarchyPalette::Parse(""));
}

TEST(OmarchyPaletteTest, InfersModeAndShortHex) {
  auto p = OmarchyPalette::Parse("background = \"#fff\"\nforeground = \"#000\"\n");
  ASSERT_TRUE(p);
  EXPECT_FALSE(p->dark);
  EXPECT_EQ(p->background, SK_ColorWHITE);
  EXPECT_EQ(p->accent, SK_ColorBLACK);  // no accent/blue: foreground
}

TEST(OmarchyPaletteTest, ContrastingText) {
  auto p = OmarchyPalette::Parse(kGruvbox);
  ASSERT_TRUE(p);
  EXPECT_EQ(p->ContrastingText(SkColorSetRGB(0x7d, 0xae, 0xa3)), p->background);
  EXPECT_EQ(p->ContrastingText(SkColorSetRGB(0x10, 0x10, 0x10)), p->foreground);
}

}  // namespace
}  // namespace omarchium
