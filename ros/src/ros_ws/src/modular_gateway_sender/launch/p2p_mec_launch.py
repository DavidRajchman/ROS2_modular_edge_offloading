from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
import os
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    pkg_dir = get_package_share_directory('modular_gateway_sender')
    default_config = os.path.join(pkg_dir, 'config', 'p2p_config.json')

    return LaunchDescription([
        DeclareLaunchArgument('discovery_host', default_value='127.0.0.1'),
        DeclareLaunchArgument('discovery_port', default_value='9090'),
        DeclareLaunchArgument('operation_mode', default_value='p2p_ds'),
        DeclareLaunchArgument('group_id', default_value='60'),
        DeclareLaunchArgument('id_in_group', default_value='5'),
        DeclareLaunchArgument('local_config_path', default_value=default_config),
        DeclareLaunchArgument('listen_port', default_value='7402'),

        Node(
            package='modular_gateway_sender',
            executable='gateway_controller',
            name='mec_gateway_controller',
            namespace='mec',
            parameters=[{
                'discovery_service.host': LaunchConfiguration('discovery_host'),
                'discovery_service.port': LaunchConfiguration('discovery_port'),
                'identity.component_type': 'M',
                'identity.component_name': 'mec_p2p_gateway',
                'identity.group_id': LaunchConfiguration('group_id'),
                'identity.id_in_group': LaunchConfiguration('id_in_group'),
                'operation.mode': LaunchConfiguration('operation_mode'),
                'operation.local_config_path': LaunchConfiguration('local_config_path'),
                'data_plane.listen_port': LaunchConfiguration('listen_port')
            }],
            output='screen'
        )
    ])
