/**
   \mainpage
   \brief
   \author Bruno da Silva Belo
   \see https://github.com/isocpp/CppCoreGuidelines
*/
#include "network.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <thread>

using namespace dog;

namespace {
std::size_t const nodes_size = 50;
}

int main(int argc, char** argv) {
  Network::instance().generate(nodes_size);
  for (auto& x : Network::instance().graph) {
    for (auto y : x)
      std::cout << y << " ";
    std::cout << "\n";
  }
  std::vector<std::thread> threads(nodes_size);

  for (std::size_t i = 0; i < nodes_size; ++i)
    Network::instance().nodes.emplace_back(i);

  for (std::size_t i = 0; i < nodes_size; ++i)
    threads[i]       = std::thread(
        [](std::size_t id) { Network::instance().nodes[id].run(); }, i);

  for (auto& i : threads)
    i.join();

  std::cout << "Throughput is ";
  std::size_t total_data = 0, arrived_data = 0;

  for (auto& i : Network::instance().throughput) {
    total_data += i.first;
    arrived_data += i.second;
  }
  std::cout << static_cast<float>(arrived_data) / static_cast<float>(total_data)
            << " " << arrived_data << " " << total_data << "\n";

  std::size_t metric_data = 0, metric_non_data = 0;
  for (auto& i : Network::instance().metric) {
    metric_data += i[message_type::data];
    metric_non_data = i[message_type::watchdog] + i[message_type::route_reply] +
                      i[message_type::route_request] +
                      i[message_type::route_error];
  }

  std::cout << "Rate between data package and non data package is ";
  std::cout << static_cast<float>(metric_data) /
                   static_cast<float>(metric_non_data)
            << "\n";

  return 0;
}
