#include "dds_metrics_bridge/dds_metrics_bridge_node.hpp"

#include <cstdlib>
#include <iterator>
#include <utility>

namespace dds_metrics_bridge
{

namespace
{

// Reasonable defaults for the AGV delivery fleet this tool was built
// against (Nav2 + Open-RMF): cmd_vel/odom/amcl per-robot topics, shared /tf,
// RMF fleet/task state, and laser scans. Override via the
// 'watched_topic_patterns' parameter for other projects.
const std::vector<std::string> kDefaultPatterns = {
  "^/tf$", "^/tf_static$",
  ".*cmd_vel.*", ".*odom.*", ".*amcl_pose.*", ".*scan.*",
  ".*fleet_state.*", ".*task_state.*",
};

}  // namespace

DdsMetricsBridgeNode::DdsMetricsBridgeNode(const rclcpp::NodeOptions & options)
: rclcpp::Node("dds_metrics_bridge", options)
{
  const auto patterns = this->declare_parameter<std::vector<std::string>>(
    "watched_topic_patterns", kDefaultPatterns);
  for (const auto & p : patterns) {
    include_patterns_.emplace_back(p);
  }

  const double rescan_period_sec = this->declare_parameter<double>("rescan_period_sec", 5.0);
  const double report_period_sec = this->declare_parameter<double>("report_period_sec", 1.0);
  const double probe_period_sec = this->declare_parameter<double>("probe_period_sec", 0.2);

  heartbeat_pub_ = this->create_publisher<msg::Heartbeat>("/dds/heartbeats", 10);
  rexmit_pub_ = this->create_publisher<msg::RetransmittedPacket>("/dds/retransmitted_packets", 10);
  latency_pub_ = this->create_publisher<msg::LatencyBytes>("/dds/latency_bytes", 10);
  discovery_pub_ = this->create_publisher<msg::DiscoveryEvent>("/dds/discovery_events", 10);

  discovery_watcher_ = std::make_unique<DiscoveryStatsWatcher>();
  self_probe_ = std::make_unique<SelfProbe>(discovery_watcher_->participant());

  rescan_topics();
  rescan_timer_ = this->create_wall_timer(
    std::chrono::duration<double>(rescan_period_sec),
    std::bind(&DdsMetricsBridgeNode::rescan_topics, this));
  report_timer_ = this->create_wall_timer(
    std::chrono::duration<double>(report_period_sec),
    std::bind(&DdsMetricsBridgeNode::on_report_timer, this));
  probe_timer_ = this->create_wall_timer(
    std::chrono::duration<double>(probe_period_sec),
    [this]() {self_probe_->tick();});

  const char * domain_env = std::getenv("ROS_DOMAIN_ID");
  RCLCPP_INFO(
    this->get_logger(),
    "dds_metrics_bridge started on ROS_DOMAIN_ID=%s, watching %zu topic pattern(s)",
    domain_env ? domain_env : "0 (default)", include_patterns_.size());
}

bool DdsMetricsBridgeNode::matches_watch_pattern(const std::string & topic_name) const
{
  for (const auto & re : include_patterns_) {
    if (std::regex_search(topic_name, re)) {
      return true;
    }
  }
  return false;
}

void DdsMetricsBridgeNode::rescan_topics()
{
  for (const auto & topic_and_types : this->get_topic_names_and_types()) {
    const std::string & topic_name = topic_and_types.first;
    const std::vector<std::string> & types = topic_and_types.second;
    if (types.empty() || watched_.count(topic_name) > 0) {
      continue;
    }
    if (!matches_watch_pattern(topic_name)) {
      continue;
    }
    watch_topic(topic_name, types.front());
  }
}

void DdsMetricsBridgeNode::watch_topic(const std::string & topic_name, const std::string & type_name)
{
  WatchedTopic wt;
  wt.last_message_time = std::chrono::steady_clock::now();

  rclcpp::SubscriptionOptions options;
  options.event_callbacks.matched_callback =
    [this, topic_name](rclcpp::MatchedInfo & info) {
      auto it = watched_.find(topic_name);
      if (it != watched_.end()) {
        it->second.matched = info;
      }
    };
  options.event_callbacks.liveliness_callback =
    [this, topic_name](rclcpp::QOSLivelinessChangedInfo & info) {
      auto it = watched_.find(topic_name);
      if (it != watched_.end()) {
        it->second.liveliness = info;
      }
    };
  options.event_callbacks.incompatible_qos_callback =
    [this, topic_name](rclcpp::QOSRequestedIncompatibleQoSInfo & info) {
      RCLCPP_WARN(
        this->get_logger(),
        "QoS mismatch watching '%s': %d incompatible publisher(s) (policy kind %d) - "
        "this bridge can't observe that publisher's DDS traffic",
        topic_name.c_str(), info.total_count, static_cast<int>(info.last_policy_kind));
    };

  auto callback =
    [this, topic_name](std::shared_ptr<rclcpp::SerializedMessage> message) {
      auto it = watched_.find(topic_name);
      if (it == watched_.end()) {
        return;
      }
      auto & w = it->second;
      const auto now = std::chrono::steady_clock::now();
      w.last_period_ms = std::chrono::duration<double, std::milli>(
        now - w.last_message_time).count();
      w.last_message_time = now;
      w.bytes_in_window += message->size();
      w.samples_in_window += 1;
    };

  // Best-effort, volatile, shallow history: the loosest QoS combination, so
  // this observer can match against (and receive data from) publishers
  // using almost any QoS profile - a Reliable/TransientLocal publisher is
  // compatible with a BestEffort/Volatile reader, just not vice versa.
  const rclcpp::QoS watch_qos = rclcpp::QoS(10).best_effort().durability_volatile();

  wt.subscription = this->create_generic_subscription(
    topic_name, type_name, watch_qos, callback, options);

  RCLCPP_INFO(
    this->get_logger(), "Watching '%s' (%s) for DDS health metrics",
    topic_name.c_str(), type_name.c_str());

  watched_.emplace(topic_name, std::move(wt));
}

void DdsMetricsBridgeNode::on_report_timer()
{
  const auto stamp = this->now();

  for (auto & topic_and_watch : watched_) {
    const std::string & topic_name = topic_and_watch.first;
    WatchedTopic & w = topic_and_watch.second;

    msg::Heartbeat hb;
    hb.stamp = stamp;
    hb.topic_name = topic_name;
    hb.matched_publisher_count = static_cast<uint32_t>(w.matched.current_count);
    hb.matched_count_change = w.matched.current_count_change;
    hb.liveliness_alive_count = w.liveliness.alive_count;
    hb.liveliness_not_alive_count = w.liveliness.not_alive_count;
    heartbeat_pub_->publish(hb);

    msg::LatencyBytes lb;
    lb.stamp = stamp;
    lb.topic_name = topic_name;
    lb.period_mean_ms = w.last_period_ms;
    lb.sample_count_window = w.samples_in_window;
    lb.throughput_bytes_per_sec = static_cast<double>(w.bytes_in_window);
    latency_pub_->publish(lb);

    w.bytes_in_window = 0;
    w.samples_in_window = 0;
  }

  publish_rexmit_counters();
  publish_probe_counters();
  publish_discovery_events();
}

void DdsMetricsBridgeNode::publish_rexmit_counters()
{
  const auto stamp = this->now();
  for (const auto & c : discovery_watcher_->poll_rexmit_counters()) {
    msg::RetransmittedPacket rp;
    rp.stamp = stamp;
    rp.source = c.source;
    rp.rexmit_count = c.rexmit_count;
    rp.rexmit_bytes = c.rexmit_bytes;
    rp.rexmit_lost_count = c.rexmit_lost_count;
    rp.throttle_count = 0;
    rp.discarded_bytes = c.discarded_bytes;
    rexmit_pub_->publish(rp);
  }
}

void DdsMetricsBridgeNode::publish_probe_counters()
{
  const auto stamp = this->now();
  const ProbeCounters c = self_probe_->poll_counters();

  msg::RetransmittedPacket writer_rp;
  writer_rp.stamp = stamp;
  writer_rp.source = "self_probe_writer";
  writer_rp.rexmit_count = c.writer_rexmit_count;
  writer_rp.rexmit_bytes = c.writer_rexmit_bytes;
  writer_rp.rexmit_lost_count = c.writer_rexmit_lost_count;
  writer_rp.throttle_count = c.writer_throttle_count;
  rexmit_pub_->publish(writer_rp);

  msg::RetransmittedPacket reader_rp;
  reader_rp.stamp = stamp;
  reader_rp.source = "self_probe_reader";
  reader_rp.discarded_bytes = c.reader_discarded_bytes;
  rexmit_pub_->publish(reader_rp);
}

void DdsMetricsBridgeNode::publish_discovery_events()
{
  const auto stamp = this->now();
  for (const auto & e : discovery_watcher_->poll_discovery_events()) {
    msg::DiscoveryEvent de;
    de.stamp = stamp;
    de.event_type = e.event_type;
    de.guid = e.guid;
    de.topic_name = e.topic_name;
    de.type_name = e.type_name;
    de.time_to_discover_sec = e.time_to_discover_sec;
    discovery_pub_->publish(de);
  }
}

}  // namespace dds_metrics_bridge
