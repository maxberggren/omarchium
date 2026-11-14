// Copyright 2026 The Omarchium Authors
// Use of this source code is governed by a BSD-style license.

#ifndef OMARCHIUM_BROWSER_THEME_HYPRLAND_IPC_H_
#define OMARCHIUM_BROWSER_THEME_HYPRLAND_IPC_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"

namespace omarchium::hyprland {

// Hyprland's live `decoration:rounding` (logical px), via its request socket.
// Blocking; returns nullopt when not running under Hyprland.
std::optional<int> QueryRounding();

// Listens on Hyprland's event socket and runs `on_config_reloaded` (on the
// creating sequence, which must support FileDescriptorWatcher) whenever
// Hyprland reloads its config, e.g. on an Omarchy theme switch.
class ConfigReloadListener {
 public:
  explicit ConfigReloadListener(base::RepeatingClosure on_config_reloaded);
  ConfigReloadListener(const ConfigReloadListener&) = delete;
  ConfigReloadListener& operator=(const ConfigReloadListener&) = delete;
  ~ConfigReloadListener();

 private:
  void OnReadable();

  base::RepeatingClosure on_config_reloaded_;
  base::ScopedFD fd_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> controller_;
  std::string pending_;
};

}  // namespace omarchium::hyprland

#endif  // OMARCHIUM_BROWSER_THEME_HYPRLAND_IPC_H_
