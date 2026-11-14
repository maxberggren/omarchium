// Copyright 2026 The Omarchium Authors
// Use of this source code is governed by a BSD-style license.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "testing/gtest/include/gtest/gtest.h"

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
