// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/GPFifo.h"

#include <cstddef>
#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/JitInterface.h"
#include "VideoCommon/CommandProcessor.h"

namespace GPFifo
{
// 32 Byte gather pipe with extra space
// Overfilling is no problem (up to the real limit), CheckGatherPipe will blast the
// contents in nicely sized chunks
//
// Other optimizations to think about:
// - If the GP is NOT linked to the FIFO, just blast to memory byte by word
// - If the GP IS linked to the FIFO, use a fast wrapping buffer and skip writing to memory
//
// Both of these should actually work! Only problem is that we have to decide at run time,
// the same function could use both methods. Compile 2 different versions of each such block?

// More room for the fastmodes
alignas(32) u8 s_gather_pipe[GATHER_PIPE_SIZE * 16];

// pipe pointer
u8* g_gather_pipe_ptr = s_gather_pipe;

static void SetGatherPipeCount(size_t size)
{
  g_gather_pipe_ptr = s_gather_pipe + size;
}

void DoState(PointerWrap& p)
{
  p.Do(s_gather_pipe);
  u32 pipe_count = static_cast<u32>(GetGatherPipeCount());
  p.Do(pipe_count);
  SetGatherPipeCount(pipe_count);
}

void Init()
{
  ResetGatherPipe();
  memset(s_gather_pipe, 0, sizeof(s_gather_pipe));
}

void ResetGatherPipe()
{
  SetGatherPipeCount(0);
}

void UpdateGatherPipe()
{
  size_t pipe_count = GetGatherPipeCount();
  size_t processed;
  u8* cur_mem = Memory::GetPointer(ProcessorInterface::Fifo_CPUWritePointer);
  for (processed = 0; pipe_count >= GATHER_PIPE_SIZE; processed += GATHER_PIPE_SIZE)
  {
    // copy the GatherPipe
    memcpy(cur_mem, s_gather_pipe + processed, GATHER_PIPE_SIZE);
    pipe_count -= GATHER_PIPE_SIZE;

    // increase the CPUWritePointer
    if (ProcessorInterface::Fifo_CPUWritePointer == ProcessorInterface::Fifo_CPUEnd)
    {
      ProcessorInterface::Fifo_CPUWritePointer = ProcessorInterface::Fifo_CPUBase;
      cur_mem = Memory::GetPointer(ProcessorInterface::Fifo_CPUWritePointer);
    }
    else
    {
      cur_mem += GATHER_PIPE_SIZE;
      ProcessorInterface::Fifo_CPUWritePointer += GATHER_PIPE_SIZE;
    }

    CommandProcessor::GatherPipeBursted();
  }

  // move back the spill bytes
  memmove(s_gather_pipe, s_gather_pipe + processed, pipe_count);
  SetGatherPipeCount(pipe_count);
}

}  // end of namespace GPFifo
