// Copyright 2026 The Omarchium Authors
// Use of this source code is governed by a BSD-style license.

#include "omarchium/browser/theme/omarchy_color_mixer.h"

#include "omarchium/browser/theme/omarchy_palette.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/ref_color_mixer.h"

namespace omarchium {

bool ShouldApplyOmarchyPalette(const ui::ColorProviderKey& key,
                               const OmarchyPalette& palette) {
  if (key.forced_colors != ui::ColorProviderKey::ForcedColors::kNone ||
      key.contrast_mode == ui::ColorProviderKey::ContrastMode::kHigh) {
    return false;
  }
  const bool key_dark =
      key.color_mode == ui::ColorProviderKey::ColorMode::kDark;
  return key_dark == palette.dark;
}

void AddOmarchySysColorMixers(ui::ColorProvider* provider,
                              const ui::ColorProviderKey& key,
                              const OmarchyPalette& palette,
                              const OmarchyColorMap::Resolved& colors) {
  if (!ShouldApplyOmarchyPalette(key, palette)) {
    return;
  }
  // A few places read Material reference tones (kColorRef*) directly:
  // illustrations, tab-group colors. Seeding them from the accent keeps
  // those in the theme's hue instead of Chrome blue.
  ui::ColorProviderKey ref_key = key;
  ref_key.user_color = palette.accent;
  ref_key.user_color_source = ui::ColorProviderKey::UserColorSource::kAccent;
  ui::AddRefColorMixer(provider, ref_key);

  ui::ColorMixer& mixer = provider->AddMixer();
  for (const auto& c : colors.sys) {
    mixer[c.id] = {c.color};
  }
}

void AddOmarchyChromeColorMixer(ui::ColorProvider* provider,
                                const ui::ColorProviderKey& key,
                                const OmarchyPalette& palette,
                                const OmarchyColorMap::Resolved& colors) {
  if (colors.chrome.empty() || !ShouldApplyOmarchyPalette(key, palette)) {
    return;
  }
  ui::ColorMixer& mixer = provider->AddMixer();
  for (const auto& c : colors.chrome) {
    mixer[c.id] = {c.color};
  }
}

}  // namespace omarchium
