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
    discovery_host_arg = DeclareLaunchArgument(
        'discovery_host',
        default_value='192.168.65.5',
        description='Discovery service host address'
    )
    
    discovery_port_arg = DeclareLaunchArgument(
        'discovery_port',
        default_value='9090',
        description='Discovery service port'
    )
    
    component_name_arg = DeclareLaunchArgument(
        'component_name',
        default_value='mec_p2p_ds_gateway',
        description='MEC component name'
    )
    
    group_id_arg = DeclareLaunchArgument(
        'group_id',
        default_value='60',
        description='MEC group ID (must match VHC)'
    )
    
    id_in_group_arg = DeclareLaunchArgument(
        'id_in_group',
        default_value='5',
        description='MEC ID in group (must match VHC)'
    )
    
    listen_port_arg = DeclareLaunchArgument(
        'listen_port',
        default_value='8401',
        description='MEC data plane listen port'
    )

    config_path_arg = DeclareLaunchArgument(
        'config_path',
        default_value=default_config_path,
        description='Path to the P2P configuration JSON file'
    )

    # MGW MEC Gateway Controller Node
    gateway_controller_node = Node(
        package='modular_gateway_sender',
        executable='mec_gateway',
        name='gateway_controller',
        namespace='mec',
        parameters=[{
            'discovery_service.host': LaunchConfiguration('discovery_host'),
            'discovery_service.port': LaunchConfiguration('discovery_port'),
            'identity.component_type': 'M',
            'identity.component_name': LaunchConfiguration('component_name'),
            'identity.group_id': LaunchConfiguration('group_id'),
            'identity.id_in_group': LaunchConfiguration('id_in_group'),
            'data_plane.listen_port': LaunchConfiguration('listen_port'),
            'operation.mode': 'p2p_ds',
            'operation.local_config_path': LaunchConfiguration('config_path'),
            # Override handler topics to align with test app
            'string_test_input_handler.topic': 'test_input_topic',
            'string_test_result_handler.topic': 'test_result_topic'
        }],
        output='screen'
    )

    # MEC Test Node
    mec_test_node = Node(
        package='offloading_latency_test_loopback',
        executable='mec_processing_node',
        name='mec_test_node',
        namespace='mec',
        output='screen'
    )

    return LaunchDescription([
        discovery_host_arg,
        discovery_port_arg,
        component_name_arg,
        group_id_arg,
        id_in_group_arg,
        listen_port_arg,
        config_path_arg,
        gateway_controller_node,
        mec_test_node
    ])
