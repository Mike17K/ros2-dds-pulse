# One event per newly-seen DDS participant or endpoint, sourced from the DDS
# built-in discovery topics (DCPSParticipant/DCPSPublication/
# DCPSSubscription). These are part of the DDS specification itself, so -
# unlike per-entity statistics - discovery visibility is genuinely
# cross-process and vendor-neutral.
builtin_interfaces/Time stamp
string event_type      # "participant_discovered" | "publication_discovered" | "subscription_discovered"
string guid
string topic_name      # empty for participant_discovered
string type_name       # empty for participant_discovered
float64 time_to_discover_sec   # wall-clock time since this bridge started
