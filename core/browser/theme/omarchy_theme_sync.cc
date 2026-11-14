// Copyright 2026 The Omarchium Authors
// Use of this source code is governed by a BSD-style license.

#include "omarchium/browser/theme/omarchy_theme_sync.h"

#include <utility>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/nix/xdg_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/timer/timer.h"
#include "omarchium/browser/theme/hyprland_ipc.h"
#include "omarchium/browser/theme/omarchy_color_mixer.h"
#include "omarchium/browser/theme/omarchy_font.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_mixers.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/platform_font_skia.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/layout/layout_provider.h"

namespace omarchium {

namespace {

constexpr char kDisableSwitch[] = "disable-omarchy-theme";
constexpr char kColorsSwitch[] = "omarchy-colors";
constexpr char kColorMapSwitch[] = "omarchium-color-map";

// omarchy-theme-set replaces the theme directory with `rm -rf` + `mv`;
// coalesce the burst of events into one reload after it settles.
constexpr base::TimeDelta kSettleDelay = base::Milliseconds(150);
constexpr size_t kMaxFileSize = 1 << 20;

OmarchyThemeSync* g_instance = nullptr;

}  // namespace

// Lives on a blocking-capable sequence; reports file contents (and Hyprland's
// rounding) to the UI thread whenever a watched file changes or Hyprland
// reloads its config.
class OmarchyThemeSync::Watcher {
 public:
  using Callback = base::RepeatingCallback<void(Files)>;

  Watcher(base::FilePath colors_path,
          base::FilePath user_map_path,
          base::FilePath fontconfig_path,
          Callback on_change)
      : colors_path_(std::move(colors_path)),
        user_map_path_(std::move(user_map_path)),
        on_change_(std::move(on_change)) {
    auto changed = base::BindRepeating(&Watcher::OnPathChanged,
                                       base::Unretained(this));
    // Both watchers follow the path through directory replacement and
    // creation, so they work before Omarchy has written anything too.
    if (!colors_watcher_.Watch(colors_path_,
                               base::FilePathWatcher::Type::kNonRecursive,
                               changed)) {
      LOG(WARNING) << "omarchium: cannot watch " << colors_path_;
    }
    if (!map_watcher_.Watch(user_map_path_,
                            base::FilePathWatcher::Type::kNonRecursive,
                            changed)) {
      LOG(WARNING) << "omarchium: cannot watch " << user_map_path_;
    }
    if (!font_watcher_.Watch(fontconfig_path,
                             base::FilePathWatcher::Type::kNonRecursive,
                             changed)) {
      LOG(WARNING) << "omarchium: cannot watch " << fontconfig_path;
    }
    hyprland_listener_ = std::make_unique<hyprland::ConfigReloadListener>(
        base::BindRepeating(&Watcher::OnPathChanged, base::Unretained(this),
                            base::FilePath(), false));
  }

 private:
  void OnPathChanged(const base::FilePath&, bool error) {
    settle_timer_.Start(FROM_HERE, kSettleDelay,
                        base::BindOnce(&Watcher::Reload, base::Unretained(this)));
  }

  void Reload() { on_change_.Run(ReadFiles(colors_path_, user_map_path_)); }

  const base::FilePath colors_path_;
  const base::FilePath user_map_path_;
  Callback on_change_;
  base::FilePathWatcher colors_watcher_;
  base::FilePathWatcher map_watcher_;
  base::FilePathWatcher font_watcher_;
  base::OneShotTimer settle_timer_;
  std::unique_ptr<hyprland::ConfigReloadListener> hyprland_listener_;
};

// static
void OmarchyThemeSync::Start() {
  CHECK(!g_instance);
  const base::CommandLine& cmd = *base::CommandLine::ForCurrentProcess();
  if (cmd.HasSwitch(kDisableSwitch)) {
    return;
  }
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  base::FilePath colors =
      cmd.HasSwitch(kColorsSwitch)
          ? cmd.GetSwitchValuePath(kColorsSwitch)
          : base::nix::GetXDGDirectory(env.get(), "XDG_STATE_HOME",
                                       ".local/state")
                .Append("omarchy/current/theme/colors.toml");
  base::FilePath user_map =
      cmd.HasSwitch(kColorMapSwitch)
          ? cmd.GetSwitchValuePath(kColorMapSwitch)
          : base::nix::GetXDGDirectory(env.get(), "XDG_CONFIG_HOME", ".config")
                .Append("omarchium/color-map.json");

  base::FilePath fontconfig =
      base::nix::GetXDGDirectory(env.get(), "XDG_CONFIG_HOME", ".config")
          .Append("fontconfig/fonts.conf");

  // Intentionally leaked: lives as long as the browser process.
  g_instance = new OmarchyThemeSync(std::move(colors), std::move(user_map),
                                    std::move(fontconfig));
}

OmarchyThemeSync::OmarchyThemeSync(base::FilePath colors_path,
                                   base::FilePath user_map_path,
                                   base::FilePath fontconfig_path)
    : colors_path_(std::move(colors_path)),
      user_map_path_(std::move(user_map_path)),
      fontconfig_path_(std::move(fontconfig_path)) {
  // Read synchronously so the first window already has the right colors
  // instead of flashing Chromium's. Two tiny files; the UI thread may still
  // block here (BrowserMainLoop disallows it only once the main loop runs).
  Apply(ReadFiles(colors_path_, user_map_path_));

  ui::SetSysColorMixerHook(base::BindRepeating(
      [](ui::ColorProvider* provider, const ui::ColorProviderKey& key) {
        g_instance->AddSysMixers(provider, key);
      }));
  views::LayoutProvider::SetCornerRadiusFilter(
      base::BindRepeating([](int radius) {
        return g_instance->FilterCornerRadius(radius);
      }));
  // Appended after Chrome's own initializers, so these pins win.
  ui::ColorProviderManager::Get().AppendColorProviderInitializer(
      base::BindRepeating(
          [](ui::ColorProvider* provider, const ui::ColorProviderKey& key) {
            g_instance->AddChromeMixer(provider, key);
          }));

  watcher_ = base::SequenceBound<Watcher>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      colors_path_, user_map_path_, fontconfig_path_,
      base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindRepeating(&OmarchyThemeSync::Apply,
                              weak_factory_.GetWeakPtr())));
}

OmarchyThemeSync::~OmarchyThemeSync() = default;

// static
OmarchyThemeSync::Files OmarchyThemeSync::ReadFiles(
    const base::FilePath& colors_path,
    const base::FilePath& user_map_path) {
  Files files;
  std::string contents;
  if (base::ReadFileToStringWithMaxSize(colors_path, &contents, kMaxFileSize)) {
    files.colors = std::move(contents);
  }
  contents.clear();
  if (base::ReadFileToStringWithMaxSize(user_map_path, &contents,
                                        kMaxFileSize)) {
    files.user_map = std::move(contents);
  }
  files.hyprland_rounding = hyprland::QueryRounding();
  files.omarchy_font = ResolveOmarchyFontFamily();
  return files;
}

void OmarchyThemeSync::Apply(Files files) {
  if (files == last_files_ && map_) {
    return;
  }

  // Color map: the user's override if it parses, else the built-in one.
  std::vector<std::string> errors;
  std::unique_ptr<OmarchyColorMap> map;
  if (files.user_map) {
    map = OmarchyColorMap::Parse(*files.user_map, &errors);
    for (const std::string& e : errors) {
      LOG(WARNING) << "omarchium: " << user_map_path_ << ": " << e;
    }
  }
  if (!map) {
    map = OmarchyColorMap::CreateDefault();
  }

  // Palette. A missing or half-written file (mid theme switch) keeps the
  // previous palette rather than dropping back to stock colors.
  std::optional<OmarchyPalette> palette =
      files.colors ? OmarchyPalette::Parse(*files.colors) : std::nullopt;
  if (!palette) {
    if (files.colors) {
      LOG(WARNING) << "omarchium: " << colors_path_
                   << " has no background/foreground; keeping colors";
    }
    palette = palette_;
  }

  switch (map->shape().mode) {
    case OmarchyColorMap::Shape::Mode::kStock:
      address_bar_radius_ = std::nullopt;
      break;
    case OmarchyColorMap::Shape::Mode::kFixed:
      address_bar_radius_ = map->shape().address_bar_radius;
      break;
    case OmarchyColorMap::Shape::Mode::kHyprland:
      address_bar_radius_ = files.hyprland_rounding;  // nullopt off Hyprland
      break;
  }

  std::optional<std::string> ui_font;
  switch (map->font().mode) {
    case OmarchyColorMap::Font::Mode::kStock:
      break;
    case OmarchyColorMap::Font::Mode::kFamily:
      ui_font = map->font().family;
      break;
    case OmarchyColorMap::Font::Mode::kOmarchy:
      ui_font = files.omarchy_font;
      break;
  }
  if (ui_font != ui_font_) {
    ui_font_ = ui_font;
    gfx::PlatformFontSkia::SetDefaultFontFamilyOverride(ui_font_);
    gfx::PlatformFontSkia::ReloadDefaultFont();
    if (ui::ResourceBundle::HasSharedInstance()) {
      ui::ResourceBundle::GetSharedInstance().ReloadFonts();
    }
  }

  last_files_ = std::move(files);
  map_ = std::move(map);
  palette_ = std::move(palette);
  colors_ = {};
  if (palette_) {
    errors.clear();
    colors_ = map_->Evaluate(*palette_, &errors);
    for (const std::string& e : errors) {
      LOG(WARNING) << "omarchium: color map: " << e;
    }
  }

  // Light/dark follows the theme for the whole app, web content included
  // (prefers-color-scheme), unless the user picked one in settings.
  ui::NativeTheme::SetPreferredColorSchemeOverride(
      palette_ ? std::make_optional(
                     palette_->dark
                         ? ui::NativeTheme::PreferredColorScheme::kDark
                         : ui::NativeTheme::PreferredColorScheme::kLight)
               : std::nullopt);

  // New colors under the same keys: bump the version so every provider is
  // rebuilt, then tell every widget and web contents to repaint.
  ui::NativeTheme* native = ui::NativeTheme::GetInstanceForNativeUi();
  ui::NativeTheme* web = ui::NativeTheme::GetInstanceForWeb();
  native->IncrementSystemColorVersion();
  web->IncrementSystemColorVersion();
  ui::ColorProviderManager::Get().ResetColorProviderCache();
  native->NotifyOnNativeThemeUpdated();
  web->NotifyOnNativeThemeUpdated();
}

void OmarchyThemeSync::AddSysMixers(ui::ColorProvider* provider,
                                    const ui::ColorProviderKey& key) const {
  if (palette_) {
    AddOmarchySysColorMixers(provider, key, *palette_, colors_);
  }
}

int OmarchyThemeSync::FilterCornerRadius(int radius) const {
  return address_bar_radius_ ? map_->shape().Scale(radius, *address_bar_radius_)
                             : radius;
}

void OmarchyThemeSync::AddChromeMixer(ui::ColorProvider* provider,
                                      const ui::ColorProviderKey& key) const {
  if (palette_) {
    AddOmarchyChromeColorMixer(provider, key, *palette_, colors_);
  }
}

}  // namespace omarchium
