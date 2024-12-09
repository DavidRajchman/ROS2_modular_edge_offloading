#include "AstarClass.hpp"

Astar::Astar() : 
    Node("astar"),
    mapMsg(this->get_logger()),
    positionMsg(this->get_logger()),
    pathMsg(this->get_logger(), this->create_publisher<nav_msgs::msg::Path>("/path", 1))
{
    this->get_logger().set_level(rclcpp::Logger::Level::Debug);


    // Declare parameters from yaml files
    CAR_WIDTH = this->declare_parameter("CAR_WIDTH", 0.2);
    LOOK_DISTANCE = this->declare_parameter("LOOK_DISTANCE", 1.0);
    PENALTY_CHANGE_HIGH = this->declare_parameter("PENALTY_CHANGE_HIGH", 0.2);
    PENALTY_CHANGE_LOW = this->declare_parameter("PENALTY_CHANGE_LOW", 0.1);
    PENALTY_INPUT_OUTPUT = this->declare_parameter("PENALTY_INPUT_OUTPUT", 0.1);
    PENALTY_LAST_CHANGE = this->declare_parameter("PENALTY_LAST_CHANGE", 0.1);
    DILATATION = this->declare_parameter("DILATATION", 1);


    // Publishers and Subscribers
    // path_pub = this->create_publisher<nav_msgs::msg::Path>("/path", 1);

    map_sub = this->create_subscription<nav_msgs::msg::OccupancyGrid>("map", 1, bind(&Astar::mapCb, this, std::placeholders::_1));
    goal_sub = this->create_subscription<geometry_msgs::msg::PoseStamped>("goal_pose", 1, bind(&Astar::goalCb, this, std::placeholders::_1));
    pose_sub = this->create_subscription<services::msg::Position>("robot_position", 1, bind(&Astar::poseCb, this, std::placeholders::_1));
}



void Astar::goalCb(const geometry_msgs::msg::PoseStamped::SharedPtr msg){
    RCLCPP_INFO(get_logger(), "goal received");

    goalX = msg -> pose.position.x;
    goalY = msg -> pose.position.y;

    double rotX, rotY, rotZ, rotW;
    rotX = msg -> pose.orientation.x;
    rotY = msg -> pose.orientation.y;
    rotZ = msg -> pose.orientation.z;
    rotW = msg -> pose.orientation.w;

    goalPhi = quaternionToYaw(rotX, rotY, rotZ, rotW);
    
    
    mapGoalX = (goalX - originX)/mapRes;
    mapGoalY = (goalY - originY)/mapRes;
    mapGoalPhi = yawToGridDirection(goalPhi);
    RCLCPP_DEBUG_STREAM(get_logger(), "x: " << goalX << " y: " << goalY << " phi: " << goalPhi);
    RCLCPP_DEBUG_STREAM(get_logger(), "map x: " << mapGoalX << " map y: " << mapGoalY << " map phi1: " << mapGoalPhi.first <<" map phi 2: "<<mapGoalPhi.second);

    //TODO run A*
    
    // checkpoints.push_back({this->mapRobY, this->mapRobX, this->poseRobPhi});

    // for (const auto& point : this->checkPoints) {
    //     double pointX = std::round(point[0] / this->mapRes) + this->mapOriginX;
    //     double pointY = std::round(point[1] / this->mapRes) + this->mapOriginY;
    //     checkpoints.push_back({pointY, pointX, point[2]});
    // }
    // checkpoints.push_back({this->goalY, this->goalX, yaw});

    // Kopie gridu
    auto gridCopy = this->grid;
    path.clear();

    RCLCPP_INFO(get_logger(), "start");
    RCLCPP_DEBUG_STREAM(get_logger(), "x: " << poseRobX << " y: " << poseRobY << " phi: " << poseRobPhi);
    RCLCPP_DEBUG_STREAM(get_logger(), "map x: " << mapRobX << " map y: " << mapRobY << " map phi1: " << mapRobPhi.first <<" map phi 2: "<<mapRobPhi.second);

    RCLCPP_DEBUG(get_logger(), "before A*");
    astar(gridDil, {mapRobX,mapRobY, mapRobPhi}, {mapGoalX, mapGoalY, mapGoalPhi});
    RCLCPP_DEBUG(get_logger(), "after A*");
    // Druhý algoritmus A* přes všechny kontrolní body
    // for (size_t i = 0; i < static_cast<int>c.size() - 1; ++i) {
    //     this->astar(gridCopy, checkpoints[i], checkpoints[i + 1]);
    //     RCLCPP_INFO(this->get_logger(), "Checkpoint n.%zu reached", i + 1);
    // }

    this->publishPath();
} 

void Astar::mapCb(const nav_msgs::msg::OccupancyGrid::SharedPtr msg) { 
    RCLCPP_DEBUG_ONCE(get_logger(), "map received");
    mapMsg.receiveMsg(msg);

    // widthMap = msg->info.width;
    // heightMap = msg->info.height;
    // mapRes = msg->info.resolution;

    // originX = msg -> info.origin.position.x;
    // originY = msg -> info.origin.position.y;
    // originPhi = quaternionToYaw(
    //         static_cast<double> (msg -> info.origin.orientation.x),
    //         static_cast<double> (msg -> info.origin.orientation.y),
    //         static_cast<double> (msg -> info.origin.orientation.z),
    //         static_cast<double> (msg -> info.origin.orientation.w)
    //     );

    // transformation from 1D array to 2D array
    grid.resize(heightMap, std::vector<int8_t>(widthMap));
    for (unsigned int i = 0; i < heightMap; ++i)
    {
        for (unsigned int j = 0; j < widthMap; ++j)
        {
            grid[i][j] = msg->data[i * widthMap + j];
            if(grid[i][j] == -1 ){
                msg->data[i * widthMap + j]=static_cast<int8_t>(101);
                // RCLCPP_DEBUG_STREAM(get_logger(), "obstacle x: "<< i << " y: "<<j);      
            }
            else if (grid[i][j] >=80){
                // RCLCPP_DEBUG_STREAM(get_logger(), "obstacle2 x: "<< i << " y: "<<j);      

            }
        }
    }
    // cv::Mat mat(grid.size(), grid[0].size(), CV_8S, &msg->data[0]);
    auto [sizeX, sizeY, resolution] = mapMsg.getMapDimensions();
    gridDil = createDelatatedMap(msg->data, sizeX, sizeY, DILATATION);
    RCLCPP_DEBUG_STREAM_ONCE(get_logger(), "width: "<< widthMap << " height: " << heightMap << " resolution: "<<mapRes);
}

void Astar::poseCb(const services::msg::Position::SharedPtr msg) {
    RCLCPP_DEBUG_ONCE(get_logger(), "pose received");

    positionMsg.receiveMsg(msg);
    
    // poseRobX = msg->pose_rob_x;
    // poseRobY = msg->pose_rob_y;
    // poseRobPhi = msg->pose_rob_phi;

    // mapRobX = msg->map_rob_x;
    // mapRobY = msg->map_rob_y;
    // mapRobPhi = yawToGridDirection(poseRobPhi);

    // mapOriginX = msg->map_origin_x;
    // mapOriginY = msg->map_origin_y;

    // RCLCPP_DEBUG_STREAM_ONCE(get_logger(), "x: "<< poseRobX << " y: " << poseRobY << " phi: "<< poseRobPhi);
    // RCLCPP_DEBUG_STREAM_ONCE(get_logger(), "map x: "<< mapRobX << " map y: " << mapRobY );
    // RCLCPP_DEBUG_STREAM_ONCE(get_logger(), "map origin x: "<< mapOriginX << " map origin y: " << mapOriginY );

}

void Astar::publishPath() {
    nav_msgs::msg::Path path2Pub;
    std_msgs::msg::Header header;

    // header
    header.stamp = this->get_clock()->now();
    header.frame_id = "map";

    // vector<geometry_msgs::msg::PoseStamped> poses = convertGridPathToPoses(path);
    path2Pub.header = header;
    // path2Pub.poses = poses;

    path_pub->publish(path2Pub);

    // Aktualizace stavu
    closedPath = true;
    checkpoints.clear();

    RCLCPP_DEBUG(this->get_logger(), "Path published.");
}








double Astar::heuristic(const pair<int, int>& node, const pair<int, int>& goal) {
    return sqrt((node.first - goal.first) * (node.first - goal.first) +(node.second - goal.second) * (node.second - goal.second));
}


vector<NodeStar> Astar::getNeighbor(const NodeStar& node,
                            const vector<vector<bool>>& grid, 
                            vector<vector<array<int, 2>>>& way, 
                            // const pair<int, int>& start,
                            const tuple<int, int, pair<int,int>>& start,
                            const tuple<int, int, pair<int,int>>& goal,
                            int lastChangeDir) {
    // RCLCPP_INFO(get_logger(), "get neighbor");                            
    vector<pair<int, int>> movesAll = {{-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}};
    vector<pair<int, int>> moves = movesAll;

    int ancestorX = way[node.x][node.y][0];
    int ancestorY = way[node.x][node.y][1];
    int dirX = node.x - ancestorX;
    int dirY = node.y - ancestorY;

    if (ancestorX != -1 && ancestorY != -1) {
        auto it = find(moves.begin(), moves.end(), make_pair(dirX, dirY));
        if (it != moves.end()) {
            int index = distance(moves.begin(), it);
            moves = {moves[(index - 1 + 8) % 8], moves[index], moves[(index + 1) % 8]};
        }
    }
    else{
        auto it = find(moves.begin(), moves.end(), get<2>(start));
        if (it != moves.end()) {
            int index = distance(moves.begin(), it);
            moves = {moves[(index - 1 + 8) % 8], moves[index], moves[(index + 1) % 8]};
        } 
    }
    // int startX = start.first;
    // int startY = start.second;

    vector<NodeStar> ret;
    for (int i = 0; i < static_cast<int>(moves.size()); ++i) {
        int newX = node.x + moves[i].first;
        int newY = node.y + moves[i].second;
        // RCLCPP_DEBUG_STREAM(get_logger(), "neighbor x: " << newX<< " y: " << newY);
        

        if (newX < 0 || newY < 0 || newX >= static_cast<int>(grid.size()) || newY >= static_cast<int>(grid[0].size()) || grid[newY][newX]) {
            continue;
        }
        // RCLCPP_DEBUG_STREAM(get_logger(), "no obstacle: " << grid[newX][newY]);

        // else if( grid[newX][newY] == true){
        //     continue;
        // }

        // Penále za změny směru
        double penalty = 0.0;
        if (abs(moves[i].first - dirX) != 0 && abs(moves[i].second - dirY) != 0) {
            penalty = PENALTY_CHANGE_HIGH;
        } else if (abs(moves[i].first - dirX) != 0 || abs(moves[i].second - dirY) != 0) {
            penalty = PENALTY_CHANGE_LOW;
        }

        int startX = get<0>(start);
        int startY = get<1>(start);

        // Penále za výstupní směr blízko startu
        if (abs(newX - startX) <= 8 && abs(newY - startY) <= 8) {

            int index2 = 0;
            auto it = find(movesAll.begin(), movesAll.end(), get<2>(start));
            if (it != movesAll.end()) {
                index2 = distance(movesAll.begin(), it);
                // RCLCPP_DEBUG_STREAM(get_logger(), " index of direction in array is: " << index2 );
                
            } else {
                // RCLCPP_WARN(get_logger(), "direction not found in array");
            }
            int distInd = min({abs(index2 - i), abs((index2 + 8) - i), abs(index2 - (i + 8))});
            penalty += distInd * PENALTY_INPUT_OUTPUT;
        }

        int goalX = get<0>(goal);
        int goalY = get<1>(goal);

        // Penále za vstupní směr blízko cíle
        if (abs(newX - goalX) <= 8 && abs(newY - goalY) <= 8) {

            int index2 = 0;
            auto it = find(movesAll.begin(), movesAll.end(), get<2>(goal));
            if (it != movesAll.end()) {
                index2 = distance(movesAll.begin(), it);
                // RCLCPP_DEBUG_STREAM(get_logger(), " index of direction in array is: " << index2 );
                
            } else {
                // RCLCPP_WARN(get_logger(), "direction not found in array");
            }
            // int index2 = static_cast<int>((((get<2>(goal) + M_PI) / (2 * M_PI)) * 8) - 2) % 8;
            int distInd = min({abs(index2 - i), abs((index2 + 8) - i), abs(index2 - (i + 8))});
            penalty += distInd * PENALTY_INPUT_OUTPUT;
        }
        int changeDir = 0;
        // Penále za příliš častou změnu směru
        changeDir = (i == 1 && ancestorX != -1 && ancestorY != -1) ? lastChangeDir + 1 : 0;
        if (i != 1 && lastChangeDir <= 9 && ancestorX != -1 && ancestorY != -1) {
            penalty += PENALTY_LAST_CHANGE;
        }

        ret.push_back({sqrt(moves[i].first * moves[i].first + moves[i].second * moves[i].second) + penalty, 
                    newX, newY, node.x, node.y, changeDir});
    }
    return ret;
}


void Astar::makePath(const vector<vector<std::array<int, 2>>>& way,
                                        const pair<int, int>& start, const pair<int, int>& goal) {
    RCLCPP_INFO(get_logger(), "making path");
    // vector<pair<int, int>> path;
    int x = goal.first;
    int y = goal.second;

    // if (way[x][y][0] == -1 && way[x][y][1] == -1) {
    //     return;
    // }

    while (x != start.first || y != start.second) {
        // path.emplace_back(x, y);
        // RCLCPP_DEBUG_STREAM(get_logger(), "path x: "<<x<<" path y: "<<y);
        
        // path.emplace_back(x, y);
        path.push_back(make_pair(x, y));
        int newX = way[x][y][0];
        int newY = way[x][y][1];
        x = newX;
        y = newY;
    }

    reverse(path.begin(), path.end());
    // return path;
}


void Astar::astar(const vector<vector<bool>> grid, const tuple<int, int, pair<int,int>>& start, 
                                    const tuple<int, int, pair<int,int>>& goal) {
    vector<std::vector<double>> price(grid.size(), vector<double>(grid[0].size(), 1e9));
    vector<std::vector<array<int, 2>>> way(grid.size(), vector<array<int, 2>>(grid[0].size(), {-1, -1}));
    priority_queue<NodeStar, vector<NodeStar>, greater<>> priorityQ;

    price[get<0>(start)][get<1>(start)] = 0;
    double cost = heuristic(make_pair(get<0>(start), get<1>(start)), {get<0>(goal), get<1>(goal)});
    priorityQ.push({cost, get<0>(start), get<1>(start),-1, -1, 0});
    while (!priorityQ.empty()) {
        NodeStar node = priorityQ.top();
        priorityQ.pop();
        // RCLCPP_DEBUG_STREAM(get_logger(), "node x: " << node.x << " node y: " << node.y);

        if (node.x == get<0>(goal) && node.y == get<1>(goal)) {
            return makePath(way, make_pair(get<0>(start),get<1>(start)), {node.x, node.y});
        }

        // vector<NodeStar> neighbors = getNeighbor(node, grid, way, start, goal, node.lastChangeDir);
        vector<NodeStar> neighbors = getNeighbor(node, grid, way, start, goal, node.lastChangeDir);
        // NodeStar neighbors = getNeighbor(node, grid, way, goal, node.lastChangeDir);
        for (const auto& neighbor : neighbors) {
            double newCost = price[node.x][node.y] + neighbor.cost;
            if (newCost < price[neighbor.x][neighbor.y]) {
                price[neighbor.x][neighbor.y] = newCost;
                priorityQ.push(
                    {
                        newCost + heuristic({neighbor.x, neighbor.y},{get<0>(goal), get<1>(goal)}), 
                        neighbor.x,
                        neighbor.y,
                        neighbor.prevX,
                        neighbor.prevY,
                        neighbor.lastChangeDir
                    });
                way[neighbor.x][neighbor.y] = {neighbor.prevX, neighbor.prevY};
            }
        }
    }

    // return {}; 

}


