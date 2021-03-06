#include "mtcommandbuffer.h"

#include "mtbuffer.h"
#include "mtdevice.h"
#include "mtframebuffer.h"
#include "mtpipeline.h"
#include "mtrenderpass.h"
#include "mtpipelinelay.h"
#include "mtdescriptorarray.h"
#include "mttexture.h"

#include <Metal/MTLCommandQueue.h>

using namespace Tempest;
using namespace Tempest::Detail;

MtCommandBuffer::MtCommandBuffer(MtDevice& dev)
  : device(dev) {
  reset();
  }

MtCommandBuffer::~MtCommandBuffer() {
  [impl release];
  }

bool MtCommandBuffer::isRecording() const {
  // FIXME
  return encDraw!=nil && encComp!=nil && encBlit!=nullptr;
  }

void MtCommandBuffer::begin() {
  reset();
  }

void MtCommandBuffer::end() {
  setEncoder(E_None,nullptr,nullptr);
  }

void MtCommandBuffer::reset() {
  MTLCommandBufferDescriptor* desc = [MTLCommandBufferDescriptor new];
  desc.retainedReferences = NO;

  impl = [device.queue commandBufferWithDescriptor:desc];
  [impl retain];
  [desc release];
  }

void MtCommandBuffer::beginRenderPass(AbstractGraphicsApi::Fbo *f,
                                      AbstractGraphicsApi::Pass *p,
                                      uint32_t width, uint32_t height) {
  auto& fbo  = *reinterpret_cast<MtFramebuffer*>(f);
  auto& pass = *reinterpret_cast<MtRenderPass*> (p);

  setEncoder(E_Draw,&fbo,&pass);
  curFbo  = &fbo;

  // [enc setFrontFacingWinding:MTLWindingCounterClockwise];
  setViewport(Rect(0,0,width,height));
  }

void MtCommandBuffer::endRenderPass() {
  setEncoder(E_None,nullptr,nullptr);
  curFbo = nullptr;
  }

void MtCommandBuffer::setEncoder(MtCommandBuffer::EncType e, MtFramebuffer* fbo, MtRenderPass* pass) {
  if(encDraw!=nil && e!=E_Draw) {
    [encDraw endEncoding];
    [encDraw release];
    encDraw = nil;
    }
  if(encComp!=nil && e!=E_Comp) {
    [encComp endEncoding];
    [encComp release];
    encComp = nil;
    }
  if(encBlit!=nil && e!=E_Blit) {
    [encBlit endEncoding];
    [encBlit release];
    encBlit = nil;
    }

  switch(e) {
    case E_None: {
      break;
      }
    case E_Draw: {
      if(encDraw!=nil) {
        [encDraw endEncoding];
        [encDraw release];
        }
      MTLRenderPassDescriptor* desc = fbo->instance(*pass);
      encDraw = [impl renderCommandEncoderWithDescriptor:desc];
      [encDraw retain];
      break;
      }
    case E_Comp: {
      if(encComp!=nil)
        return;
      encComp = [impl computeCommandEncoder];
      [encComp retain];
      break;
      }
    case E_Blit: {
      if(encBlit!=nil)
        return;
      encBlit = [impl blitCommandEncoder];
      [encBlit retain];
      break;
      }
    }
  }

void MtCommandBuffer::changeLayout(AbstractGraphicsApi::Buffer&,
                                   BufferLayout /*prev*/, BufferLayout /*next*/) {
  }

void MtCommandBuffer::changeLayout(AbstractGraphicsApi::Attach &,
                                   TextureLayout /*prev*/, TextureLayout /*next*/, bool /*byRegion*/) {
  }

void MtCommandBuffer::setComputePipeline(AbstractGraphicsApi::CompPipeline &p) {
  setEncoder(E_Comp,nullptr,nullptr);
  auto& px = reinterpret_cast<MtCompPipeline&>(p);
  [encComp setComputePipelineState:px.impl];
  curLay   = px.lay.handler;
  maxTotalThreadsPerThreadgroup = px.impl.maxTotalThreadsPerThreadgroup;
  }

void MtCommandBuffer::setBytes(AbstractGraphicsApi::Pipeline&,
                               const void *data, size_t size) {
  implSetBytes(data,size);
  }

void MtCommandBuffer::setUniforms(AbstractGraphicsApi::Pipeline&,
                                  AbstractGraphicsApi::Desc &u) {
  implSetUniforms(u);
  }

void MtCommandBuffer::setBytes(AbstractGraphicsApi::CompPipeline&,
                               const void *data, size_t size) {
  implSetBytes(data,size);
  }

void MtCommandBuffer::setUniforms(AbstractGraphicsApi::CompPipeline&,
                                  AbstractGraphicsApi::Desc &u) {
  implSetUniforms(u);
  }

void MtCommandBuffer::setViewport(const Rect &r) {
  MTLViewport v = {};
  v.originX = r.x;
  v.originY = r.y;
  v.width   = r.w;
  v.height  = r.h;
  v.znear   = 0;
  v.zfar    = 1;

  [encDraw setViewport:v];
  }

void MtCommandBuffer::draw(const AbstractGraphicsApi::Buffer& ivbo, size_t offset, size_t vertexCount, size_t firstInstance, size_t instanceCount) {
  auto& vbo = reinterpret_cast<const MtBuffer&>(ivbo);
  [encDraw setVertexBuffer:vbo.impl
                    offset:0
                   atIndex:curVboId];
  [encDraw drawPrimitives:topology
                      vertexStart:offset
                      vertexCount:vertexCount
                      instanceCount:instanceCount
                      baseInstance:firstInstance];
  }

void MtCommandBuffer::drawIndexed(const AbstractGraphicsApi::Buffer& ivbo, const AbstractGraphicsApi::Buffer& iibo, Detail::IndexClass cls,
                                  size_t ioffset, size_t isize, size_t voffset, size_t firstInstance, size_t instanceCount) {
  static const MTLIndexType type[2] = {
    MTLIndexTypeUInt16,
    MTLIndexTypeUInt32,
  };
  auto&    vbo     = reinterpret_cast<const MtBuffer&>(ivbo);
  auto&    ibo     = reinterpret_cast<const MtBuffer&>(iibo);
  auto     iboType = type[uint32_t(cls)];
  uint32_t mul     = (iboType==MTLIndexTypeUInt16 ? 2 : 4);

  [encDraw setVertexBuffer:vbo.impl
                    offset:0
                   atIndex:curVboId];
  [encDraw drawIndexedPrimitives:topology
                             indexCount:isize
                             indexType:iboType
                             indexBuffer:ibo.impl
                             indexBufferOffset:ioffset*mul
                             instanceCount:instanceCount
                             baseVertex:voffset
                             baseInstance:firstInstance
                             ];
  }

void MtCommandBuffer::dispatch(size_t x, size_t y, size_t z) {
  MTLSize sz;
  sz.width  = x;
  sz.height = y;
  sz.depth  = z;

  MTLSize th;
  th.width  = 1;
  th.height = 1;
  th.depth  = 1;

  [encComp dispatchThreadgroups:sz threadsPerThreadgroup:th];
  }

void MtCommandBuffer::implSetBytes(const void* bytes, size_t sz) {
  auto& mtl = curLay->bindPush;
  auto& l   = curLay->pb;

  if(l.stage==0)
    return;
  if(mtl.bindVs!=uint32_t(-1)) {
    [encDraw setVertexBytes:bytes length:sz atIndex:mtl.bindVs];
    }
  if(mtl.bindFs!=uint32_t(-1)) {
    [encDraw setFragmentBytes:bytes length:sz atIndex:mtl.bindFs];
    }
  if(mtl.bindCs!=uint32_t(-1)) {
    [encComp setBytes:bytes length:sz atIndex:mtl.bindCs];
    }
  }

void MtCommandBuffer::implSetUniforms(AbstractGraphicsApi::Desc& u) {
  auto& d   = reinterpret_cast<MtDescriptorArray&>(u);
  auto& lay = curLay->lay;
  auto& mtl = curLay->bind;
  for(size_t i=0; i<lay.size(); ++i) {
    auto& l = lay[i];
    if(l.stage==0)
      continue;
    switch(l.cls) {
      case ShaderReflection::Push:
        break;
      case ShaderReflection::Ubo:
      case ShaderReflection::SsboR:
      case ShaderReflection::SsboRW:
        setBuffer(mtl[i],d.desc[i].val,d.desc[i].offset);
        break;
      case ShaderReflection::Texture:
      case ShaderReflection::ImgR:
      case ShaderReflection::ImgRW:
        setTexture(mtl[i],d.desc[i].val,d.desc[i].sampler);
        break;
      }
    }
  }

void MtCommandBuffer::setBuffer(const MtPipelineLay::MTLBind& mtl,
                                id<MTLBuffer> buf, size_t offset) {
  if(mtl.bindVs!=uint32_t(-1)) {
    [encDraw setVertexBuffer:buf offset:offset atIndex:mtl.bindVs];
    }
  if(mtl.bindFs!=uint32_t(-1)) {
    [encDraw setFragmentBuffer:buf offset:offset atIndex:mtl.bindFs];
    }
  if(mtl.bindCs!=uint32_t(-1)) {
    [encComp setBuffer:buf offset:offset atIndex:mtl.bindCs];
    }
  }

void MtCommandBuffer::setTexture(const MtPipelineLay::MTLBind& mtl,
                                 id<MTLTexture> tex, id<MTLSamplerState> ss) {
  if(mtl.bindVs!=uint32_t(-1))
    [encDraw setVertexTexture:tex atIndex:mtl.bindVs];
  if(mtl.bindVsSmp!=uint32_t(-1))
    [encDraw setVertexSamplerState:ss atIndex:mtl.bindVsSmp];

  if(mtl.bindFs!=uint32_t(-1))
    [encDraw setFragmentTexture:tex atIndex:mtl.bindFs];
  if(mtl.bindFsSmp!=uint32_t(-1))
    [encDraw setFragmentSamplerState:ss atIndex:mtl.bindFsSmp];

  if(mtl.bindCs!=uint32_t(-1))
    [encComp setTexture:tex atIndex:mtl.bindCs];
  if(mtl.bindCsSmp!=uint32_t(-1))
    [encComp setSamplerState:ss atIndex:mtl.bindCsSmp];
  }

void MtCommandBuffer::generateMipmap(AbstractGraphicsApi::Texture &image,
                                     TextureLayout /*defLayout*/,
                                     uint32_t /*texWidth*/, uint32_t /*texHeight*/,
                                     uint32_t /*mipLevels*/) {
  setEncoder(E_Blit,nullptr,nullptr);

  auto& t = reinterpret_cast<MtTexture&>(image);
  [encBlit generateMipmapsForTexture:t.impl];
  }

void MtCommandBuffer::setPipeline(AbstractGraphicsApi::Pipeline &p) {
  auto& px   = reinterpret_cast<MtPipeline&>(p);
  auto& inst = px.inst(*curFbo->layout.handler);

  if(curFbo->depth!=nullptr)
    [encDraw setDepthStencilState:px.depthStZ]; else
    [encDraw setDepthStencilState:px.depthStNoZ];
  [encDraw setRenderPipelineState:inst.pso];
  [encDraw setCullMode:px.cullMode];
  topology = px.topology;
  curVboId = px.lay.handler->vboIndex;
  curLay   = px.lay.handler;
  }

void Tempest::Detail::MtCommandBuffer::copy(Tempest::AbstractGraphicsApi::Buffer& dest, Tempest::TextureLayout defLayout,
                                            uint32_t width, uint32_t height, uint32_t mip,
                                            Tempest::AbstractGraphicsApi::Texture& src, size_t offset) {
  setEncoder(E_Blit,nullptr,nullptr);

  auto& s = reinterpret_cast<MtTexture&>(src);
  auto& d = reinterpret_cast<MtBuffer&> (dest);
  const uint32_t bpp = s.bitCount()/8;

  [encBlit copyFromTexture:s.impl
                           sourceSlice:0
                           sourceLevel:mip
                           sourceOrigin:MTLOriginMake(0,0,0)
                           sourceSize:MTLSizeMake(width,height,1)
                           toBuffer:d.impl
                           destinationOffset:offset
                           destinationBytesPerRow:bpp*width
                           destinationBytesPerImage:bpp*width*height];
  }
