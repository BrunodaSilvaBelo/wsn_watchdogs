#include "network.h"
#include "gsl/gsl"
#include <iostream>
#include <random>
#include <thread>

using namespace dog;

namespace {
std::mt19937 seed(1);

enum class package_data { sender, destination, type, id };
std::vector<std::mutex> mutexes;
}

static auto get_random_id() {
  static std::uniform_int_distribution<> gen(0, 1000);
  return gen(seed);
}

static auto is_failed() {
  static std::uniform_int_distribution<> gen(1, 100);
  return static_cast<std::size_t>(gen(seed)) >
         Network::instance().chance_to_fail;
}

template <typename T> static void log(T&& message) {
  if (Network::instance().debug) {
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
    std::copy_n(std::begin(aux_route), i + 1, std::back_inserter(route));
    bool repeated = false;
    for (auto& z : Network::instance().path[id][aux_route[i]])
      if (z.first == route)
        repeated = true;
    if (repeated)
      continue;
    auto rater = 0.f;
    for (auto& x : route)
      rater += Network::instance().pathrater[id][x];
    Network::instance().path[id][aux_route[i]].push_back(
        std::make_pair(route, rater / route.size()));
    route.clear();
  }
}

template <typename T> static auto choose_best_route(T sender, T destination) {
  auto& routes = Network::instance().path[sender][destination];
  for (auto& x : routes) {
    x.second = 0;
    for (auto& i : x.first)
      x.second += Network::instance().pathrater[sender][i];
    x.second /= x.first.size();
  }

  auto result = std::max_element(std::begin(routes), std::end(routes),
                                 [](auto& x, auto& y) {
                                   if (x.second < y.second)
                                     return true;
                                   if (x.second == y.second)
                                     return x.first.size() < y.first.size();

                                   return false;
                                 });

  return (*result).first;
}
void Network_Impl::generate(std::size_t size) {
  mutexes = std::vector<std::mutex>(size);
  std::vector<bool> graph_col(size, false);
  graph = std::vector<std::vector<bool>>(size, std::move(graph_col));

  std::uniform_int_distribution<> gen(0, 1);
  std::uniform_int_distribution<> gen2(0, size);
  for (std::size_t i = 0; i < size; ++i) {
    auto con{static_cast<std::size_t>(gen2(seed))/2};
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

  std::vector<float> pathrater_col(size, 0.5f);
  pathrater = std::vector<std::vector<float>>(size, std::move(pathrater_col));
  for (std::size_t i = 0; i < size; ++i)
    pathrater[i][i]  = 1.f;

  path.resize(size);
  for (auto& i : path)
    i.resize(size);

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
    package.route = choose_best_route(id, package.destination);
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
        package.route = choose_best_route(id, package.destination);
        if (Network::instance().watchdog)
          watching.emplace_back(package, Clock::now());
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
  auto update_time   = previous_time;
  static std::uniform_int_distribution<> gen(0, Network::instance().size() - 1);
  auto random_send        = gen(seed);
  std::size_t last_number = 2;
  for (;;) {
    auto current_time = Clock::now();

    if (!package_buffer.empty()) {
      mutexes[id].lock();
      auto package = package_buffer.front();
      package_buffer.pop_front();
      mutexes[id].unlock();
      if (package.type == message_type::route_request) {
        auto repeated =
            std::find(std::begin(route_requests), std::end(route_requests),
                      std::make_pair(package.sender, package.id));
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
          if (repeated == std::end(route_requests)) {
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
            if (s == status::normal) {
              send(package);
              if (Network::instance().watchdog) {
                decltype(package.route) route;
                std::reverse_copy(std::begin(package.route),
                                  std::end(package.route),
                                  std::back_inserter(route));
                package.type  = message_type::watchdog;
                package.route = route;
                send(package);
              }
            } else {
	      log("++++");
	    }
          }
        }
      } else if (package.type == message_type::watchdog) {
        if (Network::instance().watchdog)
          Network::instance().pathrater[id][package.sender] += 0.01;
      }
    }

    if (Network::instance().watchdog) {      {
        std::lock_guard<std::mutex> locker(mutexes[id]);
        for (std::size_t i = 0, end = package_buffer.size(); i < end; ++i) {
          std::vector<std::size_t> erased;
          for (std::size_t j = 0; j < watching.size(); ++j) {
            if (diff_time(current_time, watching[j].second) >
                Network::instance().timeout)
              erased.push_back(j);
            if (package_buffer[i].type == message_type::watchdog)
              if (package_buffer[i].sender == watching[j].first.sender &&
                  package_buffer[i].destination ==
                      watching[j].first.destination &&
                  package_buffer[i].id == watching[j].first.id)
                erased.push_back(j);
          }

          for (auto& x : erased)
            watching.erase(std::begin(watching) + x);
          if (watching.empty())
            break;
        }
      }

      for (auto& i : watching)
        if (diff_time(current_time, i.second) > Network::instance().timeout) {
          Network::instance().pathrater[id][i.first.route[1]] = -100.f;
          log("----");
        }


    }

    if (diff_time(current_time, update_time) >
        ((60 * Network::instance().minutes_running) / 7)) {
      s = (is_failed()) ? status::misbehaved : status::normal;
      if (Network::instance().watchdog)
        watching.clear();
      update_time = current_time;
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
      ++Network::instance().throughput[id].first;
      log("**** Node " + std::to_string(id) + " sending data to " +
          std::to_string(des));
      previous_time = current_time;
    }

    if (diff_time(current_time, start_time) >
        60 * Network::instance().minutes_running)
      break;
  }
}

void Node::receive(Package package) {
  std::lock_guard<std::mutex> locker(mutexes[id]);
  package_buffer.push_back(package);
}

Package::Package(std::size_t _sender, std::size_t _destination,
                 message_type _type)
    : sender(_sender), destination(_destination), id(get_random_id()),
      type(_type) {}
