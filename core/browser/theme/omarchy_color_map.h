// Copyright 2026 The Omarchium Authors
// Use of this source code is governed by a BSD-style license.

#ifndef OMARCHIUM_BROWSER_THEME_OMARCHY_COLOR_MAP_H_
#define OMARCHIUM_BROWSER_THEME_OMARCHY_COLOR_MAP_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"

namespace omarchium {

struct OmarchyPalette;

// How Omarchy palette roles become Chromium colors; see
// core/resources/omarchy_color_map.json for the format. The same file drives
// the HTML preview (preview/colormap.js), which mirrors this evaluator.
class OmarchyColorMap {
 public:
  // A parsed value expression.
  struct Expr {
    enum class Kind { kName, kColor, kNumber, kCall };
    Kind kind = Kind::kName;
    std::string name;  // kName: role/var name; kCall: function name
    SkColor color = SK_ColorTRANSPARENT;
    float number = 0;
    std::vector<Expr> args;
  };

  struct Entry {
    std::string name;  // e.g. "kColorSysPrimary"
    ui::ColorId id;
    Expr expr;
  };

  struct ResolvedColor {
    ui::ColorId id;
    std::string name;
    SkColor color;
  };
  struct Resolved {
    std::vector<ResolvedColor> sys;
    std::vector<ResolvedColor> chrome;
  };

  // The "shape" section: the address bar gets `address_bar_radius`, every
  // other radius scales by the same factor.
  struct Shape {
    enum class Mode { kStock, kHyprland, kFixed };
    Mode mode = Mode::kHyprland;
    int address_bar_radius = 0;  // kFixed only
    int stock_address_bar_radius = 17;

    // `radius` scaled for an address bar radius of `target`.
    int Scale(int radius, int target) const;
  };

  // The "font" section.
  struct Font {
    enum class Mode { kStock, kOmarchy, kFamily };
    Mode mode = Mode::kOmarchy;
    std::string family;  // kFamily only
  };

  OmarchyColorMap();
  OmarchyColorMap(const OmarchyColorMap&) = delete;
  OmarchyColorMap& operator=(const OmarchyColorMap&) = delete;
  ~OmarchyColorMap();

  // Parses a color map. Unknown color ids and bad expressions are reported
  // in `errors` and skipped; a JSON syntax error returns nullptr.
  static std::unique_ptr<OmarchyColorMap> Parse(
      std::string_view json,
      std::vector<std::string>* errors);

  // The map compiled into the browser.
  static std::unique_ptr<OmarchyColorMap> CreateDefault();

  // Evaluates every entry against `palette`. Entries that fail (for example
  // a var cycle) are reported in `errors` and left out.
  Resolved Evaluate(const OmarchyPalette& palette,
                    std::vector<std::string>* errors) const;

  const std::string& source() const { return source_; }
  const Shape& shape() const { return shape_; }
  const Font& font() const { return font_; }

  // Exposed for tests.
  static std::optional<Expr> ParseExpr(std::string_view src,
                                       std::string* error);

 private:
  std::string source_;
  Shape shape_;
  Font font_;
  std::map<std::string, Expr, std::less<>> vars_;
  std::vector<Entry> sys_;
  std::vector<Entry> chrome_;
};

}  // namespace omarchium

#endif  // OMARCHIUM_BROWSER_THEME_OMARCHY_COLOR_MAP_H_
