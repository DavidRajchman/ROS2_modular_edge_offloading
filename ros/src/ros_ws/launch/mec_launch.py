# launch/mec_simple_launch.py
# (e.g., place in offloading_latency_test_loopback/launch/)

from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='modular_gateway_sender',
            executable='gateway_MEC', # Executable from gateway_client_main.cpp
            name='mec_modular_gateway',
            output='screen'
            # No parameters set here, node will use its defaults
            # IMPORTANT: The 'server_host' for the gateway_client will default to
            # '127.0.0.1' as per its C++ code. This will only work if the VHC gateway
            # is also running on the same machine as the MEC gateway.
            # If they are on different machines, you MUST set the 'server_host'
            # parameter, either here, via command line, or by changing the default
            # in the C++ code of gateway_client.
        ),
        Node(
            package='offloading_latency_test_loopback',
            executable='mec_processing_node',
            name='mec_latency_processor',
            output='screen'
            # No parameters set here, node will use its defaults
        )
    ])