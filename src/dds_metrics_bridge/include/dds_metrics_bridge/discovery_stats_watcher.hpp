#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include <dds/dds.h>
#include <dds/ddsc/dds_statistics.h>

namespace dds_metrics_bridge
{

struct DiscoveryEventData
{
  std::string event_type;
  std::string guid;
  std::string topic_name;
  std::string type_name;
  double time_to_discover_sec = 0.0;
};

struct RexmitCounters
{
  std::string source;
  uint64_t rexmit_count = 0;
  uint64_t rexmit_bytes = 0;
  uint64_t rexmit_lost_count = 0;
  uint64_t discarded_bytes = 0;
};

/// Owns a dedicated, native Cyclone DDS participant (separate from
/// rmw_cyclonedds_cpp's own participant, but on the same ROS_DOMAIN_ID) that
/// subscribes to the three DDS built-in discovery topics (DCPSParticipant,
/// DCPSPublication, DCPSSubscription).
///
/// These topics are part of the DDS specification itself (vendor-neutral
/// SPDP/SEDP discovery data) - unlike dds_get_statistics(), which Cyclone
/// only exposes for entities a process itself holds a handle to (see
/// dds/ddsc/dds_statistics.h), discovery visibility is genuinely
/// cross-process. And because SPDP/SEDP discovery traffic flows over the
/// full reliable RTPS protocol (HEARTBEAT/ACKNACK/retransmit), the
/// dds_get_statistics() counters on *these* built-in readers give honest,
/// fleet-wide discovery-layer retransmit/discard numbers - exactly the
/// "multi-robot discovery noise" problem this workspace's own docs cite
/// when justifying Cyclone DDS over Fast DDS.
class DiscoveryStatsWatcher
{
public:
  DiscoveryStatsWatcher();
  ~DiscoveryStatsWatcher();

  DiscoveryStatsWatcher(const DiscoveryStatsWatcher &) = delete;
  DiscoveryStatsWatcher & operator=(const DiscoveryStatsWatcher &) = delete;

  /// The native participant backing this watcher, shared with SelfProbe.
  dds_entity_t participant() const {return participant_;}

  /// Drains newly-seen participants/endpoints since the last call.
  std::vector<DiscoveryEventData> poll_discovery_events();

  /// Refreshes and returns current rexmit/discard counters for all three
  /// built-in discovery readers.
  std::vector<RexmitCounters> poll_rexmit_counters();

private:
  struct BuiltinReader
  {
    std::string source;
    dds_entity_t reader = 0;
    dds_statistics * stats = nullptr;
  };

  BuiltinReader make_reader(dds_entity_t builtin_topic, const char * source);
  void take_participant_events(std::vector<DiscoveryEventData> & out);
  void take_endpoint_events(
    BuiltinReader & reader, const std::string & event_type,
    std::vector<DiscoveryEventData> & out);

  dds_entity_t participant_ = 0;
  BuiltinReader participant_reader_;
  BuiltinReader publication_reader_;
  BuiltinReader subscription_reader_;

  std::unordered_set<std::string> seen_guids_;
  std::chrono::steady_clock::time_point start_time_;
};

}  // namespace dds_metrics_bridge
