"""Starts the DDS metrics bridge and a Foxglove WebSocket bridge together,
so `ros2 launch dds_metrics_bridge dds_metrics_bridge.launch.py` is enough
to get live /dds/* topics into Foxglove Studio at ws://localhost:8765.

The bridge rescans ros2 topic list every few seconds (see
'rescan_period_sec'), so launch order relative to the rest of your ROS 2
graph doesn't matter - it just won't report anything on /dds/heartbeats or
/dds/latency_bytes until at least one watched topic exists.
"""
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    foxglove_port = LaunchConfiguration("foxglove_port")

    return LaunchDescription([
        DeclareLaunchArgument(
            "foxglove_port", default_value="8765",
            description="Port foxglove_bridge listens on for Foxglove Studio connections.",
        ),
        Node(
            package="dds_metrics_bridge",
            executable="dds_metrics_bridge_node",
            name="dds_metrics_bridge",
            output="screen",
        ),
        Node(
            package="foxglove_bridge",
            executable="foxglove_bridge",
            name="foxglove_bridge",
            output="screen",
            parameters=[{
                "port": foxglove_port,
                # Keep bandwidth low by default: only the /dds/* metrics and
                # /tf are sent until a panel in Foxglove actually subscribes
                # to something else (foxglove_bridge is subscribe-on-demand
                # per client regardless, but this also hides irrelevant
                # topics from the topic list entirely).
                "topic_whitelist": [".*dds.*", ".*tf.*"],
            }],
        ),
    ])
