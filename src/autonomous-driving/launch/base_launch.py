import launch
import launch_ros
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():
    launch_lidar = os.path.join(
        get_package_share_directory('sllidar_ros2'),  # Název balíčku s launch souborem
        'launch',
        'sllidar_a1_launch.py'  # Název souboru, který bude spuštěn
    )
    launch_odom = os.path.join(
        get_package_share_directory('rf2o_laser_odometry'),  # Název balíčku s launch souborem
        'launch',
        'rf2o_laser_odometry.launch.py'  # Název souboru, který bude spuštěn
    )
    launch_static_transform = os.path.join(
        get_package_share_directory('services'),  # Název balíčku s launch souborem
        'launch',
        'static_transformation_launch.py'  # Název souboru, který bude spuštěn
    )
    launch_imu = os.path.join(
        get_package_share_directory('wit_ros2_imu'),  # Název balíčku s launch souborem
        'launch',
        'rviz_and_imu.launch.py'  # Název souboru, který bude spuštěn
    )
    launch_slam = os.path.join(
        get_package_share_directory('slam'),  # Název balíčku s launch souborem
        'launch',
        'online_async_launch.py'  # Název souboru, který bude spuštěn
    )
    

    return LaunchDescription([
        IncludeLaunchDescription(PythonLaunchDescriptionSource(launch_lidar)),
        IncludeLaunchDescription(PythonLaunchDescriptionSource(launch_odom)),
        IncludeLaunchDescription(PythonLaunchDescriptionSource(launch_static_transform)),
        IncludeLaunchDescription(PythonLaunchDescriptionSource(launch_slam))
    ])

