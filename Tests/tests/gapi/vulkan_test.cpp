#include <Tempest/VulkanApi>
#include <Tempest/Except>
#include <Tempest/Device>
#include <Tempest/Fence>
#include <Tempest/Pixmap>
#include <Tempest/Log>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include "gapi_test_common.h"

using namespace testing;
using namespace Tempest;

TEST(VulkanApi,VulkanApi) {
#if !defined(__OSX__)
  GapiTestCommon::init<VulkanApi>();
#endif
  }

TEST(VulkanApi,Vbo) {
#if !defined(__OSX__)
  GapiTestCommon::vbo<VulkanApi>();
#endif
  }

TEST(VulkanApi,VboInit) {
#if !defined(__OSX__)
  GapiTestCommon::vboInit<VulkanApi>();
#endif
  }

TEST(VulkanApi,VboDyn) {
#if !defined(__OSX__)
  GapiTestCommon::vboDyn<VulkanApi>();
#endif
  }

TEST(VulkanApi,SsboCopy) {
#if !defined(__OSX__)
  GapiTestCommon::bufCopy<VulkanApi>();
#endif
  }

TEST(VulkanApi,Shader) {
#if !defined(__OSX__)
  GapiTestCommon::shader<VulkanApi>();
#endif
  }

TEST(VulkanApi,Pso) {
#if !defined(__OSX__)
  GapiTestCommon::pso<VulkanApi>();
#endif
  }

TEST(VulkanApi,Fbo) {
#if !defined(__OSX__)
  GapiTestCommon::fbo<VulkanApi>("VulkanApi_Fbo.png");
#endif
  }

TEST(VulkanApi,Draw) {
#if !defined(__OSX__)
  GapiTestCommon::draw<VulkanApi,TextureFormat::RGBA8>  ("VulkanApi_Draw_RGBA8.png");
  GapiTestCommon::draw<VulkanApi,TextureFormat::RG8>    ("VulkanApi_Draw_RG8.png");
  GapiTestCommon::draw<VulkanApi,TextureFormat::R8>     ("VulkanApi_Draw_R8.png");
  GapiTestCommon::draw<VulkanApi,TextureFormat::RGBA16> ("VulkanApi_Draw_RGBA16.png");
  GapiTestCommon::draw<VulkanApi,TextureFormat::RG16>   ("VulkanApi_Draw_RG16.png");
  GapiTestCommon::draw<VulkanApi,TextureFormat::R16>    ("VulkanApi_Draw_R16.png");
  GapiTestCommon::draw<VulkanApi,TextureFormat::RGBA32F>("VulkanApi_Draw_RGBA32F.png");
  GapiTestCommon::draw<VulkanApi,TextureFormat::RG32F>  ("VulkanApi_Draw_RG32F.png");
  GapiTestCommon::draw<VulkanApi,TextureFormat::R32F>   ("VulkanApi_Draw_R32F.png");
#endif
  }

TEST(VulkanApi,Compute) {
#if !defined(__OSX__)
  GapiTestCommon::ssboDispath<VulkanApi>();
#endif
  }

TEST(VulkanApi,ComputeImage) {
#if !defined(__OSX__)
  GapiTestCommon::imageCompute<VulkanApi>("VulkanApi_Compute.png");
#endif
  }

TEST(VulkanApi,MipMaps) {
#if !defined(__OSX__)
  GapiTestCommon::mipMaps<VulkanApi,TextureFormat::RGBA8>  ("VulkanApi_MipMaps_RGBA8.png");
  GapiTestCommon::mipMaps<VulkanApi,TextureFormat::RGBA16> ("VulkanApi_MipMaps_RGBA16.png");
  GapiTestCommon::mipMaps<VulkanApi,TextureFormat::RGBA32F>("VulkanApi_MipMaps_RGBA32.png");
#endif
  }

TEST(VulkanApi,TesselationBasic) {
#if !defined(__OSX__)
  GapiTestCommon::psoTess<VulkanApi>();
#endif
  }

TEST(VulkanApi,SsboWrite) {
#if !defined(__OSX__)
  GapiTestCommon::ssboWriteVs<VulkanApi>();
#endif
  }

TEST(VulkanApi,PushRemapping) {
#if !defined(__OSX__)
  GapiTestCommon::pushConstant<VulkanApi>();
#endif
  }
