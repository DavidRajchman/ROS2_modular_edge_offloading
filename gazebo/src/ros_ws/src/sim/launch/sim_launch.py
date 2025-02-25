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
            launch_arguments={'gz_args': '/home/ubuntu/ros_ws/src/sim/description/world.sdf'}.items()
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
                arguments=[ '/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock']
            )
        ]
    )

    gazebo_bridge_lidar = TimerAction(
        period=5.0,  # Zpoždění v sekundách
        actions=[
            Node(
                package='ros_gz_bridge',
                namespace='ros_gz_bridge',
                executable='parameter_bridge',
                name='gz_bridge_lidar',
                arguments=[ '/lidar@sensor_msgs/msg/LaserScan[gz.msgs.LaserScan']
            )
        ]
    )

    gazebo_bridge_cam = TimerAction(
        period=5.0,  # Zpoždění v sekundách
        actions=[
            Node(
                package='ros_gz_bridge',
                namespace='ros_gz_bridge',
                executable='parameter_bridge',
                name='gz_bridge_cam',
                arguments=[ '/camera/image_raw@sensor_msgs/msg/Image[gz.msgs.Image']
            )
        ]
    )
    
    gazebo_bridge_imu = TimerAction(
        period=5.0,  # Zpoždění v sekundách
        actions=[
            Node(
                package='ros_gz_bridge',
                namespace='ros_gz_bridge',
                executable='parameter_bridge',
                name='gz_bridge_imu',
                arguments=[ '/imu@sensor_msgs/msg/Imu[gz.msgs.IMU']
            )
        ]
    )

    gazebo_bridge_odom = TimerAction(
        period=5.0,  # Zpoždění v sekundách
        actions=[
            Node(
                package='ros_gz_bridge',
                namespace='ros_gz_bridge',
                executable='parameter_bridge',
                name='gz_bridge_odom',
                arguments=[ '/model/vehicle_blue/odometry@nav_msgs/msg/Odometry[gz.msgs.Odometry']
            )
        ]
    )

    gazebo_bridge_cmd = TimerAction(
        period=5.0,  # Zpoždění v sekundách
        actions=[
            Node(
                package='ros_gz_bridge',
                namespace='ros_gz_bridge',
                executable='parameter_bridge',
                name='gz_bridge_cmd',
                arguments=[ '/model/vehicle_blue/cmd_vel@geometry_msgs/msg/Twist]gz.msgs.Twist']
            )
        ]
    )

    return LaunchDescription([
        gazebo_sim,
        gazebo_bridge_clock,
        gazebo_bridge_lidar,
        gazebo_bridge_cam,
        gazebo_bridge_imu,
        gazebo_bridge_odom,
        gazebo_bridge_cmd

        
    ])
