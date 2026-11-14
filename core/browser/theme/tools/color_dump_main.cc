// Copyright 2026 The Omarchium Authors
// Use of this source code is governed by a BSD-style license.
//
//   omarchium_color_dump <colors.toml> [<color-map.json>]
//
// Prints what a theme resolves to (JSON, "#rrggbb[aa]" values), using
// exactly the code the browser runs. scripts/check-preview compares this
// with the HTML preview's evaluator.

#include <cstdio>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "omarchium/browser/theme/omarchy_color_map.h"
#include "omarchium/browser/theme/omarchy_palette.h"

namespace {

std::string Hex(SkColor c) {
  std::string s = base::StringPrintf("#%02x%02x%02x", SkColorGetR(c),
                                     SkColorGetG(c), SkColorGetB(c));
  if (SkColorGetA(c) != 0xFF) {
    s += base::StringPrintf("%02x", SkColorGetA(c));
  }
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  const auto args = base::CommandLine::ForCurrentProcess()->GetArgs();
  if (args.empty()) {
    fprintf(stderr, "usage: omarchium_color_dump <colors.toml> [<map.json>]\n");
    return 2;
  }
  std::string toml;
  if (!base::ReadFileToString(base::FilePath(args[0]), &toml)) {
    fprintf(stderr, "cannot read %s\n", args[0].c_str());
    return 1;
  }
  std::vector<std::string> errors;
  std::unique_ptr<omarchium::OmarchyColorMap> map;
  if (args.size() > 1) {
    std::string json;
    if (base::ReadFileToString(base::FilePath(args[1]), &json)) {
      map = omarchium::OmarchyColorMap::Parse(json, &errors);
    }
  }
  if (!map) {
    map = omarchium::OmarchyColorMap::CreateDefault();
  }
  auto palette = omarchium::OmarchyPalette::Parse(toml);
  if (!palette) {
    fprintf(stderr, "no background/foreground in %s\n", args[0].c_str());
    return 1;
  }
  auto resolved = map->Evaluate(*palette, &errors);

  base::DictValue out;
  out.Set("dark", palette->dark);
  base::DictValue sys, chrome;
  for (const auto& c : resolved.sys) sys.Set(c.name, Hex(c.color));
  for (const auto& c : resolved.chrome) chrome.Set(c.name, Hex(c.color));
  out.Set("sys", std::move(sys));
  out.Set("chrome", std::move(chrome));
  base::DictValue shape;
  for (int r : {4, 8, 10, 12, 16, 17, 24}) {
    shape.Set(base::NumberToString(r), map->shape().Scale(r, 6));
  }
  out.Set("scale_at_6", std::move(shape));
  base::ListValue errs;
  for (auto& e : errors) errs.Append(e);
  out.Set("errors", std::move(errs));
  printf("%s\n", base::WriteJson(out).value_or("{}").c_str());
  return 0;
}
