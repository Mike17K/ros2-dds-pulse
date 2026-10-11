# Per-watched-topic matched-endpoint snapshot, sourced from this bridge's
# own rclcpp generic subscriptions via event_callbacks.matched_callback -
# vendor-neutral, publicly documented rclcpp/rmw API (rmw_matched_status_t),
# not a Cyclone- or FastDDS-specific hack. "Heartbeat" here reflects DDS
# liveliness/matching activity for real publishers currently in the fleet.
builtin_interfaces/Time stamp
string topic_name
uint32 matched_publisher_count
int32  matched_count_change
int32  liveliness_alive_count
int32  liveliness_not_alive_count
