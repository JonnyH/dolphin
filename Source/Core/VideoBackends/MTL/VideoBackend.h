// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/VideoBackendBase.h"

namespace MTL
{
class VideoBackend : public VideoBackendBase
{
public:
  bool Initialize(void* window_handle) override;
  void Shutdown() override;

  std::string GetName() const override { return "MTL"; }
  std::string GetDisplayName() const override { return "Metal (experimental)"; }
  void Video_Prepare() override;
  void Video_Cleanup() override;

  void InitBackendInfo() override;

  unsigned int PeekMessages() override { return 0; }
};
}
