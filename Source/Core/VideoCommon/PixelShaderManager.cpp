// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>
#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

bool PixelShaderManager::s_bFogRangeAdjustChanged;
bool PixelShaderManager::s_bViewPortChanged;
bool PixelShaderManager::s_bIndirectDirty;
bool PixelShaderManager::s_bTexProjDirty;

PixelShaderConstants PixelShaderManager::constants;
UberShaderConstants PixelShaderManager::more_constants;
bool PixelShaderManager::dirty;

void PixelShaderManager::Init()
{
  constants = {};

  // Init any intial constants which aren't zero when bpmem is zero.
  s_bFogRangeAdjustChanged = true;
  s_bViewPortChanged = false;

  SetIndMatrixChanged(0);
  SetIndMatrixChanged(1);
  SetIndMatrixChanged(2);
  SetZTextureTypeChanged();
  SetTexCoordChanged(0);
  SetTexCoordChanged(1);
  SetTexCoordChanged(2);
  SetTexCoordChanged(3);
  SetTexCoordChanged(4);
  SetTexCoordChanged(5);
  SetTexCoordChanged(6);
  SetTexCoordChanged(7);

  // fixed Konstants
  for (int component = 0; component < 4; component++)
  {
    more_constants.konst[0][component] = 255;  // 1
    more_constants.konst[1][component] = 223;  // 7/8
    more_constants.konst[2][component] = 191;  // 3/4
    more_constants.konst[3][component] = 159;  // 5/8
    more_constants.konst[4][component] = 128;  // 1/2
    more_constants.konst[5][component] = 96;   // 3/8
    more_constants.konst[6][component] = 64;   // 1/4
    more_constants.konst[7][component] = 32;   // 1/8

    // Invalid Konstants (reads as zero on hardware)
    more_constants.konst[8][component] = 0;
    more_constants.konst[9][component] = 0;
    more_constants.konst[10][component] = 0;
    more_constants.konst[11][component] = 0;

    // Annoyingly, alpha reads zero values for the .rgb colors (offically
    // defined as invalid)
    // If it wasn't for this, we could just use one of the first 3 colunms
    // instead of
    // wasting an entire 4th column just for alpha.
    if (component == 3)
    {
      more_constants.konst[12][component] = 0;
      more_constants.konst[13][component] = 0;
      more_constants.konst[14][component] = 0;
      more_constants.konst[15][component] = 0;
    }
  }

  dirty = true;
}

void PixelShaderManager::Dirty()
{
  // This function is called after a savestate is loaded.
  // Any constants that can changed based on settings should be re-calculated
  s_bFogRangeAdjustChanged = true;

  SetEfbScaleChanged(g_renderer->EFBToScaledXf(1), g_renderer->EFBToScaledYf(1));
  SetFogParamChanged();

  dirty = true;
}

void PixelShaderManager::SetConstants()
{
  if (s_bFogRangeAdjustChanged)
  {
    // set by two components, so keep changed flag here
    // TODO: try to split both registers and move this logic to the shader
    if (!g_ActiveConfig.bDisableFog && bpmem.fogRange.Base.Enabled == 1)
    {
      // bpmem.fogRange.Base.Center : center of the viewport in x axis. observation:
      // bpmem.fogRange.Base.Center = realcenter + 342;
      int center = ((u32)bpmem.fogRange.Base.Center) - 342;
      // normalize center to make calculations easy
      float ScreenSpaceCenter = center / (2.0f * xfmem.viewport.wd);
      ScreenSpaceCenter = (ScreenSpaceCenter * 2.0f) - 1.0f;
      // bpmem.fogRange.K seems to be  a table of precalculated coefficients for the adjust factor
      // observations: bpmem.fogRange.K[0].LO appears to be the lowest value and
      // bpmem.fogRange.K[4].HI the largest
      // they always seems to be larger than 256 so my theory is :
      // they are the coefficients from the center to the border of the screen
      // so to simplify I use the hi coefficient as K in the shader taking 256 as the scale
      // TODO: Shouldn't this be EFBToScaledXf?
      constants.fogf[0][0] = ScreenSpaceCenter;
      constants.fogf[0][1] =
          static_cast<float>(g_renderer->EFBToScaledX(static_cast<int>(2.0f * xfmem.viewport.wd)));
      constants.fogf[0][2] = bpmem.fogRange.K[4].HI / 256.0f;
    }
    else
    {
      constants.fogf[0][0] = 0;
      constants.fogf[0][1] = 1;
      constants.fogf[0][2] = 1;
    }
    dirty = true;

    s_bFogRangeAdjustChanged = false;
  }

  if (s_bViewPortChanged)
  {
    constants.zbias[1][0] = (s32)xfmem.viewport.farZ;
    constants.zbias[1][1] = (s32)xfmem.viewport.zRange;
    dirty = true;
    s_bViewPortChanged = false;
  }

  if (s_bIndirectDirty)
  {
    for (int i = 0; i < 4; i++)
      more_constants.iref[i] = 0;

    for (u32 i = 0; i < (bpmem.genMode.numtevstages + 1); ++i)
    {
      u32 stage = bpmem.tevind[i].bt;
      if (stage < bpmem.genMode.numindstages)
      {
        // We set some extra bits so the ubershader can quickly check if these
        // features are in use.
        if (bpmem.tevind[i].IsActive())
          more_constants.iref[stage] =
              bpmem.tevindref.getTexCoord(stage) | bpmem.tevindref.getTexMap(stage) << 8 | 1 << 16;
        more_constants.tevind[i][0] =
            bpmem.tevind[i].hex | 1 << 31;  // TODO: This match shadergen, but
                                            // videosw will always wrap.
      }
      else
      {
        more_constants.tevind[i][0] = 0;
      }
    }

    dirty = true;
    s_bIndirectDirty = false;
  }

  if (s_bTexProjDirty)
  {
    u32 projection = 0;
    for (int i = 0; i < 8; i++)
      projection |= xfmem.texMtxInfo[i].projection << i;
    more_constants.projection = projection;
  }
}

void PixelShaderManager::SetTevColor(int index, int component, s32 value)
{
  auto& c = constants.colors[index];
  c[component] = value;
  dirty = true;

  PRIM_LOG("tev color%d: %d %d %d %d", index, c[0], c[1], c[2], c[3]);
}

void PixelShaderManager::SetTevKonstColor(int index, int component, s32 value)
{
  auto& c = constants.kcolors[index];
  c[component] = value;
  dirty = true;

  // Konst for ubershaders. We build the whole array on cpu so the gpu can do a single indirect
  // access.
  if (component != 3)  // Alpha doesn't included in the .rgb konsts
    more_constants.konst[index + 12][component] = value;

  // .rrrr .gggg .bbbb .aaaa konsts
  more_constants.konst[index + 16 + component * 4][0] = value;
  more_constants.konst[index + 16 + component * 4][1] = value;
  more_constants.konst[index + 16 + component * 4][2] = value;
  more_constants.konst[index + 16 + component * 4][3] = value;

  PRIM_LOG("tev konst color%d: %d %d %d %d", index, c[0], c[1], c[2], c[3]);
}

void PixelShaderManager::SetAlpha()
{
  constants.alpha[0] = bpmem.alpha_test.ref0;
  constants.alpha[1] = bpmem.alpha_test.ref1;
  dirty = true;
}

void PixelShaderManager::SetDestAlpha()
{
  constants.alpha[3] = bpmem.dstalpha.alpha;
  dirty = true;
}

void PixelShaderManager::SetTexDims(int texmapid, u32 width, u32 height)
{
  float rwidth = 1.0f / (width * 128.0f);
  float rheight = 1.0f / (height * 128.0f);

  // TODO: move this check out to callee. There we could just call this function on texture changes
  // or better, use textureSize() in glsl
  if (constants.texdims[texmapid][0] != rwidth || constants.texdims[texmapid][1] != rheight)
    dirty = true;

  constants.texdims[texmapid][0] = rwidth;
  constants.texdims[texmapid][1] = rheight;
}

void PixelShaderManager::SetZTextureBias()
{
  constants.zbias[1][3] = bpmem.ztex1.bias;
  dirty = true;
}

void PixelShaderManager::SetViewportChanged()
{
  s_bViewPortChanged = true;
  s_bFogRangeAdjustChanged =
      true;  // TODO: Shouldn't be necessary with an accurate fog range adjust implementation
}

void PixelShaderManager::SetEfbScaleChanged(float scalex, float scaley)
{
  constants.efbscale[0] = 1.0f / scalex;
  constants.efbscale[1] = 1.0f / scaley;
  dirty = true;
}

void PixelShaderManager::SetZSlope(float dfdx, float dfdy, float f0)
{
  constants.zslope[0] = dfdx;
  constants.zslope[1] = dfdy;
  constants.zslope[2] = f0;
  dirty = true;
}

void PixelShaderManager::SetIndTexScaleChanged(bool high)
{
  constants.indtexscale[high][0] = bpmem.texscale[high].ss0;
  constants.indtexscale[high][1] = bpmem.texscale[high].ts0;
  constants.indtexscale[high][2] = bpmem.texscale[high].ss1;
  constants.indtexscale[high][3] = bpmem.texscale[high].ts1;
  dirty = true;
}

void PixelShaderManager::SetIndMatrixChanged(int matrixidx)
{
  int scale = ((u32)bpmem.indmtx[matrixidx].col0.s0 << 0) |
              ((u32)bpmem.indmtx[matrixidx].col1.s1 << 2) |
              ((u32)bpmem.indmtx[matrixidx].col2.s2 << 4);

  // xyz - static matrix
  // w - dynamic matrix scale / 128
  constants.indtexmtx[2 * matrixidx][0] = bpmem.indmtx[matrixidx].col0.ma;
  constants.indtexmtx[2 * matrixidx][1] = bpmem.indmtx[matrixidx].col1.mc;
  constants.indtexmtx[2 * matrixidx][2] = bpmem.indmtx[matrixidx].col2.me;
  constants.indtexmtx[2 * matrixidx][3] = 17 - scale;
  constants.indtexmtx[2 * matrixidx + 1][0] = bpmem.indmtx[matrixidx].col0.mb;
  constants.indtexmtx[2 * matrixidx + 1][1] = bpmem.indmtx[matrixidx].col1.md;
  constants.indtexmtx[2 * matrixidx + 1][2] = bpmem.indmtx[matrixidx].col2.mf;
  constants.indtexmtx[2 * matrixidx + 1][3] = 17 - scale;
  dirty = true;

  PRIM_LOG("indmtx%d: scale=%d, mat=(%d %d %d; %d %d %d)", matrixidx, scale,
           bpmem.indmtx[matrixidx].col0.ma, bpmem.indmtx[matrixidx].col1.mc,
           bpmem.indmtx[matrixidx].col2.me, bpmem.indmtx[matrixidx].col0.mb,
           bpmem.indmtx[matrixidx].col1.md, bpmem.indmtx[matrixidx].col2.mf);
}

void PixelShaderManager::SetZTextureTypeChanged()
{
  switch (bpmem.ztex2.type)
  {
  case TEV_ZTEX_TYPE_U8:
    constants.zbias[0][0] = 0;
    constants.zbias[0][1] = 0;
    constants.zbias[0][2] = 0;
    constants.zbias[0][3] = 1;
    break;
  case TEV_ZTEX_TYPE_U16:
    constants.zbias[0][0] = 1;
    constants.zbias[0][1] = 0;
    constants.zbias[0][2] = 0;
    constants.zbias[0][3] = 256;
    break;
  case TEV_ZTEX_TYPE_U24:
    constants.zbias[0][0] = 65536;
    constants.zbias[0][1] = 256;
    constants.zbias[0][2] = 1;
    constants.zbias[0][3] = 0;
    break;
  default:
    break;
  }
  dirty = true;
}

void PixelShaderManager::SetTexCoordChanged(u8 texmapid)
{
  TCoordInfo& tc = bpmem.texcoords[texmapid];
  constants.texdims[texmapid][2] = (float)(tc.s.scale_minus_1 + 1) * 128.0f;
  constants.texdims[texmapid][3] = (float)(tc.t.scale_minus_1 + 1) * 128.0f;
  dirty = true;
}

void PixelShaderManager::SetFogColorChanged()
{
  if (g_ActiveConfig.bDisableFog)
    return;

  constants.fogcolor[0] = bpmem.fog.color.r;
  constants.fogcolor[1] = bpmem.fog.color.g;
  constants.fogcolor[2] = bpmem.fog.color.b;
  dirty = true;
}

void PixelShaderManager::SetFogParamChanged()
{
  if (!g_ActiveConfig.bDisableFog)
  {
    constants.fogf[1][0] = bpmem.fog.a.GetA();
    constants.fogi[1] = bpmem.fog.b_magnitude;
    constants.fogf[1][2] = bpmem.fog.c_proj_fsel.GetC();
    constants.fogi[3] = bpmem.fog.b_shift;
  }
  else
  {
    constants.fogf[1][0] = 0.f;
    constants.fogi[1] = 1;
    constants.fogf[1][2] = 0.f;
    constants.fogi[3] = 1;
  }
  dirty = true;
}

void PixelShaderManager::SetFogRangeAdjustChanged()
{
  if (g_ActiveConfig.bDisableFog)
    return;

  s_bFogRangeAdjustChanged = true;
}

void PixelShaderManager::SetTexProjectionChanged()
{
  s_bTexProjDirty = true;
}

void PixelShaderManager::UpdateBP(u32 bp, u32 newValue)
{
  // TODO: think of a less totally hacky way of doing this.
  if (bp == 0)
  {
    more_constants.genmode = newValue;
    dirty = true;
  }
  else if (bp >= 0x28 && bp < 0x30)
  {
    u32 order = bp - 0x28;
    more_constants.tevorder[order][0] = newValue;
    dirty = true;
  }
  else if (bp == 0x42)
  {
    more_constants.dstalpha = newValue;
    dirty = true;
  }
  else if (bp == 0x43)
  {
    more_constants.zcontrol = newValue;
    dirty = true;
  }
  else if (bp >= 0xc0 && bp < 0xe0)
  {
    u32 comb = bp - 0xc0;
    more_constants.combiners[comb >> 1][comb & 1] = newValue;
    dirty = true;
  }
  else if (bp == 0xe8)
  {
    more_constants.fogRangeBase = newValue;
    dirty = true;
  }
  else if (bp == 0xf1)
  {
    more_constants.fogParam3 = newValue;
    dirty = true;
  }
  else if (bp == 0xf3)
  {
    more_constants.alphaTest = newValue;
    dirty = true;
  }
  else if (bp == 0xf5)
  {
    more_constants.ztex2 = newValue;
    dirty = true;
  }
  else if (bp >= 0xf6 && bp < 0xfe)
  {
    u32 ksel = bp - 0xf6;
    more_constants.tevksel[ksel][0] = newValue;
    dirty = true;
  }
  else if (bp == BPMEM_IREF || (bp & 0xf0) == BPMEM_IND_CMD)
  {
    more_constants.tevind[bp & 0xf][0] = newValue;
    dirty = true;
    s_bIndirectDirty = true;
  }
}

void PixelShaderManager::DoState(PointerWrap& p)
{
  p.Do(s_bFogRangeAdjustChanged);
  p.Do(s_bViewPortChanged);

  p.Do(constants);

  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    // Fixup the current state from global GPU state
    // NOTE: This requires that all GPU memory has been loaded already.
    Dirty();
  }
}
