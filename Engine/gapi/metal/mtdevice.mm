#include "mtdevice.h"

#include <Tempest/Log>

#include <Metal/MTLDevice.h>
#include <Metal/MTLCommandQueue.h>

using namespace Tempest;
using namespace Tempest::Detail;

MtDevice::autoDevice::autoDevice(const char* name) {
  if(name==nullptr) {
    impl = MTLCreateSystemDefaultDevice();
    } else {
    NSArray<id<MTLDevice>>* dev = MTLCopyAllDevices();
    for(size_t i=0; i<dev.count; ++i) {
      if(std::strcmp(name,dev[i].name.UTF8String)==0) {
        impl = dev[i];
        break;
        }
      }
    [dev release];
    }

  if(impl!=nil)
    queue = [impl newCommandQueue];
  }

MtDevice::autoDevice::~autoDevice() {
  if(queue!=nil)
    [queue release];
  if(impl!=nil)
    [impl release];
  }

MtDevice::MtDevice(const char* name) : dev(name), samplers(dev.impl) {
  impl  = dev.impl;
  queue = dev.queue;

  if(queue==nil)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);

  deductProps(prop,impl);
  }

MtDevice::~MtDevice() {
  }

void MtDevice::waitIdle() {
  // TODO: verify, if this correct at all
  id<MTLCommandBuffer> cmd = [queue commandBuffer];
  [cmd commit];
  [cmd waitUntilCompleted];
  }

void Tempest::Detail::MtDevice::handleError(NSError *err) {
  if(err==nil)
    return;
#if !defined(NDEBUG)
  const char* e = [[err domain] UTF8String];
  Log::d("NSError: \"",e,"\"");
#endif
  throw DeviceLostException();
  }

void MtDevice::deductProps(AbstractGraphicsApi::Props& prop, id<MTLDevice> dev) {
  SInt32 majorVersion = 0, minorVersion = 0;
  if([[NSProcessInfo processInfo] respondsToSelector:@selector(operatingSystemVersion)]) {
    NSOperatingSystemVersion ver = [[NSProcessInfo processInfo] operatingSystemVersion];
    majorVersion = ver.majorVersion;
    minorVersion = ver.minorVersion;
    }

  std::strncpy(prop.name,dev.name.UTF8String,sizeof(prop.name));
  if(dev.hasUnifiedMemory)
    prop.type = AbstractGraphicsApi::DeviceType::Integrated; else
    prop.type = AbstractGraphicsApi::DeviceType::Discrete;

  // https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
  static const TextureFormat smp[] = {TextureFormat::R8,   TextureFormat::RG8,   TextureFormat::RGBA8,
                                      TextureFormat::R16,  TextureFormat::RG16,  TextureFormat::RGBA16,
                                      TextureFormat::R32F, TextureFormat::RG32F, TextureFormat::RGBA32F
                                     };

  static const TextureFormat att[] = {TextureFormat::R8,   TextureFormat::RG8,   TextureFormat::RGBA8,
                                      TextureFormat::R16,  TextureFormat::RG16,  TextureFormat::RGBA16,
                                      TextureFormat::R32F, TextureFormat::RG32F, TextureFormat::RGBA32F
                                     };

  static const TextureFormat sso[] = {TextureFormat::R8,   TextureFormat::RG8,   TextureFormat::RGBA8,
                                      TextureFormat::R16,  TextureFormat::RG16,  TextureFormat::RGBA16,
                                      TextureFormat::R32F, TextureFormat::RGBA32F
                                     };

  // TODO: expose MTLPixelFormatDepth32Float
  static const TextureFormat ds[]  = {TextureFormat::Depth16};

  uint64_t smpBit = 0, attBit = 0, dsBit = 0, storBit = 0;
  for(auto& i:smp)
    smpBit |= uint64_t(1) << uint64_t(i);
  for(auto& i:att)
    attBit |= uint64_t(1) << uint64_t(i);
  for(auto& i:sso)
    storBit  |= uint64_t(1) << uint64_t(i);
  for(auto& i:ds)
    dsBit  |= uint64_t(1) << uint64_t(i);

  if(dev.supportsBCTextureCompression) {
    static const TextureFormat bc[] = {TextureFormat::DXT1, TextureFormat::DXT3, TextureFormat::DXT5};
    for(auto& i:bc)
      smpBit |= uint64_t(1) << uint64_t(i);
    }

  if(dev.depth24Stencil8PixelFormatSupported) {
    static const TextureFormat ds[] = {TextureFormat::Depth24S8};
    for(auto& i:ds)
      dsBit  |= uint64_t(1) << uint64_t(i);
    }

  prop.setSamplerFormats(smpBit);
  prop.setAttachFormats (attBit);
  prop.setDepthFormats  (dsBit);
  prop.setStorageFormats(storBit);

  prop.mrt.maxColorAttachments = 4;
  if([dev supportsFamily:MTLGPUFamilyApple2])
    prop.mrt.maxColorAttachments = 8;

  // https://developer.apple.com/documentation/metal/mtlrendercommandencoder/1515829-setvertexbuffer?language=objc
  prop.vbo.maxAttribs = 31;

#ifdef __IOS__
  prop.ubo .offsetAlign = 16;
  prop.ssbo.offsetAlign = 16;
#else
  prop.ubo .offsetAlign = 256; //assuming device-local memory
  prop.ssbo.offsetAlign = 16;
#endif

  // in fact there is no limit, just recomendation to submit less than 4kb of data
  prop.push.maxRange = 256;

  prop.compute.maxGroupSize.x = 512;
  prop.compute.maxGroupSize.y = 512;
  prop.compute.maxGroupSize.z = 512;
  if([dev supportsFamily:MTLGPUFamilyApple4]) {
    prop.compute.maxGroupSize.x = 1024;
    prop.compute.maxGroupSize.y = 1024;
    prop.compute.maxGroupSize.z = 1024;
    }

  prop.anisotropy    = true;
  prop.maxAnisotropy = 16;

  // TODO: verify
  //prop.storeAndAtomicVs = true;
  //prop.storeAndAtomicFs = true;
  }

