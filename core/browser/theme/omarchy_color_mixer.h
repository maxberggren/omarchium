// Copyright 2026 The Omarchium Authors
// Use of this source code is governed by a BSD-style license.

#ifndef OMARCHIUM_BROWSER_THEME_OMARCHY_COLOR_MIXER_H_
#define OMARCHIUM_BROWSER_THEME_OMARCHY_COLOR_MIXER_H_

#include "omarchium/browser/theme/omarchy_color_map.h"

namespace ui {
class ColorProvider;
struct ColorProviderKey;
}  // namespace ui

namespace omarchium {

// Whether `palette` should restyle a provider for `key`. Not for forced
// colors / high contrast (accessibility wins), nor when the window asks for
// the opposite light/dark mode (the user picked one in chrome://settings).
bool ShouldApplyOmarchyPalette(const ui::ColorProviderKey& key, bool dark);

// Runs directly after ui::AddSysColorMixer(): reseeds the Material reference
// palette from the Omarchy accent, then sets every "sys" token from the color
// map, so ui/, components/ and chrome/ mixers all derive from Omarchy colors.
// `dark` is the mode these colors are for; `accent` seeds the reference
// palette.
void AddOmarchySysColorMixers(ui::ColorProvider* provider,
                              const ui::ColorProviderKey& key,
                              bool dark,
                              SkColor accent,
                              const OmarchyColorMap::Resolved& colors);

// Runs after all of Chrome's mixers: applies the map's "chrome" pins.
void AddOmarchyChromeColorMixer(ui::ColorProvider* provider,
                                const ui::ColorProviderKey& key,
                                bool dark,
                                const OmarchyColorMap::Resolved& colors);

}  // namespace omarchium

#endif  // OMARCHIUM_BROWSER_THEME_OMARCHY_COLOR_MIXER_H_
