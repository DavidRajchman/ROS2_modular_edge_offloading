# launch/vch_simple_launch.py
# (e.g., place in offloading_latency_test_loopback/launch/)

from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='modular_gateway_sender',
            executable='gateway_VHC', # Executable from gateway_VHC_main.cpp
            name='vhc_modular_gateway',
            output='screen'
            # No parameters set here, node will use its defaults
        ),
        Node(
            package='offloading_latency_test_loopback',
            executable='vhc_node',
            name='vhc_latency_tester',
            output='screen'
            # No parameters set here, node will use its defaults
        )
    ])