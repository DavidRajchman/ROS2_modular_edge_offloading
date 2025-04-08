import launch
from launch.actions import IncludeLaunchDescription
from launch_ros.actions import PushRosNamespace
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():

    # lidar 
    lidar_path = get_package_share_directory('sllidar_ros2') + '/launch/sllidar_a1_launch.py'
    lidar_launch = launch.actions.IncludeLaunchDescription(
        PythonLaunchDescriptionSource(lidar_path)
    )

    # IMU
    imu_path = get_package_share_directory('wit_ros2_imu') + '/rviz_and_imu.launch.py'
    imu_launch = launch.actions.IncludeLaunchDescription(
        PythonLaunchDescriptionSource(imu_path)
    )

    # lidar to odom
    lidar_odom_path = get_package_share_directory('rf2o_laser_odometry') + '/launch/rf2o_laser_odometry.launch.py'
    lidar_odom_launch = launch.actions.IncludeLaunchDescription(
        PythonLaunchDescriptionSource(lidar_odom_path)
    )

    # teensy
    teensy_node = Node(
        package='motor',
        executable='motor',
        name='motor',
        output='screen',
    )

    #robot localization
    robot_localization_node = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        output='screen',
        parameters=[{'use_sim_time': True}, '/home/ubuntu/ros_ws/src/sim/config/ekf.yaml']
    )

    ### static transforms ###
    #imu
    imu_tf_node = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='static_tf2_imu',
        arguments=[
            '0.0', '0.0', '0.0',          # x y z
            '3.14', '0.0', '0.0',          # roll pitch yaw (v radiánech)
            'base_footprint', 'imu_link'   # parent_frame, child_frame
        ],
        output='screen'
    )
    #lidar
    lidar_tf_node = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='static_tf2_lidar',
        arguments=[
            '0.1', '0.0', '0.15',          # x y z
            '0.0', '0.0', '0.0',          # roll pitch yaw (v radiánech)
            'base_footprint', 'laser'   # parent_frame, child_frame
        ],
        output='screen'
    )
    #camera
    camera_tf_node = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='static_tf2_lidar',
        arguments=[
            '0.5', '0.0', '0.08',          # x y z
            '0.0', '0.0', '0.0',          # roll pitch yaw (v radiánech)
            'base_footprint', 'camera'   # parent_frame, child_frame
        ],
        output='screen'
    )
    return LaunchDescription([
        imu_tf_node,
        lidar_tf_node,
        camera_tf_node,
        lidar_launch,
        imu_launch,
        teensy_node,
        lidar_odom_launch,
        robot_localization_node,


    ])
