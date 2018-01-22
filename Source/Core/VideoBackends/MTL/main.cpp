#include "mtlpp.hpp"
#include "VideoBackends/MTL/VideoBackend.h"

namespace MTL
{

#warning ayy
bool VideoBackend::Initialize(void* window_handle){ return false;}
void VideoBackend::Shutdown(){}

void VideoBackend::Video_Prepare(){}
void VideoBackend::Video_Cleanup(){}
void VideoBackend::InitBackendInfo(){}

}
