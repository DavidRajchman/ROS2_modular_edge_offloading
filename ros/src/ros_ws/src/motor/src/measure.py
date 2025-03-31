#!/usr/bin/env python3

import rclpy
from rclpy.node import Node
import csv
import os

from std_msgs.msg import String
from nav_msgs.msg import Odometry

from datetime import datetime
from scipy.spatial.transform import Rotation 


class MinimalSubscriber(Node):

    def __init__(self):
        self.csv_filename = os.path.join(os.path.expanduser('~'), 'data.csv')
        file = open(self.csv_filename, mode='w', newline='')
        writer = csv.writer(file)
        writer.writerow(['Time', 'Frame','x','y', 'yaw', 'vel_x', 'vel_y', 'vel_yaw' ])
        
        super().__init__('measurement_odom')
        self.scanOdom = self.create_subscription(
            Odometry,
            'scan_odom',
            self.callback,
            2)
        
        self.wheelOdom = self.create_subscription(
            Odometry,
            'motor/odom',
            self.callback,
            2)

    def callback(self, msg):
        quat = [msg.pose.pose.orientation.x, msg.pose.pose.orientation.y, msg.pose.pose.orientation.z, msg.pose.pose.orientation.w]

        euler = Rotation.from_quat(quat).as_euler('xyz', degrees=True)

        print(datetime.now(), msg.child_frame_id, msg.pose.pose.position.x, msg.pose.pose.position.y, euler[2], msg.twist.twist.linear.x, msg.twist.twist.linear.y, msg.twist.twist.angular.z)


def main(args=None):
    rclpy.init(args=args)

    minimal_subscriber = MinimalSubscriber()

    rclpy.spin(minimal_subscriber)

    # Destroy the node explicitly
    # (optional - otherwise it will be done automatically
    # when the garbage collector destroys the node object)
    minimal_subscriber.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()