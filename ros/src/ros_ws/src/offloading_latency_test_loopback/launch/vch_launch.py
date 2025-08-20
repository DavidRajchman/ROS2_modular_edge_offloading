from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    # Declare launch arguments with default values
    discovery_host_arg = DeclareLaunchArgument(
        'discovery_host',
        default_value='192.168.50.114',
        description='Discovery service host address'
    )
    
    discovery_port_arg = DeclareLaunchArgument(
        'discovery_port',
        default_value='9090',
        description='Discovery service port'
    )
    
    component_name_arg = DeclareLaunchArgument(
        'component_name',
        default_value='vhc_test_gateway',
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

    # MGW VHC Gateway Controller Node
    gateway_controller_node = Node(
        package='modular_gateway_sender',
        executable='gateway_controller',
        name='gateway_controller',
        parameters=[{
            'discovery_service.host': LaunchConfiguration('discovery_host'),
            'discovery_service.port': LaunchConfiguration('discovery_port'),
            'identity.component_type': 'V',
            'identity.component_name': LaunchConfiguration('component_name'),
            'identity.group_id': LaunchConfiguration('group_id'),
            'identity.id_in_group': LaunchConfiguration('id_in_group'),
            'data_plane.listen_port': LaunchConfiguration('listen_port'),
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
        output='screen'
    )

    return LaunchDescription([
        discovery_host_arg,
        discovery_port_arg,
        component_name_arg,
        group_id_arg,
        id_in_group_arg,
        listen_port_arg,
        gateway_controller_node,
        vhc_test_node
    ])
