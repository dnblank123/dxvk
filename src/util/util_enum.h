#pragma once

#define ENUM_NAME(name) \
  case name: return os << #name

#define ENUM_DEFAULT(name) \
  default: return os << static_cast<int32_t>(e)

#include <type_traits>

namespace dxvk {

  /**
   * Convenience template alias for std::underlying_type_t
   */
  template<typename T>
  using u_t = std::underlying_type_t<T>;

  /**
   * Casts a scoped enumerator constant expression to its underlying type
   */
  template<auto I>
  inline constexpr auto u_v = static_cast<u_t<decltype(I)>>(I);

  /**
   * Casts a scoped enumerator to its underlying type
   */
  template<typename T>
  inline constexpr auto to_int(T v) {
    return static_cast<u_t<T>>(v);
  }

}
