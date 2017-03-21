#ifndef DOG_SINGLETON_H
#define DOG_SINGLETON_H

#include <memory>

namespace dog {
  template <class Host>
  class Singleton {
    public:
    static Host& instance() {
      if (!host)
	host = std::make_unique<Host>();

      return *host.get();
    }
    Singleton() = delete;
    private:
    static std::unique_ptr<Host> host;
  };

  template <class Host>
  std::unique_ptr<Host> Singleton<Host>::host = nullptr;
} // namespace dog

#endif /* DOG_SINGLETON_H */
