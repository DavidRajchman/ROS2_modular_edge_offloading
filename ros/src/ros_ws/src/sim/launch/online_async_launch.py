import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (DeclareLaunchArgument, EmitEvent, LogInfo,
                            RegisterEventHandler)
from launch.conditions import IfCondition
from launch.events import matches_action
from launch.substitutions import (AndSubstitution, LaunchConfiguration,
                                  NotSubstitution)
from launch_ros.actions import LifecycleNode
from launch_ros.event_handlers import OnStateTransition
from launch_ros.events.lifecycle import ChangeState
from lifecycle_msgs.msg import Transition
from launch_ros.actions import Node



def generate_launch_description():
    autostart = LaunchConfiguration('autostart')
    use_lifecycle_manager = LaunchConfiguration("use_lifecycle_manager")
    use_sim_time = LaunchConfiguration('use_sim_time')
    slam_params_file = LaunchConfiguration('slam_params_file')

    declare_autostart_cmd = DeclareLaunchArgument(
        'autostart', default_value='true',
        description='Automatically startup the slamtoolbox. '
                    'Ignored when use_lifecycle_manager is true.')
    declare_use_lifecycle_manager = DeclareLaunchArgument(
        'use_lifecycle_manager', default_value='false',
        description='Enable bond connection during node activation')
    declare_use_sim_time_argument = DeclareLaunchArgument(
        'use_sim_time',
        default_value='true',
        description='Use simulation/Gazebo clock')
    declare_slam_params_file_cmd = DeclareLaunchArgument(
        'slam_params_file',
        default_value = '/home/ubuntu/ros_ws/src/sim/config/online_async.yaml',
        # os.path.join(get_package_share_directory("sim"),
                                #    'config', 'online_async.yaml'),
        description='Full path to the ROS2 parameters file to use for the slam_toolbox node')

    start_async_slam_toolbox_node = LifecycleNode(
        parameters=[
          slam_params_file,
          {
            'use_lifecycle_manager': use_lifecycle_manager,
            'use_sim_time': use_sim_time
          }
        ],
        package='slam_toolbox',
        executable='async_slam_toolbox_node',
        name='slam_toolbox',
        output='screen',
        namespace=''
    )

    configure_event = EmitEvent(
        event=ChangeState(
          lifecycle_node_matcher=matches_action(start_async_slam_toolbox_node),
          transition_id=Transition.TRANSITION_CONFIGURE
        ),
        condition=IfCondition(AndSubstitution(autostart, NotSubstitution(use_lifecycle_manager)))
    )

    activate_event = RegisterEventHandler(
        OnStateTransition(
            target_lifecycle_node=start_async_slam_toolbox_node,
            start_state="configuring",
            goal_state="inactive",
            entities=[
                LogInfo(msg="[LifecycleLaunch] Slamtoolbox node is activating."),
                EmitEvent(event=ChangeState(
                    lifecycle_node_matcher=matches_action(start_async_slam_toolbox_node),
                    transition_id=Transition.TRANSITION_ACTIVATE
                ))
            ]
        ),
        condition=IfCondition(AndSubstitution(autostart, NotSubstitution(use_lifecycle_manager)))
    )

    static_laser = Node(
        package="tf2_ros",
        executable="static_transform_publisher",
        arguments=["0", "0", "1", "0", "0", "0", "base_footprint", "laser"],
        output="screen"
    )

    # static_camera = Node(
    #     package="tf2_ros",
    #     executable="static_transform_publisher",
    #     arguments=["0", "0", "1", "0", "0", "0", "vehicle_blue/chassis", "vehicle_blue/laser_frame/gpu_lidar"],
    #     output="screen"
    # )

    # static_imu = Node(
    #     package="tf2_ros",
    #     executable="static_transform_publisher",
    #     arguments=["0", "0", "1", "0", "0", "0", "vehicle_blue/chassis", "vehicle_blue/laser_frame/gpu_lidar"],
    #     output="screen"
    # )

    # static_odom = Node(
    #     package="tf2_ros",
    #     executable="static_transform_publisher",
    #     arguments=["0", "0", "1", "0", "0", "0", "base_footprint", "vehicle_blue/odom"],
    #     output="screen"
    # )

    static_imu = Node(
        package="tf2_ros",
        executable="static_transform_publisher",
        arguments=["0", "0", "1", "0", "0", "0", "base_footprint", "imu"],
        output="screen"
    )

    # odom = Node(
    #         package='robot_localization',
    #         executable='ekf_node',
    #         name='ekf_node',
    #         output='screen',
    #         parameters=[{'use_sim_time': True}, '/home/ubuntu/ros_ws/src/sim/config/ekf.yaml']
    #     )

    ld = LaunchDescription()

    ld.add_action(declare_autostart_cmd)
    ld.add_action(declare_use_lifecycle_manager)
    ld.add_action(declare_use_sim_time_argument)
    ld.add_action(declare_slam_params_file_cmd)
    ld.add_action(start_async_slam_toolbox_node)
    ld.add_action(configure_event)
    ld.add_action(activate_event)
    ld.add_action(static_laser)
    # ld.add_action(static_odom)
    ld.add_action(static_imu)
    # ld.add_action(odom)

    return ld
