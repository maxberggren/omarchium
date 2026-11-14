// Copyright 2026 The Omarchium Authors
// Use of this source code is governed by a BSD-style license.

#include "omarchium/browser/theme/omarchy_color_map.h"

#include <cmath>
#include <set>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "components/color/color_id.h"
#include "omarchium/browser/theme/omarchy_palette.h"
#include "omarchium/grit/omarchy_color_map_default.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_utils.h"

namespace omarchium {

namespace {

using Expr = OmarchyColorMap::Expr;
using IdMap = base::flat_map<std::string_view, ui::ColorId>;

// Name -> id for every ui/, components/ and chrome/ color, built from the
// same X-macro lists Chromium uses for ColorIdName().
#define COLOR_ID_MAP_ENTRY(enum_name, string_name) \
  std::pair<std::string_view, ui::ColorId>(string_name, enum_name)

std::vector<std::pair<std::string_view, ui::ColorId>> UiIds() {
  using namespace ui;  // NOLINT(build/namespaces)
#include "ui/color/color_id_map_macros.inc"  // NOLINT(build/include)
  return {COLOR_IDS};
#include "ui/color/color_id_map_macros.inc"  // NOLINT(build/include)
}

#define COLOR_ID_MAP_ENTRY(enum_name, string_name) \
  std::pair<std::string_view, ui::ColorId>(string_name, enum_name)
std::vector<std::pair<std::string_view, ui::ColorId>> ComponentsIds() {
  using namespace color;  // NOLINT(build/namespaces)
#include "ui/color/color_id_map_macros.inc"  // NOLINT(build/include)
  return {COMPONENTS_COLOR_IDS};
#include "ui/color/color_id_map_macros.inc"  // NOLINT(build/include)
}

#define COLOR_ID_MAP_ENTRY(enum_name, string_name) \
  std::pair<std::string_view, ui::ColorId>(string_name, enum_name)
std::vector<std::pair<std::string_view, ui::ColorId>> ChromeIds() {
#include "ui/color/color_id_map_macros.inc"  // NOLINT(build/include)
  return {CHROME_COLOR_IDS};
#include "ui/color/color_id_map_macros.inc"  // NOLINT(build/include)
}

const IdMap& AllIds() {
  static const base::NoDestructor<IdMap> ids([] {
    std::vector<std::pair<std::string_view, ui::ColorId>> all = UiIds();
    for (auto& v : {ComponentsIds(), ChromeIds()}) {
      all.insert(all.end(), v.begin(), v.end());
    }
    return IdMap(std::move(all));
  }());
  return *ids;
}

std::optional<SkColor> ParseHex(std::string_view v) {
  if (!v.starts_with('#')) {
    return std::nullopt;
  }
  std::string hex(v.substr(1));
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

// ---- expression parsing ---------------------------------------------------

class ExprParser {
 public:
  explicit ExprParser(std::string_view src) : src_(src) {}

  std::optional<Expr> ParseAll(std::string* error) {
    std::optional<Expr> e = ParseOne();
    SkipSpace();
    if (e && pos_ != src_.size()) {
      error_ = base::StrCat({"trailing input in ", src_});
      e.reset();
    }
    if (!e && error) {
      *error = error_;
    }
    return e;
  }

 private:
  void SkipSpace() {
    while (pos_ < src_.size() && (src_[pos_] == ' ' || src_[pos_] == '\t')) {
      ++pos_;
    }
  }
  bool Peek(char c) {
    SkipSpace();
    return pos_ < src_.size() && src_[pos_] == c;
  }
  static bool IsIdent(char c, bool first) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
           (!first && c >= '0' && c <= '9');
  }

  std::optional<Expr> ParseOne() {
    SkipSpace();
    if (pos_ >= src_.size()) {
      error_ = "unexpected end of expression";
      return std::nullopt;
    }
    Expr e;
    const char c = src_[pos_];
    if (c == '#') {
      size_t end = pos_ + 1;
      while (end < src_.size() && base::IsHexDigit(src_[end])) {
        ++end;
      }
      auto color = ParseHex(src_.substr(pos_, end - pos_));
      if (!color) {
        error_ = base::StrCat({"bad color ", src_.substr(pos_, end - pos_)});
        return std::nullopt;
      }
      pos_ = end;
      e.kind = Expr::Kind::kColor;
      e.color = *color;
      return e;
    }
    if ((c >= '0' && c <= '9') || c == '.') {
      size_t end = pos_;
      while (end < src_.size() &&
             ((src_[end] >= '0' && src_[end] <= '9') || src_[end] == '.')) {
        ++end;
      }
      double n = 0;
      if (!base::StringToDouble(src_.substr(pos_, end - pos_), &n)) {
        error_ = base::StrCat({"bad number in ", src_});
        return std::nullopt;
      }
      pos_ = end;
      e.kind = Expr::Kind::kNumber;
      e.number = static_cast<float>(n);
      return e;
    }
    if (!IsIdent(c, true)) {
      error_ = base::StrCat({"bad expression near \"", src_.substr(pos_), "\""});
      return std::nullopt;
    }
    size_t end = pos_;
    while (end < src_.size() && IsIdent(src_[end], end == pos_)) {
      ++end;
    }
    e.name = std::string(src_.substr(pos_, end - pos_));
    pos_ = end;
    if (!Peek('(')) {
      e.kind = Expr::Kind::kName;
      return e;
    }
    ++pos_;
    e.kind = Expr::Kind::kCall;
    if (!Peek(')')) {
      for (;;) {
        std::optional<Expr> arg = ParseOne();
        if (!arg) {
          return std::nullopt;
        }
        e.args.push_back(std::move(*arg));
        if (Peek(',')) {
          ++pos_;
          continue;
        }
        break;
      }
    }
    if (!Peek(')')) {
      error_ = base::StrCat({"missing ) in ", src_});
      return std::nullopt;
    }
    ++pos_;
    return e;
  }

  std::string_view src_;
  size_t pos_ = 0;
  std::string error_;
};

// ---- evaluation (mirror of preview/colormap.js evalExpr) -------------------

size_t Arity(std::string_view fn) {
  if (fn == "mix") return 3;
  if (fn == "alpha") return 2;
  if (fn == "contrast") return 1;
  if (fn == "dark") return 2;
  return SIZE_MAX;
}

std::optional<SkColor> PaletteRole(const OmarchyPalette& p,
                                   std::string_view n) {
  static constexpr std::pair<std::string_view, SkColor OmarchyPalette::*>
      kRoles[] = {
          {"accent", &OmarchyPalette::accent},
          {"selection", &OmarchyPalette::selection},
          {"muted", &OmarchyPalette::muted},
          {"background", &OmarchyPalette::background},
          {"dark_background", &OmarchyPalette::dark_background},
          {"darker_background", &OmarchyPalette::darker_background},
          {"lighter_background", &OmarchyPalette::lighter_background},
          {"foreground", &OmarchyPalette::foreground},
          {"dark_foreground", &OmarchyPalette::dark_foreground},
          {"light_foreground", &OmarchyPalette::light_foreground},
          {"red", &OmarchyPalette::red},
          {"yellow", &OmarchyPalette::yellow},
          {"green", &OmarchyPalette::green},
          {"cyan", &OmarchyPalette::cyan},
          {"blue", &OmarchyPalette::blue},
          {"magenta", &OmarchyPalette::magenta},
      };
  for (const auto& [name, member] : kRoles) {
    if (name == n) {
      return p.*member;
    }
  }
  if (n == "black") return SK_ColorBLACK;
  if (n == "white") return SK_ColorWHITE;
  return std::nullopt;
}

class Evaluator {
 public:
  Evaluator(const OmarchyPalette& palette,
            const std::map<std::string, Expr, std::less<>>& vars)
      : p_(palette), vars_(vars) {}

  std::optional<SkColor> Color(const Expr& e, std::string* error) {
    switch (e.kind) {
      case Expr::Kind::kColor:
        return e.color;
      case Expr::Kind::kNumber:
        *error = "expected a color, got a number";
        return std::nullopt;
      case Expr::Kind::kName:
        return Name(e.name, error);
      case Expr::Kind::kCall:
        return Call(e, error);
    }
  }

 private:
  // Vars resolve lazily, so their order in the file doesn't matter.
  std::optional<SkColor> Name(const std::string& n, std::string* error) {
    if (auto it = var_values_.find(n); it != var_values_.end()) {
      return it->second;
    }
    if (auto it = vars_.find(n); it != vars_.end()) {
      if (!resolving_.insert(n).second) {
        *error = base::StrCat({"var cycle through ", n});
        return std::nullopt;
      }
      std::optional<SkColor> v = Color(it->second, error);
      resolving_.erase(n);
      if (v) {
        var_values_[n] = *v;
      }
      return v;
    }
    if (auto role = PaletteRole(p_, n)) {
      return role;
    }
    *error = base::StrCat({"unknown name ", n});
    return std::nullopt;
  }

  std::optional<float> Number(const Expr& e, std::string* error) {
    if (e.kind != Expr::Kind::kNumber || e.number < 0 || e.number > 1) {
      *error = "expected a number in 0..1";
      return std::nullopt;
    }
    return e.number;
  }

  std::optional<SkColor> Call(const Expr& e, std::string* error) {
    if (Arity(e.name) != e.args.size()) {
      *error = base::StrCat({e.name, "() has the wrong number of arguments"});
      return std::nullopt;
    }
    if (e.name == "mix") {
      auto a = Color(e.args[0], error);
      auto b = a ? Color(e.args[1], error) : std::nullopt;
      auto w = b ? Number(e.args[2], error) : std::nullopt;
      if (!w) return std::nullopt;
      return color_utils::AlphaBlend(*a, *b, *w);
    }
    if (e.name == "alpha") {
      auto c = Color(e.args[0], error);
      auto a = c ? Number(e.args[1], error) : std::nullopt;
      if (!a) return std::nullopt;
      return SkColorSetA(*c, static_cast<U8CPU>(std::lround(*a * 255)));
    }
    if (e.name == "contrast") {
      auto c = Color(e.args[0], error);
      if (!c) return std::nullopt;
      return p_.ContrastingText(*c);
    }
    if (e.name == "dark") {
      return Color(e.args[p_.dark ? 0 : 1], error);
    }
    *error = base::StrCat({"unknown function ", e.name});
    return std::nullopt;
  }

  const OmarchyPalette& p_;
  const std::map<std::string, Expr, std::less<>>& vars_;
  std::map<std::string, SkColor, std::less<>> var_values_;
  std::set<std::string, std::less<>> resolving_;
};

}  // namespace

int OmarchyColorMap::Shape::Scale(int radius, int target) const {
  if (radius <= 0) {
    return radius;
  }
  return static_cast<int>(std::lround(static_cast<double>(radius) * target /
                                      stock_address_bar_radius));
}

OmarchyColorMap::OmarchyColorMap() = default;
OmarchyColorMap::~OmarchyColorMap() = default;

// static
std::optional<OmarchyColorMap::Expr> OmarchyColorMap::ParseExpr(
    std::string_view src,
    std::string* error) {
  return ExprParser(src).ParseAll(error);
}

// static
std::unique_ptr<OmarchyColorMap> OmarchyColorMap::Parse(
    std::string_view json,
    std::vector<std::string>* errors) {
  std::optional<base::DictValue> root = base::JSONReader::ReadDict(
      json, base::JSON_PARSE_CHROMIUM_EXTENSIONS | base::JSON_ALLOW_TRAILING_COMMAS);
  if (!root) {
    errors->push_back("color map is not a valid JSON object");
    return nullptr;
  }
  auto map = std::make_unique<OmarchyColorMap>();
  map->source_ = std::string(json);

  auto read_section = [&](std::string_view section, auto&& add) {
    const base::DictValue* dict = root->FindDict(section);
    if (!dict) {
      return;
    }
    for (const auto [key, value] : *dict) {
      const std::string* src = value.GetIfString();
      std::string error;
      std::optional<Expr> expr =
          src ? ParseExpr(*src, &error) : std::nullopt;
      if (!src) {
        error = "value must be a string";
      }
      if (!expr) {
        errors->push_back(base::StrCat({section, ".", key, ": ", error}));
        continue;
      }
      add(key, std::move(*expr));
    }
  };

  if (const base::DictValue* shape = root->FindDict("shape")) {
    const base::Value* target = shape->Find("address_bar_radius");
    if (target && target->is_int() && target->GetInt() >= 0) {
      map->shape_.mode = Shape::Mode::kFixed;
      map->shape_.address_bar_radius = target->GetInt();
    } else if (target && target->is_string() && target->GetString() == "stock") {
      map->shape_.mode = Shape::Mode::kStock;
    } else if (target &&
               !(target->is_string() && target->GetString() == "hyprland")) {
      errors->push_back(
          "shape.address_bar_radius: expected \"hyprland\", \"stock\" or a "
          "number of px");
    }
    std::optional<int> stock = shape->FindInt("stock_address_bar_radius");
    if (stock && *stock > 0) {
      map->shape_.stock_address_bar_radius = *stock;
    }
  }

  if (const base::DictValue* font = root->FindDict("font")) {
    if (const std::string* ui = font->FindString("ui")) {
      if (*ui == "stock") {
        map->font_.mode = Font::Mode::kStock;
      } else if (*ui != "omarchy" && !ui->empty()) {
        map->font_ = {Font::Mode::kFamily, *ui};
      }
    }
  }

  read_section("vars", [&](const std::string& key, Expr expr) {
    map->vars_.insert_or_assign(key, std::move(expr));
  });
  const IdMap& ids = AllIds();
  auto add_entry = [&](std::vector<Entry>& into, std::string_view section) {
    return [&, section](const std::string& key, Expr expr) {
      auto it = ids.find(key);
      if (it == ids.end()) {
        errors->push_back(base::StrCat({section, ".", key, ": unknown color id"}));
        return;
      }
      into.push_back({key, it->second, std::move(expr)});
    };
  };
  read_section("sys", add_entry(map->sys_, "sys"));
  read_section("chrome", add_entry(map->chrome_, "chrome"));
  return map;
}

// static
std::unique_ptr<OmarchyColorMap> OmarchyColorMap::CreateDefault() {
  std::vector<std::string> errors;
  auto map = Parse(kOmarchyColorMapDefault, &errors);
  CHECK(map) << "built-in color map failed to parse";
  return map;
}

OmarchyColorMap::Resolved OmarchyColorMap::Evaluate(
    const OmarchyPalette& palette,
    std::vector<std::string>* errors) const {
  Evaluator eval(palette, vars_);
  Resolved out;
  auto run = [&](const std::vector<Entry>& entries, auto& into) {
    for (const Entry& entry : entries) {
      std::string error;
      if (std::optional<SkColor> c = eval.Color(entry.expr, &error)) {
        into.push_back({entry.id, entry.name, *c});
      } else if (errors) {
        errors->push_back(base::StrCat({entry.name, ": ", error}));
      }
    }
  };
  run(sys_, out.sys);
  run(chrome_, out.chrome);
  return out;
}

}  // namespace omarchium
