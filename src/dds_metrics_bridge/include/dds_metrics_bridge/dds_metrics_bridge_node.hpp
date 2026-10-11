#pragma once

#include <chrono>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp/event_handler.hpp"
#include "rclcpp/generic_subscription.hpp"
#include "rclcpp/serialized_message.hpp"

#include "dds_metrics_bridge/msg/heartbeat.hpp"
#include "dds_metrics_bridge/msg/retransmitted_packet.hpp"
#include "dds_metrics_bridge/msg/latency_bytes.hpp"
#include "dds_metrics_bridge/msg/discovery_event.hpp"

#include "dds_metrics_bridge/discovery_stats_watcher.hpp"
#include "dds_metrics_bridge/self_probe.hpp"

namespace dds_metrics_bridge
{

/// Surfaces low-level DDS transport health as standard ROS 2 topics:
///   /dds/heartbeats           - per-topic matched/liveliness snapshots
///   /dds/retransmitted_packets - RTPS retransmit/discard counters
///   /dds/latency_bytes        - per-topic throughput/period
///   /dds/discovery_events     - new participant/endpoint discovery events
///
/// See README.md for the architecture rationale (why these four sources,
/// not a single DDS-vendor "statistics module").
class DdsMetricsBridgeNode : public rclcpp::Node
{
public:
  explicit DdsMetricsBridgeNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

private:
  struct WatchedTopic
  {
    rclcpp::GenericSubscription::SharedPtr subscription;
    std::chrono::steady_clock::time_point last_message_time{};
    double last_period_ms = 0.0;
    uint64_t bytes_in_window = 0;
    uint32_t samples_in_window = 0;
    rmw_matched_status_t matched{};
    rmw_liveliness_changed_status_t liveliness{};
  };

  void rescan_topics();
  bool matches_watch_pattern(const std::string & topic_name) const;
  void watch_topic(const std::string & topic_name, const std::string & type_name);

  void on_report_timer();
  void publish_rexmit_counters();
  void publish_probe_counters();
  void publish_discovery_events();

  std::vector<std::regex> include_patterns_;
  std::unordered_map<std::string, WatchedTopic> watched_;

  rclcpp::TimerBase::SharedPtr rescan_timer_;
  rclcpp::TimerBase::SharedPtr report_timer_;
  rclcpp::TimerBase::SharedPtr probe_timer_;

  rclcpp::Publisher<msg::Heartbeat>::SharedPtr heartbeat_pub_;
  rclcpp::Publisher<msg::RetransmittedPacket>::SharedPtr rexmit_pub_;
  rclcpp::Publisher<msg::LatencyBytes>::SharedPtr latency_pub_;
  rclcpp::Publisher<msg::DiscoveryEvent>::SharedPtr discovery_pub_;

  std::unique_ptr<DiscoveryStatsWatcher> discovery_watcher_;
  std::unique_ptr<SelfProbe> self_probe_;
};

}  // namespace dds_metrics_bridge
