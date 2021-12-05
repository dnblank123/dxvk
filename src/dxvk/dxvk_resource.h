#pragma once

#include "dxvk_include.h"

namespace dxvk {
  
  enum class DxvkAccess {
    Read    = 0,
    Write   = 1,
    None    = 2,
  };
  
  using DxvkAccessFlags = Flags<DxvkAccess>;
  
  /**
   * \brief DXVK resource
   * 
   * Keeps track of whether the resource is currently in use
   * by the GPU. As soon as a command that uses the resource
   * is recorded, it will be marked as 'in use'.
   */
  class DxvkResource : public RcObject {

  public:
    
    virtual ~DxvkResource();
    
    /**
     * \brief Checks whether resource is in use
     * 
     * Returns \c true if there are pending accesses to
     * the resource by the GPU matching the given access
     * type. Note that checking for reads will also return
     * \c true if the resource is being written to.
     * \param [in] access Access type to check for
     * \returns \c true if the resource is in use
     */
    bool isInUse(DxvkAccess access) const {
      bool result = m_useCount[u_v<DxvkAccess::Write>].load(std::memory_order_seq_cst) != 0;
      if (access == DxvkAccess::Read)
        result |= m_useCount[u_v<DxvkAccess::Read>].load(std::memory_order_seq_cst) != 0;
      return result;
    }
    
    template<DxvkAccess Access = DxvkAccess::Read>
    inline bool isInUse() const;

    /**
     * \brief Acquires resource
     * 
     * Increments use count for the given access type.
     * \param Access Resource access type
     */
    void acquire(DxvkAccess access) {
      if (access != DxvkAccess::None)
        m_useCount[to_int(access)].fetch_add(1, std::memory_order_seq_cst);
    }

    template<DxvkAccess Access>
    inline void acquire();

    /**
     * \brief Releases resource
     * 
     * Decrements use count for the given access type.
     * \param Access Resource access type
     */
    void release(DxvkAccess access) {
      if (access != DxvkAccess::None)
        m_useCount[to_int(access)].fetch_sub(1, std::memory_order_seq_cst);
    }

    template<DxvkAccess Access>
    inline void release();

    /**
     * \brief Waits for resource to become unused
     *
     * Blocks calling thread until the GPU finishes
     * using the resource with the given access type.
     * \param [in] access Access type to check for
     */
    void waitIdle(DxvkAccess access) const {
      sync::spin(50000, [this, access] {
        return !isInUse(access);
      });
    }
    
    template<DxvkAccess Access>
    inline void waitIdle() const;

  private:
    
    std::atomic<uint32_t> m_useCount[2] = { 0u, 0u };

  };
  
  /* isInUse() specialized for DxvkAccess::Read */
  template<>
  inline bool DxvkResource::isInUse<DxvkAccess::Read>() const {
    return m_useCount[u_v<DxvkAccess::Write>].load(std::memory_order_seq_cst) != 0
        || m_useCount[u_v<DxvkAccess::Read>].load(std::memory_order_seq_cst) != 0;
  }

  /* isInUse() specialized for DxvkAccess::Write and DxvkAccess::None */
  template<DxvkAccess>
  inline bool DxvkResource::isInUse() const {
    return m_useCount[u_v<DxvkAccess::Write>].load(std::memory_order_seq_cst) != 0;
  }

  /* acquire() specialized for DxvkAccess::Read and DxvkAccess::Write */
  template<DxvkAccess Access>
  inline void DxvkResource::acquire() {
    m_useCount[u_v<Access>].fetch_add(1, std::memory_order_seq_cst);
  }

  /* acquire() specialized for DxvkAccess::None (no-op) */
  template<> inline void DxvkResource::acquire<DxvkAccess::None>() {}

  /* release() specialized for DxvkAccess::Read and DxvkAccess::Write */
  template<DxvkAccess Access>
  inline void DxvkResource::release() {
    m_useCount[u_v<Access>].fetch_sub(1, std::memory_order_seq_cst);
  }

  /* release() specialized for DxvkAccess::None (no-op) */
  template<> inline void DxvkResource::release<DxvkAccess::None>() {}

  template<DxvkAccess Access>
  inline void DxvkResource::waitIdle() const {
    sync::spin(50000, [this] {
      return !isInUse<Access>();
    });
  }
}