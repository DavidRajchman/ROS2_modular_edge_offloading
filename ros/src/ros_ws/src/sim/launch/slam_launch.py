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
    slam_launch_file = get_package_share_directory('slam_toolbox') + '/launch/online_async_launch2.py'
    slam = launch.actions.IncludeLaunchDescription(
            # PythonLaunchDescriptionSource([LaunchConfiguration('ros_gz_sim'), '/', other_launch_file])
            PythonLaunchDescriptionSource(slam_launch_file),
            launch_arguments={'config_file': '/home/ubuntu/ros_ws/src/sim/config/online_async.yaml'}.items()
        )

    return LaunchDescription([
        slam
    ])
