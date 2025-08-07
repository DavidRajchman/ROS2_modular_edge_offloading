from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    # Declare launch arguments with default values
    discovery_host_arg = DeclareLaunchArgument(
        'discovery_host',
        default_value='192.168.65.10',
        description='Discovery service host address'
    )
    
    discovery_port_arg = DeclareLaunchArgument(
        'discovery_port',
        default_value='9090',
        description='Discovery service port'
    )
    
    component_name_arg = DeclareLaunchArgument(
        'component_name',
        default_value='mec_test_gateway',
        description='MEC component name'
    )
    
    group_id_arg = DeclareLaunchArgument(
        'group_id',
        default_value='70',
        description='MEC group ID'
    )
    
    id_in_group_arg = DeclareLaunchArgument(
        'id_in_group',
        default_value='1',
        description='MEC ID in group'
    )
    
    listen_port_arg = DeclareLaunchArgument(
        'listen_port',
        default_value='7501',
        description='MEC data plane listen port'
    )

    # MGW MEC Gateway Controller Node  
    gateway_controller_node = Node(
        package='modular_gateway_sender',
        executable='mec_gateway',
        name='mec_gateway_controller',
        parameters=[{
            'discovery_service.host': LaunchConfiguration('discovery_host'),
            'discovery_service.port': LaunchConfiguration('discovery_port'),
            'identity.component_type': 'M',
            'identity.component_name': LaunchConfiguration('component_name'),
            'identity.group_id': LaunchConfiguration('group_id'),
            'identity.id_in_group': LaunchConfiguration('id_in_group'),
            'data_plane.listen_port': LaunchConfiguration('listen_port')
        }],
        output='screen'
    )

    # MEC Test Node (echoes back received strings with processing timestamp)
    mec_test_node = Node(
        package='offloading_latency_test_loopback',
        executable='mec_node',
        name='mec_test_node',
        parameters=[{
            'subscribe_topic': 'test/string_input',
            'publish_topic': 'test/string_result'
        }],
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
        mec_test_node
    ])
