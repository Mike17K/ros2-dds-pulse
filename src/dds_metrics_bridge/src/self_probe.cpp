#include "dds_metrics_bridge/self_probe.hpp"

#include <chrono>
#include <stdexcept>

#include "Probe.h"

namespace dds_metrics_bridge
{

namespace
{

uint64_t stat_u64(const dds_statistics * stats, const char * name)
{
  const dds_stat_keyvalue * kv = dds_lookup_statistic(stats, name);
  if (!kv) {
    return 0;
  }
  switch (kv->kind) {
    case DDS_STAT_KIND_UINT32:
      return kv->u.u32;
    case DDS_STAT_KIND_UINT64:
      return kv->u.u64;
    case DDS_STAT_KIND_LENGTHTIME:
      return kv->u.lengthtime;
  }
  return 0;
}

}  // namespace

SelfProbe::SelfProbe(dds_entity_t participant)
{
  dds_qos_t * qos = dds_create_qos();
  dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
  dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);

  topic_ = dds_create_topic(
    participant, &dds_metrics_bridge_Probe_desc, "dds_metrics_bridge/probe", qos, nullptr);
  if (topic_ < 0) {
    dds_delete_qos(qos);
    throw std::runtime_error("SelfProbe: dds_create_topic failed");
  }

  writer_ = dds_create_writer(participant, topic_, qos, nullptr);
  reader_ = dds_create_reader(participant, topic_, qos, nullptr);
  dds_delete_qos(qos);

  if (writer_ < 0 || reader_ < 0) {
    throw std::runtime_error("SelfProbe: failed to create probe writer/reader");
  }

  writer_stats_ = dds_create_statistics(writer_);
  reader_stats_ = dds_create_statistics(reader_);
}

SelfProbe::~SelfProbe()
{
  dds_delete_statistics(writer_stats_);
  dds_delete_statistics(reader_stats_);
  // writer_/reader_/topic_ are deleted implicitly when the owning
  // participant (owned by DiscoveryStatsWatcher) is deleted.
}

void SelfProbe::tick()
{
  dds_metrics_bridge_Probe sample;
  sample.seq = seq_++;
  sample.send_time_ns = static_cast<uint64_t>(
    std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
  dds_write(writer_, &sample);

  // Drain our own reader so its history doesn't grow; only its statistics
  // (not the payload) are of interest.
  void * samples[4] = {nullptr};
  dds_sample_info_t infos[4];
  const dds_return_t n = dds_take(reader_, samples, infos, 4, 4);
  if (n > 0) {
    dds_return_loan(reader_, samples, n);
  }
}

ProbeCounters SelfProbe::poll_counters()
{
  ProbeCounters c;
  if (writer_stats_) {
    dds_refresh_statistics(writer_stats_);
    c.writer_rexmit_count = stat_u64(writer_stats_, "rexmit_count");
    c.writer_rexmit_bytes = stat_u64(writer_stats_, "rexmit_bytes");
    c.writer_rexmit_lost_count = stat_u64(writer_stats_, "rexmit_lost_count");
    c.writer_throttle_count = stat_u64(writer_stats_, "throttle_count");
  }
  if (reader_stats_) {
    dds_refresh_statistics(reader_stats_);
    c.reader_discarded_bytes = stat_u64(reader_stats_, "discarded_bytes");
  }
  return c;
}

}  // namespace dds_metrics_bridge
