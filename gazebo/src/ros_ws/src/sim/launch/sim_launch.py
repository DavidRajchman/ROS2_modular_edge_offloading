import launch
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, LogInfo
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.descriptions import Parameter
from launch.launch_description_sources import PythonLaunchDescriptionSource
from ament_index_python.packages import get_package_share_directory
from launch.actions import TimerAction





def generate_launch_description():
    # Zahrnutí jiného launch souboru
    # other_launch_file = LaunchConfiguration('ros_gz_sim', default='gz_sim.launch.py')
    ros_gz_sim_launch_file = get_package_share_directory('ros_gz_sim') + '/launch/gz_sim.launch.py'
    gazebo_sim = launch.actions.IncludeLaunchDescription(
            # PythonLaunchDescriptionSource([LaunchConfiguration('ros_gz_sim'), '/', other_launch_file])
            PythonLaunchDescriptionSource(ros_gz_sim_launch_file),
            launch_arguments={'gz_args': '/home/ubuntu/ros_ws/src/sim/description/robot.sdf'}.items()
        )
    gazebo_bridge = Node(
            package='demo_nodes_cpp',
            namespace='demo_nodes_cpp',
            executable='talker',
            name='sim_talker'
        )
    gazebo_bridge_clock = TimerAction(
        period=5.0,  # Zpoždění v sekundách
        actions=[
            Node(
                package='ros_gz_bridge',
                namespace='ros_gz_bridge',
                executable='parameter_bridge',
                name='gz_bridge_clock',
                arguments=[ '/clock@rosgraph_msgs/msg/Clock@gz.msgs.Clock']
            )
        ]
    )

    return LaunchDescription([
        gazebo_sim,
        gazebo_bridge_clock
        # gazebo_bridge

        
    ])
