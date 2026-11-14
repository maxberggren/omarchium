// Copyright 2026 The Omarchium Authors
// Use of this source code is governed by a BSD-style license.

#ifndef OMARCHIUM_BROWSER_THEME_OMARCHY_FONT_H_
#define OMARCHIUM_BROWSER_THEME_OMARCHY_FONT_H_

#include <optional>
#include <string>

namespace omarchium {

// The family fontconfig resolves "monospace" to: Omarchy's system font
// (`omarchy font set` writes it into ~/.config/fontconfig/fonts.conf).
//
// Uses a freshly loaded fontconfig configuration every call, so it sees
// changes the process-wide one (cached at startup) would miss. Blocking.
std::optional<std::string> ResolveOmarchyFontFamily();

}  // namespace omarchium

#endif  // OMARCHIUM_BROWSER_THEME_OMARCHY_FONT_H_
