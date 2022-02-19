#include "d3d9_mem.h"
#include "../util/util_string.h"
#include "../util/log/log.h"
#include "../util/util_math.h"
#include <utility>

namespace dxvk {
  D3D9MemoryAllocator::D3D9MemoryAllocator() {}

  D3D9MemoryAllocator::~D3D9MemoryAllocator() {}

  D3D9Memory D3D9MemoryAllocator::Alloc(uint32_t Size) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    uint32_t alignedSize = align(Size, sizeof(void*));
    m_usedMemory += alignedSize;
    for (auto chunk : m_chunks) {
      D3D9Memory memory = chunk->Alloc(alignedSize);
      if (memory) {
        return memory;
      }
    }

    uint32_t pageAlignedSize = align(Size, 4 << 10);
    uint32_t chunkSize = std::max(D3D9ChunkSize, pageAlignedSize);
    m_allocatedMemory += chunkSize;

    D3D9MemoryChunk* chunk = new D3D9MemoryChunk(this, chunkSize);
    D3D9Memory memory = chunk->Alloc(alignedSize);

    m_chunks.push_back(chunk);
    return memory;
  }

  void D3D9MemoryAllocator::NotifyMapped(uint32_t Size) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    m_mappedMemory += Size;
    Logger::warn(str::format("Mapped memory (MiB): ", m_mappedMemory >> 20, " allocated memory (MiB): ", m_allocatedMemory >> 20, " mapped diff (KiB): +", Size >> 10));
  }

  void D3D9MemoryAllocator::NotifyUnmapped(uint32_t Size) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    m_mappedMemory -= Size;
    Logger::warn(str::format("Mapped memory (MiB): ", m_mappedMemory >> 20, " allocated memory (MiB): ", m_allocatedMemory >> 20, " mapped diff (KiB): -", Size >> 10));
  }

  void D3D9MemoryAllocator::NotifyFreed(uint32_t Size) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    m_usedMemory -= Size;
  }

  D3D9MemoryChunk::D3D9MemoryChunk(D3D9MemoryAllocator* Allocator, uint32_t Size)
    : m_allocator(Allocator), m_size(Size) {
    m_mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, Size, nullptr);
    m_freeRanges.push_back({ 0, Size });
  }

  D3D9MemoryChunk::~D3D9MemoryChunk() {
    if (m_ptr != nullptr) {
      UnmapViewOfFile(m_ptr);
    }
    CloseHandle(m_mapping);
  }

  void D3D9MemoryChunk::IncMapCounter() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    m_mapCounter++;

    if (m_mapCounter == 1) {
      m_allocator->NotifyMapped(m_size);
      m_ptr = MapViewOfFile(m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, m_size);
    }
  }

  void D3D9MemoryChunk::DecMapCounter() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    m_mapCounter--;

    if (m_mapCounter == 0) {
      m_allocator->NotifyUnmapped(m_size);
      UnmapViewOfFile(m_ptr);
      m_ptr = nullptr;
    }
  }

  D3D9Memory D3D9MemoryChunk::Alloc(uint32_t Size) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    uint32_t offset = 0;
    uint32_t size = 0;

    for (auto range = m_freeRanges.begin(); range != m_freeRanges.end(); range++) {
      if (range->length >= Size) {
        offset = range->offset;
        size = Size;
        range->offset += Size;
        range->length -= Size;
        if (range->length < (4 << 10)) {
          size += range->length;
          m_freeRanges.erase(range);
        }
        break;
      }
    }

    if (size != 0)
      return D3D9Memory(this, offset, size);

    return {};
  }

  void D3D9MemoryChunk::Free(D3D9Memory *Memory) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    uint32_t offset = Memory->GetOffset();
    uint32_t length = Memory->GetSize();

    auto curr = m_freeRanges.begin();

    // shamelessly stolen from dxvk_memory.cpp
    while (curr != m_freeRanges.end()) {
      if (curr->offset == offset + length) {
        length += curr->length;
        curr = m_freeRanges.erase(curr);
      } else if (curr->offset + curr->length == offset) {
        offset -= curr->length;
        length += curr->length;
        curr = m_freeRanges.erase(curr);
      } else {
        curr++;
      }
    }

    m_freeRanges.push_back({ offset, length });
    m_allocator->NotifyFreed(Memory->GetSize());
  }

  D3D9Memory::D3D9Memory() { }

  D3D9Memory::D3D9Memory(D3D9MemoryChunk* Chunk, size_t Offset, size_t Size)
    : m_chunk(Chunk), m_offset(Offset), m_size(Size) { }

  D3D9Memory::D3D9Memory(D3D9Memory&& other)
    : m_chunk(std::exchange(other.m_chunk, nullptr)),
      m_isMappped(std::exchange(other.m_isMappped, false)),
      m_offset(std::exchange(other.m_offset, 0)),
      m_size(std::exchange(other.m_size, 0)) {}

  D3D9Memory::~D3D9Memory() {
    this->Free();
  }

  D3D9Memory& D3D9Memory::operator = (D3D9Memory&& other) {
    this->Free();

    m_chunk = std::exchange(other.m_chunk, nullptr);
    m_isMappped = std::exchange(other.m_isMappped, false);
    m_offset = std::exchange(other.m_offset, 0);
    m_size = std::exchange(other.m_size, 0);
    return *this;
  }

  void D3D9Memory::Free() {
    if (!m_chunk)
      return;

    if (m_isMappped)
      m_chunk->DecMapCounter();

    m_chunk->Free(this);
  }

  void D3D9Memory::Map() {
    if (m_isMappped)
      return;

    if (m_chunk == nullptr) {
      Logger::err("Tried to map dead memory");
      return;
    }

    m_chunk->IncMapCounter();
    m_isMappped = true;
  }

  void D3D9Memory::Unmap() {
    if (!m_isMappped)
      return;

    m_isMappped = false;
    m_chunk->DecMapCounter();
  }

  void* D3D9Memory::Ptr() {
    if (!m_isMappped) {
      Logger::err("Tried accessing unmapped memory.");
      return nullptr;
    }

    uint8_t* ptr = reinterpret_cast<uint8_t*>(m_chunk->Ptr());
    return ptr + m_offset;
  }
}