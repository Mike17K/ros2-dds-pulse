#pragma once

#include <cstdint>

#include <dds/dds.h>
#include <dds/ddsc/dds_statistics.h>

namespace dds_metrics_bridge
{

struct ProbeCounters
{
  uint64_t writer_rexmit_count = 0;
  uint64_t writer_rexmit_bytes = 0;
  uint64_t writer_rexmit_lost_count = 0;
  uint64_t writer_throttle_count = 0;
  uint64_t reader_discarded_bytes = 0;
};

/// A tiny, synthetic "canary" writer+reader pair on a dedicated reliable
/// native-Cyclone topic (see idl/Probe.idl), created in the same native
/// participant as DiscoveryStatsWatcher.
///
/// Cyclone's dds_get_statistics() only reports counters for entities the
/// calling process itself owns - there is no supported way to read another
/// process's writer/reader retransmit counters directly. Running a small,
/// continuous probe on the real ROS_DOMAIN_ID gives an honest, synthetic
/// measurement of "how healthy is DDS transport on this domain right now",
/// the same principle as a network ping/heartbeat health probe.
class SelfProbe
{
public:
  explicit SelfProbe(dds_entity_t participant);
  ~SelfProbe();

  SelfProbe(const SelfProbe &) = delete;
  SelfProbe & operator=(const SelfProbe &) = delete;

  /// Writes one probe sample and drains the matching local reader.
  void tick();

  /// Refreshes and returns current writer/reader counters.
  ProbeCounters poll_counters();

private:
  dds_entity_t topic_ = 0;
  dds_entity_t writer_ = 0;
  dds_entity_t reader_ = 0;
  dds_statistics * writer_stats_ = nullptr;
  dds_statistics * reader_stats_ = nullptr;
  uint64_t seq_ = 0;
};

}  // namespace dds_metrics_bridge
