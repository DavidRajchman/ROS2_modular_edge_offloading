import launch
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, LogInfo
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.descriptions import Parameter
from launch.launch_description_sources import PythonLaunchDescriptionSource
from ament_index_python.packages import get_package_share_directory





def generate_launch_description():
    # Zahrnutí jiného launch souboru
    # other_launch_file = LaunchConfiguration('ros_gz_sim', default='gz_sim.launch.py')
    ros_gz_sim_launch_file = get_package_share_directory('ros_gz_sim') + '/launch/gz_sim.launch.py'

    return LaunchDescription([
        # Zahrnutí jiného launch souboru
        launch.actions.IncludeLaunchDescription(
            # PythonLaunchDescriptionSource([LaunchConfiguration('ros_gz_sim'), '/', other_launch_file])
            PythonLaunchDescriptionSource(ros_gz_sim_launch_file)
        ),
        # Další akce pro tento soubor
        LogInfo(msg="Main launch file is running 3 ."),
        
        Node(
            package='demo_nodes_cpp',
            namespace='demo_nodes_cpp',
            executable='talker',
            name='sim_talker'
        )
        
    ])
