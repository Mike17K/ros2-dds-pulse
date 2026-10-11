# Per-watched-topic throughput/period, self-measured by the bridge's own
# generic-subscription callback via rclcpp::SerializedMessage::size() - no
# need to know each topic's concrete message type. This measures *this
# node's* view of the topic (wire throughput and inter-arrival period as
# seen by a well-connected observer), not any other subscriber's experience.
builtin_interfaces/Time stamp
string topic_name
float64 period_mean_ms
float64 throughput_bytes_per_sec
uint32  sample_count_window
