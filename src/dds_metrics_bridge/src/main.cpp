#include "rclcpp/rclcpp.hpp"
#include "dds_metrics_bridge/dds_metrics_bridge_node.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<dds_metrics_bridge::DdsMetricsBridgeNode>());
  rclcpp::shutdown();
  return 0;
}
