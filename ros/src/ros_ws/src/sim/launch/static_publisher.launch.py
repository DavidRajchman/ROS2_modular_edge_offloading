from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    static_tf_pub = Node(
        package="tf2_ros",
        executable="static_transform_publisher",
        arguments=["0", "0", "0", "0", "0", "0", "vehicle_blue/chassis", "vehicle_blue/laser_frame/gpu_lidar"],
        output="screen"
    )

    return LaunchDescription([
        static_tf_pub
    ])
