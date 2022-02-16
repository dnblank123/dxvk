
#pragma once

#include "../util/thread.h"
#include "../util/rc/util_rc.h"
#include "../util/rc/util_rc_ptr.h"

#define WIN32_LEAN_AND_MEAN
#include <winbase.h>

namespace dxvk {

  constexpr uint32_t D3D9ChunkSize = 64 << 20;

  class D3D9MemoryAllocator;
  class D3D9Memory;
  class D3D9MemoryChunk;

  struct D3D9MemoryRange {
    uint32_t offset;
    uint32_t length;
  };

  class D3D9MemoryChunk {
    public:
      D3D9MemoryChunk();
      D3D9MemoryChunk(D3D9MemoryAllocator* Allocator, uint32_t Size);
      ~D3D9MemoryChunk();

      void IncMapCounter();
      void DecMapCounter();
      void* Ptr() const { return m_ptr; }
      D3D9Memory Alloc(uint32_t Size);
      void Free(D3D9Memory* Memory);

    private:
      dxvk::mutex m_mutex;
      D3D9MemoryAllocator* m_allocator;
      uint32_t m_mapCounter = 0;
      HANDLE m_mapping;
      void* m_ptr;
      uint32_t m_size;
      std::vector<D3D9MemoryRange> m_freeRanges;
  };

  class D3D9Memory {
    public:
      D3D9Memory();
      D3D9Memory(D3D9MemoryChunk* Chunk, size_t Offset, size_t Size);
      ~D3D9Memory();

      D3D9Memory             (const D3D9Memory&) = delete;
      D3D9Memory& operator = (const D3D9Memory&) = delete;

      D3D9Memory             (D3D9Memory&& other);
      D3D9Memory& operator = (D3D9Memory&& other);

      operator bool() const { return m_chunk != nullptr; }

      void Map();
      void Unmap();
      void* Ptr();
      D3D9MemoryChunk* GetChunk() const { return m_chunk; }
      size_t GetOffset() const { return m_offset; }
      size_t GetSize() const { return m_size; }

    private:
      void Free();

      D3D9MemoryChunk* m_chunk = nullptr;
      bool m_isMappped         = false;
      size_t m_offset          = 0;
      size_t m_size            = 0;
  };

  class D3D9MemoryAllocator {
    public:
      D3D9MemoryAllocator();
      ~D3D9MemoryAllocator();
      D3D9Memory Alloc(uint32_t Size);
      void NotifyMapped(uint32_t Size);
      void NotifyUnmapped(uint32_t Size);
      void NotifyFreed(uint32_t Size);

    private:
      dxvk::mutex m_mutex;
      std::vector<D3D9MemoryChunk*> m_chunks;
      size_t m_mappedMemory = 0;
      size_t m_allocatedMemory = 0;
      size_t m_usedMemory = 0;
  };

}
