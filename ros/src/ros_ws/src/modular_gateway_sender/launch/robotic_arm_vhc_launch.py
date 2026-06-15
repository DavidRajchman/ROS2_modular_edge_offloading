from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument, ExecuteProcess
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch.conditions import IfCondition, UnlessCondition
import os
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    pkg_dir = get_package_share_directory('modular_gateway_sender')
    default_config = os.path.join(pkg_dir, 'config', 'robotic_arm_config.json')
    
    # Dynamically resolve workspace root and script path
    ws_dir = os.path.abspath(os.path.join(pkg_dir, '../../../../'))

    return LaunchDescription([
        # --- Mode Selection ---
        # Use 'p2p' for static peer address (no discovery service required)
        # Use 'p2p_ds' to resolve the MEC IP via the discovery service
        DeclareLaunchArgument(
            'operation_mode',
            default_value='p2p',
            description="Connection mode: 'p2p' (static IP) or 'p2p_ds' (discovery-assisted)"
        ),
        
        DeclareLaunchArgument(
            'teleop',
            default_value='false',
            description='Launch custom python serial bridge instead of default serial_ctrl'
        ),
        DeclareLaunchArgument(
            'IK_MEC',
            default_value='false',
            description='Offload IK computation to MEC (requires teleop:=true)'
        ),

        # --- p2p mode args (required when operation_mode:=p2p) ---
        DeclareLaunchArgument(
            'peer_host',
            default_value='12.1.1.67',
            description="[p2p] Static IP of the MEC node"
        ),
        DeclareLaunchArgument(
            'peer_port',
            default_value='7401',
            description="[p2p] Data plane port of the MEC node"
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

        # --- Common Identity Args ---
        DeclareLaunchArgument('group_id', default_value='60'),
        DeclareLaunchArgument('id_in_group', default_value='1'),
        DeclareLaunchArgument('local_config_path', default_value=default_config),

        Node(
            package='modular_gateway_sender',
            executable='gateway_controller',
            name='robotic_arm_vhc_gateway',
            namespace='roarm',
            output='screen',
            parameters=[{
                # Identity
                'identity.component_type': 'V',
                'identity.component_name': 'roarm_vhc',
                'identity.group_id': LaunchConfiguration('group_id'),
                'identity.id_in_group': LaunchConfiguration('id_in_group'),

                # Mode and config
                'operation_mode': LaunchConfiguration('operation_mode'),
                'p2p.local_config_path': LaunchConfiguration('local_config_path'),

                # p2p static peer (used when operation_mode=p2p)
                'p2p.peer_host': LaunchConfiguration('peer_host'),
                'p2p.peer_port': LaunchConfiguration('peer_port'),

                # Discovery service (used when operation_mode=p2p_ds)
                'discovery_service.host': LaunchConfiguration('discovery_host'),
                'discovery_service.port': LaunchConfiguration('discovery_port'),

                # Topic overrides for robotic arm hardware (using absolute paths)
                'serial_feedback_handler.topic': '/serial_ctrl/rx',
                'serial_string_handler.topic': '/serial_ctrl/tx',
                'joint_state_handler.topic': '/joint_states',
            }],
        )
    ])
