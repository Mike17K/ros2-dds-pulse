cmake_minimum_required(VERSION 3.8)
project(dds_metrics_bridge)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(CycloneDDS REQUIRED)   # same CMake config rmw_cyclonedds_cpp itself uses

# --- ROS 2 messages published by the bridge -------------------------------
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/Heartbeat.msg"
  "msg/RetransmittedPacket.msg"
  "msg/LatencyBytes.msg"
  "msg/DiscoveryEvent.msg"
  DEPENDENCIES builtin_interfaces
)
rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

# --- Native Cyclone DDS type for the SelfProbe canary topic ---------------
# Compiled straight from IDL with Cyclone's own idlc, independent of ROS 2's
# message pipeline - see idl/Probe.idl and include/dds_metrics_bridge/self_probe.hpp.
idlc_generate(TARGET probe_idl FILES idl/Probe.idl)

add_executable(dds_metrics_bridge_node
  src/main.cpp
  src/dds_metrics_bridge_node.cpp
  src/discovery_stats_watcher.cpp
  src/self_probe.cpp
)
target_include_directories(dds_metrics_bridge_node PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
ament_target_dependencies(dds_metrics_bridge_node
  rclcpp
  builtin_interfaces
)
target_link_libraries(dds_metrics_bridge_node
  "${cpp_typesupport_target}"
  CycloneDDS::ddsc
  probe_idl
)

install(TARGETS dds_metrics_bridge_node
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY launch/
  DESTINATION share/${PROJECT_NAME}/launch
)

# Ship the Foxglove dashboard layout with the package, so `ros2 pkg prefix
# dds_metrics_bridge`/share always has the layout that matches the topics
# this exact build of the node publishes.
install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/../../foxglove_layouts/dds_pulse.json
  DESTINATION share/${PROJECT_NAME}/foxglove_layouts
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
