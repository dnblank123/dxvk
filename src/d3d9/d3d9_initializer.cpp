#include <cstring>

#include "d3d9_initializer.h"

namespace dxvk {

  D3D9Initializer::D3D9Initializer(
    const Rc<DxvkDevice>&             Device)
  : m_device(Device), m_context(m_device->createContext()) {
    m_context->beginRecording(
      m_device->createCommandList());
  }

  
  D3D9Initializer::~D3D9Initializer() {

  }


  void D3D9Initializer::Flush() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (m_transferCommands != 0)
      FlushInternal();
  }


  void D3D9Initializer::InitBuffer(
          D3D9CommonBuffer*  pBuffer) {
    VkMemoryPropertyFlags memFlags = pBuffer->GetBuffer<D3D9_COMMON_BUFFER_TYPE_REAL>()->memFlags();

    (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
      ? InitHostVisibleBuffer(pBuffer->GetBufferSlice<D3D9_COMMON_BUFFER_TYPE_REAL>())
      : InitDeviceLocalBuffer(pBuffer->GetBufferSlice<D3D9_COMMON_BUFFER_TYPE_REAL>());

    if (pBuffer->GetMapMode() == D3D9_COMMON_BUFFER_MAP_MODE_BUFFER)
      InitHostVisibleBuffer(pBuffer->GetBufferSlice<D3D9_COMMON_BUFFER_TYPE_STAGING>());
  }
  

  void D3D9Initializer::InitTexture(
          D3D9CommonTexture* pTexture,
          void*              pInitialData) {
    if (pTexture->GetMapMode() != D3D9_COMMON_TEXTURE_MAP_MODE_BACKED)
      return;

    InitDeviceLocalTexture(pTexture);
  }


  void D3D9Initializer::InitDeviceLocalBuffer(
          DxvkBufferSlice    Slice) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    m_transferCommands += 1;

    m_context->initBuffer(
      Slice.buffer());

    FlushImplicit();
  }


  void D3D9Initializer::InitHostVisibleBuffer(
          DxvkBufferSlice    Slice) {
    // If the buffer is mapped, we can write data directly
    // to the mapped memory region instead of doing it on
    // the GPU. Same goes for zero-initialization.
    std::memset(
      Slice.mapPtr(0), 0,
      Slice.length());
  }


  void D3D9Initializer::InitDeviceLocalTexture(
          D3D9CommonTexture* pTexture) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    auto InitImage = [&](Rc<DxvkImage> image) {
      if (image == nullptr)
        return;

      auto formatInfo = imageFormatInfo(image->info().format);

      m_transferCommands += 1;
      
      // While the Microsoft docs state that resource contents are
      // undefined if no initial data is provided, some applications
      // expect a resource to be pre-cleared.
      VkImageSubresourceRange subresources;
      subresources.aspectMask     = formatInfo->aspectMask;
      subresources.baseMipLevel   = 0;
      subresources.levelCount     = image->info().mipLevels;
      subresources.baseArrayLayer = 0;
      subresources.layerCount     = image->info().numLayers;

      m_context->initImage(image, subresources, VK_IMAGE_LAYOUT_UNDEFINED);
    };

    InitImage(pTexture->GetImage());

    FlushImplicit();
  }


  void D3D9Initializer::FlushImplicit() {
    if (m_transferCommands > MaxTransferCommands
     || m_transferMemory   > MaxTransferMemory)
      FlushInternal();
  }


  void D3D9Initializer::FlushInternal() {
    m_context->flushCommandList();
    
    m_transferCommands = 0;
    m_transferMemory   = 0;
  }

}