// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Common/Swap.h"
#include "Core/PowerPC/JitInterface.h"

class PointerWrap;

namespace GPFifo
{
enum
{
  GATHER_PIPE_SIZE = 32
};

// pipe pointer for JIT access
extern u8* g_gather_pipe_ptr;
extern u8 s_gather_pipe[];

// Init
void Init();
void DoState(PointerWrap& p);

// ResetGatherPipe
void ResetGatherPipe();
void UpdateGatherPipe();
inline size_t GetGatherPipeCount()
{
  return g_gather_pipe_ptr - s_gather_pipe;
}
inline void FastCheckGatherPipe()
{
  if (GetGatherPipeCount() >= GATHER_PIPE_SIZE)
  {
    UpdateGatherPipe();
  }
}

inline void CheckGatherPipe()
{
  if (GetGatherPipeCount() >= GATHER_PIPE_SIZE)
  {
    UpdateGatherPipe();

    // Profile where slow FIFO writes are occurring.
    JitInterface::CompileExceptionCheck(JitInterface::ExceptionType::FIFOWrite);
  }
}

inline bool IsEmpty()
{
  return GetGatherPipeCount() == 0;
}

// These expect pre-byteswapped values
// Also there's an upper limit of about 512 per batch
// Most likely these should be inlined into JIT instead

inline void FastWrite8(const u8 value)
{
  *g_gather_pipe_ptr = value;
  g_gather_pipe_ptr += sizeof(u8);
}

inline void FastWriteN(const u8* ptr, u32 len)
{
  std::memcpy(g_gather_pipe_ptr, ptr, len);
  g_gather_pipe_ptr += len;
}

inline void FastWrite16(u16 value)
{
  value = Common::swap16(value);
  std::memcpy(g_gather_pipe_ptr, &value, sizeof(u16));
  g_gather_pipe_ptr += sizeof(u16);
}

inline void FastWrite32(u32 value)
{
  value = Common::swap32(value);
  std::memcpy(g_gather_pipe_ptr, &value, sizeof(u32));
  g_gather_pipe_ptr += sizeof(u32);
}

inline void FastWrite64(u64 value)
{
  value = Common::swap64(value);
  std::memcpy(g_gather_pipe_ptr, &value, sizeof(u64));
  g_gather_pipe_ptr += sizeof(u64);
}

// Write
inline void Write8(const u8 value)
{
  FastWrite8(value);
  CheckGatherPipe();
}

inline void Write16(const u16 value)
{
  FastWrite16(value);
  CheckGatherPipe();
}

inline void Write32(const u32 value)
{
  FastWrite32(value);
  CheckGatherPipe();
}

inline void Write64(const u64 value)
{
  FastWrite64(value);
  CheckGatherPipe();
}
}
