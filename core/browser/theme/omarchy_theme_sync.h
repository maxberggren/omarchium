// Copyright 2026 The Omarchium Authors
// Use of this source code is governed by a BSD-style license.

#ifndef OMARCHIUM_BROWSER_THEME_OMARCHY_THEME_SYNC_H_
#define OMARCHIUM_BROWSER_THEME_OMARCHY_THEME_SYNC_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "omarchium/browser/theme/omarchy_color_map.h"
#include "omarchium/browser/theme/omarchy_palette.h"

namespace ui {
class ColorProvider;
struct ColorProviderKey;
}  // namespace ui

namespace omarchium {

// Keeps the browser's colors (and corner radii) in sync with the active
// Omarchy theme and Hyprland.
//
// Reads ~/.local/state/omarchy/current/theme/colors.toml (what
// `omarchy-theme-set` writes) and the color map (built in, or
// ~/.config/omarchium/color-map.json), feeds them into the color pipeline,
// and watches both: switching Omarchy themes, or editing the map, restyles
// every window and web page live. Corner radii follow Hyprland's
// decoration:rounding (re-read whenever Hyprland reloads its config), and
// the UI font follows the Omarchy font (`omarchy font set`).
//
// Switches:
//   --disable-omarchy-theme          stock Chromium colors
//   --omarchy-colors=<colors.toml>   follow a different file
//   --omarchium-color-map=<json>     use a different color map
class OmarchyThemeSync {
 public:
  // Call once on the UI thread, after the toolkit is initialized and before
  // any browser window exists (ChromeBrowserMainParts::ToolkitInitialized).
  static void Start();

  OmarchyThemeSync(const OmarchyThemeSync&) = delete;
  OmarchyThemeSync& operator=(const OmarchyThemeSync&) = delete;

 private:
  class Watcher;
  struct Files {
    std::optional<std::string> colors;
    std::optional<std::string> user_map;
    std::optional<int> hyprland_rounding;
    std::optional<std::string> omarchy_font;
    bool operator==(const Files&) const = default;
  };

  OmarchyThemeSync(base::FilePath colors_path,
                   base::FilePath user_map_path,
                   base::FilePath fontconfig_path);
  ~OmarchyThemeSync();

  static Files ReadFiles(const base::FilePath& colors_path,
                         const base::FilePath& user_map_path);

  void Apply(Files files);
  void AddSysMixers(ui::ColorProvider* provider,
                    const ui::ColorProviderKey& key) const;
  void AddChromeMixer(ui::ColorProvider* provider,
                      const ui::ColorProviderKey& key) const;
  int FilterCornerRadius(int radius) const;

  const base::FilePath colors_path_;
  const base::FilePath user_map_path_;
  const base::FilePath fontconfig_path_;

  Files last_files_;
  std::optional<OmarchyPalette> palette_;
  std::unique_ptr<OmarchyColorMap> map_;
  OmarchyColorMap::Resolved colors_;
  std::optional<int> address_bar_radius_;  // nullopt: stock radii
  std::optional<std::string> ui_font_;     // nullopt: toolkit font

  base::SequenceBound<Watcher> watcher_;
  base::WeakPtrFactory<OmarchyThemeSync> weak_factory_{this};
};

}  // namespace omarchium

#endif  // OMARCHIUM_BROWSER_THEME_OMARCHY_THEME_SYNC_H_
