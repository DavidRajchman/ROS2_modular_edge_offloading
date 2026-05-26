from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument, ExecuteProcess
from launch.substitutions import LaunchConfiguration
from launch.conditions import IfCondition, UnlessCondition
import os
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    pkg_dir = get_package_share_directory('modular_gateway_sender')
    default_config = os.path.join(pkg_dir, 'config', 'robotic_arm_config.json')

    return LaunchDescription([
        # --- Mode Selection ---
        # Use 'p2p' for static peer address (no discovery service required)
        # Use 'p2p_ds' to register with the discovery service (VHC finds this MEC via DS)
        DeclareLaunchArgument(
            'operation_mode',
            default_value='p2p',
            description="Connection mode: 'p2p' (static, MEC listens) or 'p2p_ds' (registers with discovery service)"
        ),
        
        DeclareLaunchArgument(
            'teleop',
            default_value='false',
            description='Launch custom python teleop script instead of default roarm GUI'
        ),

        # --- p2p_ds mode args (required when operation_mode:=p2p_ds) ---
        DeclareLaunchArgument(
            'discovery_host',
            default_value='192.168.1.1',
            description="[p2p_ds] Discovery service IP address"
        ),
        DeclareLaunchArgument(
            'discovery_port',
            default_value='9090',
            description="[p2p_ds] Discovery service port"
        ),

        # --- Common Args ---
        DeclareLaunchArgument('group_id', default_value='61'),
        DeclareLaunchArgument('id_in_group', default_value='1'),
        DeclareLaunchArgument('listen_port', default_value='7401'),
        DeclareLaunchArgument('local_config_path', default_value=default_config),

        Node(
            package='modular_gateway_sender',
            executable='mec_gateway',
            name='robotic_arm_mec_gateway',
            namespace='roarm',
            output='screen',
            parameters=[{
                # Identity
                'identity.component_type': 'M',
                'identity.component_name': 'roarm_mec',
                'identity.group_id': LaunchConfiguration('group_id'),
                'identity.id_in_group': LaunchConfiguration('id_in_group'),

                # Mode and config
                'operation_mode': LaunchConfiguration('operation_mode'),
                'p2p.local_config_path': LaunchConfiguration('local_config_path'),
                'data_plane.listen_port': LaunchConfiguration('listen_port'),

                # Discovery service (used when operation_mode=p2p_ds)
                'discovery_service.host': LaunchConfiguration('discovery_host'),
                'discovery_service.port': LaunchConfiguration('discovery_port'),

                # Topic overrides for robotic arm compute side (using absolute paths)
                'serial_feedback_handler.topic': '/serial_ctrl/rx',
                'serial_string_handler.topic': '/serial_ctrl/tx',
                'joint_state_handler.topic': '/joint_states',
            }],
        ),

        ExecuteProcess(
            condition=IfCondition(LaunchConfiguration('teleop')),
            cmd=['python3', '/home/ubuntu/ros_ws/roarm_control/roarm_keyboard_teleop.py'],
            output='screen'
        ),

        ExecuteProcess(
            condition=UnlessCondition(LaunchConfiguration('teleop')),
            cmd=['ros2', 'launch', 'roarm', 'roarm.launch.py', 'gui:=True'],
            output='screen'
        )
    ])
