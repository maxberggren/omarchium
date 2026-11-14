// Copyright 2026 The Omarchium Authors
// Use of this source code is governed by a BSD-style license.

#include "omarchium/browser/theme/omarchy_color_map.h"

#include "omarchium/browser/theme/omarchy_palette.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"

namespace omarchium {
namespace {

OmarchyPalette Palette() {
  return *OmarchyPalette::Parse(R"(
mode = "dark"
accent = "#7daea3"
background = "#282828"
foreground = "#d4be98"
lighter_background = "#3c3836"
dark_background = "#1e1e1e"
)");
}

SkColor Get(const OmarchyColorMap::Resolved& r, std::string_view name) {
  for (const auto& c : r.sys) {
    if (c.name == name) return c.color;
  }
  for (const auto& c : r.chrome) {
    if (c.name == name) return c.color;
  }
  ADD_FAILURE() << name << " not resolved";
  return SK_ColorTRANSPARENT;
}

TEST(OmarchyColorMapTest, DefaultMapParsesCleanly) {
  std::vector<std::string> errors;
  auto map = OmarchyColorMap::CreateDefault();
  ASSERT_TRUE(map);
  auto resolved = map->Evaluate(Palette(), &errors);
  EXPECT_TRUE(errors.empty()) << errors.front();
  EXPECT_GT(resolved.sys.size(), 60u);
  EXPECT_EQ(Get(resolved, "kColorSysPrimary"), SkColorSetRGB(0x7d, 0xae, 0xa3));
  EXPECT_EQ(Get(resolved, "kColorSysOmniboxContainer"),
            SkColorSetRGB(0x3c, 0x38, 0x36));
}

TEST(OmarchyColorMapTest, Functions) {
  std::vector<std::string> errors;
  auto map = OmarchyColorMap::Parse(R"json({
    "vars": { "b": "a", "a": "mix(white, black, 0.5)" },
    "sys": {
      "kColorSysPrimary": "b",
      "kColorSysOnPrimary": "alpha(foreground, 0.5)",
      "kColorSysSecondary": "contrast(accent)",
      "kColorSysTertiary": "dark(#ff0000, #00ff00)"
    },
    "chrome": { "kColorToolbar": "#123" }
  })json", &errors);
  ASSERT_TRUE(map);
  ASSERT_TRUE(errors.empty()) << errors.front();
  auto r = map->Evaluate(Palette(), &errors);
  ASSERT_TRUE(errors.empty()) << errors.front();
  EXPECT_EQ(Get(r, "kColorSysPrimary"), SkColorSetRGB(0x80, 0x80, 0x80));
  EXPECT_EQ(Get(r, "kColorSysOnPrimary"), SkColorSetARGB(0x80, 0xd4, 0xbe, 0x98));
  EXPECT_EQ(Get(r, "kColorSysSecondary"), SkColorSetRGB(0x28, 0x28, 0x28));
  EXPECT_EQ(Get(r, "kColorSysTertiary"), SK_ColorRED);
  EXPECT_EQ(Get(r, "kColorToolbar"), SkColorSetRGB(0x11, 0x22, 0x33));
}

TEST(OmarchyColorMapTest, ReportsErrors) {
  std::vector<std::string> errors;
  auto map = OmarchyColorMap::Parse(R"json({
    "vars": { "x": "y", "y": "x" },
    "sys": {
      "kColorNotARealId": "accent",
      "kColorSysPrimary": "mix(accent, background)",
      "kColorSysSecondary": "x",
      "kColorSysTertiary": "nope"
    },
    "shape": { "address_bar_radius": "round" }
  })json", &errors);
  ASSERT_TRUE(map);
  EXPECT_EQ(errors.size(), 2u);  // unknown id, bad shape value
  auto r = map->Evaluate(Palette(), &errors);
  EXPECT_EQ(errors.size(), 5u);  // + wrong arity, var cycle, unknown name
  EXPECT_TRUE(r.sys.empty());
  EXPECT_FALSE(OmarchyColorMap::Parse("{nope", &errors));
}

TEST(OmarchyColorMapTest, ShapeScalesRelativeToAddressBar) {
  std::vector<std::string> errors;
  auto map = OmarchyColorMap::CreateDefault();
  const auto& shape = map->shape();
  EXPECT_EQ(shape.mode, OmarchyColorMap::Shape::Mode::kHyprland);
  EXPECT_EQ(shape.Scale(17, 6), 6);   // the address bar itself
  EXPECT_EQ(shape.Scale(17, 0), 0);   // square windows, square UI
  EXPECT_EQ(shape.Scale(8, 6), 3);    // buttons keep their proportion
  EXPECT_EQ(shape.Scale(34, 6), 12);
  EXPECT_EQ(shape.Scale(0, 6), 0);
}

TEST(OmarchyColorMapTest, InterpolateBlendsTokenByToken) {
  OmarchyColorMap::Resolved from{{{ui::kColorSysPrimary, "p", SK_ColorBLACK}},
                                 {}};
  OmarchyColorMap::Resolved to{{{ui::kColorSysPrimary, "p", SK_ColorWHITE},
                                {ui::kColorSysSecondary, "s", SK_ColorRED}},
                               {}};
  auto half = OmarchyColorMap::Interpolate(from, to, 0.5f);
  ASSERT_EQ(half.sys.size(), 2u);
  EXPECT_EQ(half.sys[0].color, SkColorSetRGB(0x80, 0x80, 0x80));
  EXPECT_EQ(half.sys[1].color, SK_ColorRED);  // new token: no fade source
  EXPECT_EQ(OmarchyColorMap::Interpolate(from, to, 0).sys[0].color,
            SK_ColorBLACK);
  EXPECT_EQ(OmarchyColorMap::Interpolate(from, to, 1).sys[0].color,
            SK_ColorWHITE);
}

TEST(OmarchyColorMapTest, TransitionDefaultsAndParse) {
  EXPECT_EQ(OmarchyColorMap::CreateDefault()->transition().duration_ms, 300);
  std::vector<std::string> errors;
  auto map = OmarchyColorMap::Parse(R"json({"transition": {"duration_ms": 0}})json",
                                    &errors);
  ASSERT_TRUE(map);
  EXPECT_EQ(map->transition().duration_ms, 0);
}

}  // namespace
}  // namespace omarchium
