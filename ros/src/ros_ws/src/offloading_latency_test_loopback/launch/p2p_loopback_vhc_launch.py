from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():
    # Use the p2p_config.json from modular_gateway_sender config directory
    mgw_share_dir = get_package_share_directory('modular_gateway_sender')
    default_config_path = os.path.join(mgw_share_dir, 'config', 'p2p_config.json')

    # Declare launch arguments
    component_name_arg = DeclareLaunchArgument(
        'component_name',
        default_value='vhc_p2p_gateway',
        description='VHC component name'
    )
    
    group_id_arg = DeclareLaunchArgument(
        'group_id',
        default_value='60',
        description='VHC group ID'
    )
    
    id_in_group_arg = DeclareLaunchArgument(
        'id_in_group',
        default_value='5',
        description='VHC ID in group'
    )

    listen_port_arg = DeclareLaunchArgument(
        'listen_port',
        default_value='7401',
        description='VHC data plane listen port'
    )

    peer_host_arg = DeclareLaunchArgument(
        'peer_host',
        default_value='192.168.65.102',
        description='Target MEC IP address'
    )

    peer_port_arg = DeclareLaunchArgument(
        'peer_port',
        default_value='8401',
        description='Target MEC data plane port'
    )

    config_path_arg = DeclareLaunchArgument(
        'config_path',
        default_value=default_config_path,
        description='Path to the P2P configuration JSON file'
    )

    operation_mode_arg = DeclareLaunchArgument(
        'operation_mode',
        default_value='p2p_ds',
        description='Operation mode: p2p or p2p_ds'
    )

    # MGW VHC Gateway Controller Node
    gateway_controller_node = Node(
        package='modular_gateway_sender',
        executable='gateway_controller',
        name='gateway_controller',
        namespace='vhc',
        parameters=[{
            'identity.component_type': 'V',
            'identity.component_name': LaunchConfiguration('component_name'),
            'identity.group_id': LaunchConfiguration('group_id'),
            'identity.id_in_group': LaunchConfiguration('id_in_group'),
            'data_plane.listen_port': LaunchConfiguration('listen_port'),
            'operation_mode': LaunchConfiguration('operation_mode'),
            'p2p.local_config_path': LaunchConfiguration('config_path'),
            'discovery_service.host': '192.168.65.5',
            'discovery_service.port': 9090,
            'p2p.peer_host': LaunchConfiguration('peer_host'),
            'p2p.peer_port': LaunchConfiguration('peer_port'),
            # Override handler topics to align with test app
            'string_test_input_handler.topic': 'test_input_topic',
            'string_test_result_handler.topic': 'test_result_topic'
        }],
        output='screen'
    )

    # VHC Test Node
    vhc_test_node = Node(
        package='offloading_latency_test_loopback',
        executable='vhc_node',
        name='vhc_test_node',
        namespace='vhc',
        output='screen'
    )

    return LaunchDescription([
        component_name_arg,
        group_id_arg,
        id_in_group_arg,
        listen_port_arg,
        peer_host_arg,
        peer_port_arg,
        config_path_arg,
        operation_mode_arg,
        gateway_controller_node,
        vhc_test_node
    ])
