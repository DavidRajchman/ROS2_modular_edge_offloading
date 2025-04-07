import launch
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='robot_localization',
            executable='ekf_node',
            name='ekf_filter_node',
            output='screen',
            parameters=[{'use_sim_time': True}, '/home/ubuntu/ros_ws/src/sim/config/ekf.yaml']
            # parameters=[{'use_sim_time': True}, '/home/ubuntu/ros_ws/src/sim/config/IMUlocalization.yaml'],
            # arguments=['--ros-args', '--log-level', 'DEBUG']
        ),
    ])
