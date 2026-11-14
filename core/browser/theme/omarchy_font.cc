// Copyright 2026 The Omarchium Authors
// Use of this source code is governed by a BSD-style license.

#include "omarchium/browser/theme/omarchy_font.h"

#include <fontconfig/fontconfig.h>

namespace omarchium {

std::optional<std::string> ResolveOmarchyFontFamily() {
  FcConfig* config = FcInitLoadConfigAndFonts();
  if (!config) {
    return std::nullopt;
  }
  std::optional<std::string> family;
  FcPattern* pattern =
      FcNameParse(reinterpret_cast<const FcChar8*>("monospace"));
  if (pattern && FcConfigSubstitute(config, pattern, FcMatchPattern)) {
    FcDefaultSubstitute(pattern);
    FcResult result;
    if (FcPattern* match = FcFontMatch(config, pattern, &result)) {
      FcChar8* name = nullptr;
      if (FcPatternGetString(match, FC_FAMILY, 0, &name) == FcResultMatch &&
          name && *name) {
        family = reinterpret_cast<const char*>(name);
      }
      FcPatternDestroy(match);
    }
  }
  if (pattern) {
    FcPatternDestroy(pattern);
  }
  FcConfigDestroy(config);
  return family;
}

}  // namespace omarchium
