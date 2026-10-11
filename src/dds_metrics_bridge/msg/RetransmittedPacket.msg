# RTPS-level retransmit/discard counters, sourced from Cyclone DDS's
# dds_get_statistics() API (dds/ddsc/dds_statistics.h: "a quick-and-dirty
# provisional interface"). This API only reports counters for entities this
# process itself owns - there is no cross-process call. "source" identifies
# which local entity a given sample comes from:
#
#   self_probe_writer / self_probe_reader - a tiny synthetic canary pub/sub
#       pair this node creates on its own reliable topic (see self_probe.hpp),
#       used as an always-on health probe for DDS transport on this
#       ROS_DOMAIN_ID. Induce real packet loss to see it react, e.g.:
#         sudo tc qdisc add dev lo root netem loss 2% delay 50ms 10ms
#
#   DCPSParticipant / DCPSPublication / DCPSSubscription - this node's own
#       readers of the DDS built-in discovery topics (SPDP/SEDP). These are
#       part of the DDS spec itself, so they genuinely reflect fleet-wide
#       discovery traffic, not just this node's own data.
#
# Fields that don't apply to a given source's entity kind (e.g. rexmit_*
# fields for a reader) are left at 0.
builtin_interfaces/Time stamp
string source
uint64 rexmit_count
uint64 rexmit_bytes
uint64 rexmit_lost_count
uint64 throttle_count
uint64 discarded_bytes
