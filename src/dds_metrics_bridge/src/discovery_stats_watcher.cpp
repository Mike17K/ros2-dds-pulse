#include "dds_metrics_bridge/discovery_stats_watcher.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace dds_metrics_bridge
{

namespace
{

std::string guid_to_hex(const dds_guid_t & guid)
{
  char buf[33];
  for (int i = 0; i < 16; ++i) {
    std::snprintf(buf + i * 2, 3, "%02x", guid.v[i]);
  }
  return std::string(buf, 32);
}

dds_domainid_t resolve_domain_id()
{
  const char * env = std::getenv("ROS_DOMAIN_ID");
  return env ? static_cast<dds_domainid_t>(std::atoi(env)) : 0;
}

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

DiscoveryStatsWatcher::DiscoveryStatsWatcher()
: start_time_(std::chrono::steady_clock::now())
{
  participant_ = dds_create_participant(resolve_domain_id(), nullptr, nullptr);
  if (participant_ < 0) {
    throw std::runtime_error("DiscoveryStatsWatcher: dds_create_participant failed");
  }

  participant_reader_ = make_reader(DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, "DCPSParticipant");
  publication_reader_ = make_reader(DDS_BUILTIN_TOPIC_DCPSPUBLICATION, "DCPSPublication");
  subscription_reader_ = make_reader(DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, "DCPSSubscription");
}

DiscoveryStatsWatcher::BuiltinReader DiscoveryStatsWatcher::make_reader(
  dds_entity_t builtin_topic, const char * source)
{
  BuiltinReader br;
  br.source = source;
  br.reader = dds_create_reader(participant_, builtin_topic, nullptr, nullptr);
  if (br.reader < 0) {
    throw std::runtime_error(
      std::string("DiscoveryStatsWatcher: failed to create reader for ") + source);
  }
  br.stats = dds_create_statistics(br.reader);
  return br;
}

DiscoveryStatsWatcher::~DiscoveryStatsWatcher()
{
  dds_delete_statistics(participant_reader_.stats);
  dds_delete_statistics(publication_reader_.stats);
  dds_delete_statistics(subscription_reader_.stats);
  if (participant_ > 0) {
    dds_delete(participant_);
  }
}

void DiscoveryStatsWatcher::take_participant_events(std::vector<DiscoveryEventData> & out)
{
  constexpr uint32_t kMax = 32;
  void * samples[kMax] = {nullptr};
  dds_sample_info_t infos[kMax];

  const dds_return_t n = dds_take(participant_reader_.reader, samples, infos, kMax, kMax);
  if (n <= 0) {
    return;
  }

  const double elapsed =
    std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time_).count();

  for (dds_return_t i = 0; i < n; ++i) {
    if (!infos[i].valid_data) {
      continue;
    }
    const auto * sample = static_cast<const dds_builtintopic_participant_t *>(samples[i]);
    const std::string guid = guid_to_hex(sample->key);
    if (seen_guids_.insert(guid).second) {
      out.push_back({"participant_discovered", guid, "", "", elapsed});
    }
  }
  dds_return_loan(participant_reader_.reader, samples, n);
}

void DiscoveryStatsWatcher::take_endpoint_events(
  BuiltinReader & reader, const std::string & event_type,
  std::vector<DiscoveryEventData> & out)
{
  constexpr uint32_t kMax = 32;
  void * samples[kMax] = {nullptr};
  dds_sample_info_t infos[kMax];

  const dds_return_t n = dds_take(reader.reader, samples, infos, kMax, kMax);
  if (n <= 0) {
    return;
  }

  const double elapsed =
    std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time_).count();

  for (dds_return_t i = 0; i < n; ++i) {
    if (!infos[i].valid_data) {
      continue;
    }
    const auto * sample = static_cast<const dds_builtintopic_endpoint_t *>(samples[i]);
    const std::string guid = guid_to_hex(sample->key);
    if (seen_guids_.insert(guid).second) {
      out.push_back({
        event_type,
        guid,
        sample->topic_name ? sample->topic_name : "",
        sample->type_name ? sample->type_name : "",
        elapsed});
    }
  }
  dds_return_loan(reader.reader, samples, n);
}

std::vector<DiscoveryEventData> DiscoveryStatsWatcher::poll_discovery_events()
{
  std::vector<DiscoveryEventData> out;
  take_participant_events(out);
  take_endpoint_events(publication_reader_, "publication_discovered", out);
  take_endpoint_events(subscription_reader_, "subscription_discovered", out);
  return out;
}

std::vector<RexmitCounters> DiscoveryStatsWatcher::poll_rexmit_counters()
{
  std::vector<RexmitCounters> out;
  for (BuiltinReader * reader :
    {&participant_reader_, &publication_reader_, &subscription_reader_})
  {
    if (!reader->stats) {
      continue;
    }
    dds_refresh_statistics(reader->stats);
    RexmitCounters c;
    c.source = reader->source;
    c.rexmit_count = stat_u64(reader->stats, "rexmit_count");
    c.rexmit_bytes = stat_u64(reader->stats, "rexmit_bytes");
    c.rexmit_lost_count = stat_u64(reader->stats, "rexmit_lost_count");
    c.discarded_bytes = stat_u64(reader->stats, "discarded_bytes");
    out.push_back(c);
  }
  return out;
}

}  // namespace dds_metrics_bridge
