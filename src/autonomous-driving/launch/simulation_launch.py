import launch
from launch import LaunchDescription
from launch.actions import LogInfo
from launch_ros.actions import Node
from launch.actions import IncludeLaunchDescription
from launch.substitutions import LaunchConfiguration
from launch_ros.substitutions import FindPackageShare
from launch import LaunchContext
from launch.actions import DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource

def generate_launch_description():
    # Cesta k turtlebot3_world.launch.py
    # gazebo_launch_file = FindPackageShare('turtlebot3_gazebo').find('launch') + '/turtlebot3_world.launch.py'
    gazebo_launch_file = FindPackageShare('turtlebot3_gazebo').find('turtlebot3_gazebo')+'/launch/turtlebot3_world.launch.py'
    slam_launch_file = FindPackageShare('slam_toolbox').find('slam_toolbox')+'/launch/online_async_launch.py'

    print(slam_launch_file)

    return LaunchDescription([
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(gazebo_launch_file)
        ),

        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(slam_launch_file)
        ),

        Node(
            package='rviz2',
            executable='rviz2',
            name='rviz2',
            output='screen',
        ),

    # Spustí teleop uzel (ovládání robota pomocí klávesnice)
    # Node(
    #     package='turtlebot3_teleop',
    #     executable='teleop_keyboard',
    #     name='teleop_keyboard',
    #     output='screen',
    #     # prefix='xterm -e',
    # ),

    ])

    # return LaunchDescription([
    #     # Spustí turtlebot3 world
    #     IncludeLaunchDescription(
    #         PythonLaunchDescriptionSource(gazebo_launch_file)
    #     ),
        
    #     # Spustí teleop uzel (ovládání robota pomocí klávesnice)
    #     Node(
    #         package='turtlebot3_teleop',
    #         executable='teleop_keyboard',
    #         name='teleop_keyboard',
    #         output='screen',
    #     ),
        
    #     # Spustí rviz2 pro vizualizaci
    #     Node(
    #         package='rviz2',
    #         executable='rviz2',
    #         name='rviz2',
    #         output='screen',
    #     ),
    # ])