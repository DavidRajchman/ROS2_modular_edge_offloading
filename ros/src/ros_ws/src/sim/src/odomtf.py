import rclpy
from rclpy.node import Node
from geometry_msgs.msg import TransformStamped
from nav_msgs.msg import Odometry
from rosgraph_msgs.msg import Clock
import tf2_ros
# import tf_transformations

class OdomToTFNode(Node):
    def __init__(self):
        super().__init__('odom_to_tf_node')
        self.clockSec = 0
        self.clockNano = 0
        # Vytvoření tf2 broadcasteru
        self.tf_broadcaster = tf2_ros.TransformBroadcaster(self)

        # Předplatitel pro odometrické zprávy
        self.odom_subscriber = self.create_subscription(
            Odometry,
            '/model/vehicle_blue/odometry',
            self.odom_callback,
            1
        )
        self.clock_subscriber = self.create_subscription(
            Clock,
            '/clock',
            self.clock_callback,
            1
        )

    def clock_callback(self, msg):
        print("prijaty clock ", msg)
        self.clockSec = msg.clock.sec
        self.clockNano = msg.clock.nanosec

    def odom_callback(self, msg):
        # Vytvoření transformace z odometrických dat
        print("prijata zprava")
        transform = TransformStamped()

        # Nastavení hlavičky (stamp, frame_id, child_frame_id)
        transform.header.stamp.sec = self.clockSec
        transform.header.stamp.nanosec = self.clockNano
        # transform.header.stamp = self.get_clock().now().to_msg()
        transform.header.frame_id = 'vehicle_blue/odom'  # Rámec souřadnic (např. "odom")
        transform.child_frame_id = 'vehicle_blue/chassis'  # Rámec souřadnic robota (např. "base_link")

        # Nastavení transformace
        transform.transform.translation.x = msg.pose.pose.position.x
        transform.transform.translation.y = msg.pose.pose.position.y
        transform.transform.translation.z = msg.pose.pose.position.z

        # Převeďte quaternion na rotaci
        transform.transform.rotation = msg.pose.pose.orientation

        # Publikování transformace
        self.tf_broadcaster.sendTransform(transform)

def main(args=None):
    rclpy.init(args=args)

    odom_to_tf_node = OdomToTFNode()

    rclpy.spin(odom_to_tf_node)

    # Ukončení, když node skončí
    odom_to_tf_node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
