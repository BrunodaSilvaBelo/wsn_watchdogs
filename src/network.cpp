#include "network.h"
#include "gsl/gsl"
#include <iostream>
#include <random>
#include <thread>

using namespace dog;

namespace {
std::mt19937 seed(1);

enum class package_data { sender, destination, type, id };
}

static auto get_random_id() {
  static std::uniform_int_distribution<> gen(0, 1000);
  return gen(seed);
}

static auto is_failed() {
  static std::uniform_int_distribution<> gen(0, 100);
  return static_cast<std::size_t>(gen(seed)) <
         Network::instance().chance_to_fail;
}

template <typename T> static void log(T&& message) {
  if (false) {
    static std::mutex mu;
    std::lock_guard<std::mutex> locker(mu);
    std::cout << std::forward<T>(message) << "\n";
  }
}

template <typename T> static void check_route(T route) {
  std::string a = "";
  for (auto& i : route) {
    a += std::to_string(i) + " ";
  }
  log("A route is " + a);
}

template <typename Container>
static auto next_route(Container const& container, std::size_t id) {
  std::size_t i = 0;
  for (; i < container.size(); ++i) {
    if (container[i] == id)
      break;
  }
  if (i != Network::instance().size() - 1)
    ++i;

  return i;
}

static void transmit(Package package, std::size_t hop) {
  Network::instance().nodes[hop].receive(package);
}

template <typename Container>
static void update_cache(Container const& container, std::size_t id) {
  auto idx{next_route(container, id)};
  Container aux_route;
  std::copy_n(std::begin(container), idx, std::front_inserter(aux_route));
  Container route;
  for (std::size_t i = 1; i < aux_route.size(); ++i) {
    if (!Network::instance().path[id][aux_route[i]].empty())
      continue;

    std::copy_n(std::begin(aux_route), i + 1, std::back_inserter(route));
    Network::instance().path[id][aux_route[i]] = route;
    route.clear();
  }
}

void Network_Impl::generate(std::size_t size) {
  std::vector<bool> graph_col(size, false);
  graph = std::vector<std::vector<bool>>(size, std::move(graph_col));

  std::uniform_int_distribution<> gen(0, 1);
  std::uniform_int_distribution<> gen2(0, size);
  for (std::size_t i = 0; i < size; ++i) {
    auto con{static_cast<std::size_t>(gen2(seed))};
    for (std::size_t j = 0, counter = 0; j < size; ++j) {
      if (con == counter)
        break;
      ++counter;
      graph[i][j] = gen(seed);
      graph[j][i] = graph[i][j];
    }
    graph[i][i] = true;
    if (i > 0)
      graph[i - 1][i] = true;
  }

  std::vector<double> pathrater_col(size, 0.5);
  pathrater = std::vector<std::vector<double>>(size, std::move(pathrater_col));
  for (std::size_t i = 0; i < size; ++i)
    for (std::size_t j = i; j < size; ++j)
      if (i == j)
        graph[i][i] = 1.;

  std::vector<std::deque<std::size_t>> path_col(size);
  path = std::vector<std::vector<std::deque<std::size_t>>>(size,
                                                           std::move(path_col));

  std::map<message_type, std::size_t> metrics{{message_type::data, 0},
                                              {message_type::route_reply, 0},
                                              {message_type::route_error, 0},
                                              {message_type::route_request, 0},
                                              {message_type::watchdog, 0}};
  metric = std::vector<std::map<message_type, std::size_t>>(size,
                                                            std::move(metrics));

  throughput = std::vector<std::pair<std::size_t, std::size_t>>(
      size, std::make_pair(0, 0));
}

std::size_t Network_Impl::size() const { return nodes.size(); }

Node::Node(std::size_t _id) : id(_id) {}

void Node::send(std::size_t destination) {
  Package package(id, destination, message_type::data);

  if (Network::instance().path[id][destination].empty()) {
    receive(package);
    package = Package(id, destination, message_type::route_request);
    package.route.push_back(id);
    send(package, communication_type::broadcast);
  } else {
    package.route = Network::instance().path[id][destination];
    send(package);
  }
}

void Node::send(Package package, communication_type comm) {
  if (comm == communication_type::broadcast) {
    for (std::size_t i = 0, end = Network::instance().size(); i < end; ++i)
      if (Network::instance().graph[id][i] == true) {
        ++Network::instance().metric[id][package.type];
        transmit(package, i);
      }
  } else {
    if (package.type == message_type::data) {
      if (Network::instance().path[id][package.destination].empty()) {
        receive(package);
        return;
      } else {
        package.route = Network::instance().path[id][package.destination];
      }
    }
    ++Network::instance().metric[id][package.type];
    auto i = next_route(package.route, id);
    transmit(package, package.route[i]);
  }
}

void Node::run() {
  auto diff_time = [](auto const& x, auto const& y) {
    return std::chrono::duration<double>(x - y).count();
  };
  auto previous_time = Clock::now();
  auto start_time    = previous_time;
  static std::uniform_int_distribution<> gen(0, Network::instance().size());
  auto random_send = gen(seed);
  for (;;) {
    auto current_time = Clock::now();

    if (!package_buffer.empty()) {
      auto package = package_buffer.front();
      package_buffer.pop_front();

      if (package.type == message_type::route_request) {
        auto repeated =
            std::find(std::begin(route_requests), std::end(route_requests),
                      std::make_pair(package.sender, package.id));
        if (repeated == std::end(route_requests)) {
          if (package.destination == id) {
            package.route.push_back(id);
            log("#### Route request from " + std::to_string(package.sender) +
                " arrive to " + std::to_string(id));
            update_cache(package.route, id);
            Package pac(id, package.sender, message_type::route_reply);
            auto route = package.route;
            std::reverse(std::begin(route), std::end(route));
            pac.route = route;
            send(pac);
          } else {
            package.route.push_back(id);
            send(package, communication_type::broadcast);
          }
        }
        route_requests.push_back(std::make_pair(package.sender, package.id));

      } else if (package.type == message_type::route_reply) {
        if (package.destination == id) {
          log("#### Route reply from " + std::to_string(package.sender) +
              " arrive to " + std::to_string(id));
        } else {
          send(package);
        }
        update_cache(package.route, id);
      } else if (package.type == message_type::data) {
        if (package.destination == id) {
          ++Network::instance().throughput[id].second;
          log("**** A data from " + std::to_string(package.sender) +
              " arrives to " + std::to_string(id));
        } else {
          update_cache(package.route, id);
	  if (id == package.sender)
	    send(package);
	  else {
	    if (!is_failed()) {
	      send(package);
              auto watcher{next_route(package.route, id)};
              if (watcher == package.route.size() - 1)
                --watcher;
              else
                watcher -= 2;
              Package pac(id, watcher, message_type::watchdog);
              send(pac);
            }
	  }
        }
      } else if (package.type == message_type::watchdog) {

      }
    }

    for (auto& i : watching) {
      if (diff_time(current_time,i.second) > 10)

    }

    if (diff_time(current_time, previous_time) > 5 + random_send) {
      random_send = gen(seed);
      auto des    = static_cast<std::size_t>(gen(seed));
      if (des == id) {
        if (des == Network::instance().size() - 1)
          --des;
        else
          ++des;
      }
      send(des);
      watching.emplace_back(des, current_time);
      ++Network::instance().throughput[id].first;
      log("**** Node " + std::to_string(id) + " sending data to " +
          std::to_string(des));
      previous_time = current_time;
    }

    if (diff_time(current_time, start_time) > 60 * 5)
      break;
  }
}

void Node::receive(Package package) {
  static std::mutex mu;
  std::lock_guard<std::mutex> locker(mu);
  package_buffer.push_back(package);
}

Package::Package(std::size_t _sender, std::size_t _destination,
                 message_type _type)
    : sender(_sender), destination(_destination), id(get_random_id()),
      type(_type) {}
