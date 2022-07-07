#pragma once

#include "d3d9_device.h"
#include "../dxvk/hud/dxvk_hud_item.h"

namespace dxvk::hud {

  /**
   * \brief HUD item to display sampler count
   */
  class HudSamplerCount : public HudItem {

  public:

    HudSamplerCount(D3D9DeviceEx* device);

    void update(dxvk::high_resolution_clock::time_point time);

    HudPos render(
            HudRenderer&      renderer,
            HudPos            position);

  private:

    D3D9DeviceEx* m_device;

    std::string m_samplerCount;

  };

    /**
     * \brief HUD item to display unmappable memory
     */
    class HudManagedMemory : public HudItem {

    public:

        HudManagedMemory(D3D9DeviceEx* device);

        void update(dxvk::high_resolution_clock::time_point time);

        HudPos render(
                HudRenderer&      renderer,
                HudPos            position);

    private:

        D3D9DeviceEx* m_device;

        std::string m_memoryText;

    };

}