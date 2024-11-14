#include "AstarClass.hpp"

//     bool isReachable(int x, int y) {
//         // Převedení souřadnic x a y
//         x = static_cast<int>(std::round(x / mapRes_)) + mapOriginX_;
//         y = static_cast<int>(std::round(y / mapRes_)) + mapOriginY_;
        
//         // Kopírování gridu
//         cv::Mat gridCopy = grid_.clone();

//         // Vytváření masky pro nedostupné buňky
//         cv::Mat gridHelp1 = (gridCopy >= 90);
//         cv::Mat gridHelp2 = (gridCopy < 0);
//         cv::Mat grid2;
//         cv::bitwise_or(gridHelp1, gridHelp2, grid2);

//         // Dilatace (rozšíření) masky
//         int dilationSize = DILATATION;
//         cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT,
//                                                     cv::Size(2 * dilationSize + 1, 2 * dilationSize + 1),
//                                                     cv::Point(dilationSize, dilationSize));
//         cv::dilate(grid2, grid2, element);

//         // Kontrola dostupnosti bodu
//         if (grid2.at<uchar>(y, x) == 1) {
//             RCLCPP_WARN(rclcpp::get_logger("rclcpp"), "Unreachable checkpoint!!!");
//             pathStatus_ = "noGoal";
//             return false;
//         }
//         return true;
//     }


int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<Astar>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}