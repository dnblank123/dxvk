#include <cstdint>
#include <list>
#include <unordered_map>

namespace dxvk {

  template<typename T>
  class lru_list {

  public:
    typedef typename std::list<T>::const_iterator lru_iterator;

    void insert(T value) {
      auto cacheIter = m_cache.find(value);
      if (cacheIter != m_cache.end())
        m_list.erase(cacheIter->second);

      m_list.push_back(value);
      auto iter = m_list.cend();
      iter--;
      m_cache[value] = iter;
    }

    void remove(const T& value) {
      auto cacheIter = m_cache.find(value);
      if (cacheIter == m_cache.end())
        return;

      m_list.erase(cacheIter->second);
      m_cache.erase(cacheIter);
    }

    lru_iterator remove(lru_iterator iter) {
      auto cacheIter = m_cache.find(*iter);
      m_cache.erase(cacheIter);
      return m_list.erase(iter);
    }

    void touch(const T& value) {
      auto cacheIter = m_cache.find(value);
      if (cacheIter == m_cache.end())
        return;

      m_list.erase(cacheIter->second);
      m_list.push_back(value);
      auto iter = m_list.cend();
      --iter;
      m_cache[value] = iter;
    }

    lru_iterator leastRecentlyUsedIter() {
      return m_list.begin();
    }

    lru_iterator leastRecentlyUsedEndIter() {
      return m_list.end();
    }

    uint32_t size() const noexcept {
      return m_list.size();
    }

  private:
    std::list<T> m_list;
    std::unordered_map<T, lru_iterator, DxvkHash, DxvkEq> m_cache;

  };

}
