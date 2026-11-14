// Copyright 2026 The Omarchium Authors
// Use of this source code is governed by a BSD-style license.

#include "omarchium/browser/theme/omarchy_theme_sync_state.h"

namespace omarchium {

namespace {
bool g_active = false;
}  // namespace

bool IsOmarchyThemeSyncActive() {
  return g_active;
}

void SetOmarchyThemeSyncActive(bool active) {
  g_active = active;
}

}  // namespace omarchium
