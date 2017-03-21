#ifndef DOG_NETWORK_H
#define DOG_NETWORK_H

#include "singleton.h"
#include <chrono>
#include <deque>
#include <map>
#include <mutex>
#include <tuple>
#include <vector>

namespace dog {
enum class message_type {
  data,
  route_request,
  route_reply,
  route_error,
  watchdog
};

enum class communication_type { unicast, broadcast };

class Node;
class Package;
class Network_Impl {
public:
  void generate(std::size_t size);
  std::size_t size() const;

  std::size_t chance_to_fail = 0;
  std::vector<std::vector<bool>> graph;
  std::vector<std::vector<std::deque<std::size_t>>> path;
  std::vector<std::vector<double>> pathrater;
  std::vector<std::map<message_type, std::size_t>> metric;
  std::vector<std::pair<std::size_t, std::size_t>> throughput;
  std::vector<Node> nodes;
};

class Node {
public:
  enum class status { paused, running };
  Node(std::size_t id);
  void send(std::size_t destination);
  void send(Package package,
            communication_type comm = communication_type::unicast);
  void receive(Package package);
  void run();

  std::size_t id;
  status s;
  std::deque<Package> package_buffer;
  std::vector<std::pair<std::size_t, std::size_t>> route_requests;
  std::vector<std::pair<std::size_t,
                        std::chrono::time_point<std::chrono::steady_clock>>>
      watching;
};

class Package {
public:
  Package(std::size_t sender, std::size_t destination, message_type type);
  std::deque<std::size_t>& get_route();
  std::size_t sender;
  std::size_t destination;
  std::size_t id;
  message_type type;
  std::deque<std::size_t> route;
};

using Network = Singleton<Network_Impl>;
using Clock   = std::chrono::steady_clock;
} // namespace dog

#endif /* DOG_NETWORK_H */
