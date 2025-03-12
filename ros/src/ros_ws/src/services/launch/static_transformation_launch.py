from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        # První uzel
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='static_transform_lidar_base',
            output='screen',
            arguments=['0.07', '0', '0.1', '0', '0', '0', '1', 'base_footprint', 'laser'],
        ),
        # Druhý uzel
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='static_transform_camera_base',
            output='screen',
            arguments=['0.43', '0', '0.02', '0', '0', '0', '1', 'base_footprint', 'camera'],
        ),
        # Třetí uzel
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='static_transform_imu_base',
            output='screen',
            arguments=['-0.07', '0', '-0.02', '1', '0', '0', '0.0000463', 'base_footprint', 'imu_link'],
        ),
        # Node(
        #     package='tf2_ros',
        #     executable='static_transform_publisher',
        #     name='static_transform_base_odom',
        #     output='screen',
        #     arguments=['0', '0', '0', '0', '0', '0', '1', 'odom','base_footprint'],
        # ),
    ])
