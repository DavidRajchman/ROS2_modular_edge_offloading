#include "AstarClass.hpp"

Astar::Astar() : 
    Node("astar"),
    mapMsg(this->get_logger()),
    positionMsg(this->get_logger()),
    pathMsg(this->get_logger(), this->create_publisher<nav_msgs::msg::Path>("/path", 1)),
    goalMsg(this->get_logger())
{
    this->get_logger().set_level(rclcpp::Logger::Level::Debug);

    // Declare parameters from yaml files
    CAR_WIDTH = this->declare_parameter("CAR_WIDTH", 0.2);
    LOOK_DISTANCE = this->declare_parameter("LOOK_DISTANCE", 1.0);
    PENALTY_CHANGE_HIGH = this->declare_parameter("PENALTY_CHANGE_HIGH", 0.2);
    PENALTY_CHANGE_LOW = this->declare_parameter("PENALTY_CHANGE_LOW", 0.1);
    PENALTY_INPUT_OUTPUT = this->declare_parameter("PENALTY_OUTPUT_OUTPUT", 200);
    PENALTY_INPUT_OUTPUT = this->declare_parameter("PENALTY_INPUT_OUTPUT", 1'000);
    PENALTY_LAST_CHANGE = this->declare_parameter("PENALTY_LAST_CHANGE", 5'000);
    DILATATION = this->declare_parameter("DILATATION", 1);

    // Publishers and Subscribers
    map_sub = this->create_subscription<nav_msgs::msg::OccupancyGrid>("map", 1, bind(&Astar::mapCb, this, std::placeholders::_1));
    goal_sub = this->create_subscription<geometry_msgs::msg::PoseStamped>("goal_pose", 1, bind(&Astar::goalCb, this, std::placeholders::_1));
    pose_sub = this->create_subscription<services::msg::Position>("robot_position", 1, bind(&Astar::poseCb, this, std::placeholders::_1));
}


void Astar::goalCb(const geometry_msgs::msg::PoseStamped::SharedPtr msg){
    RCLCPP_INFO(get_logger(), "goal received");

    goalMsg.receiveMsg(msg);
    auto [poseX, poseY, posePhi] = goalMsg.getPosition();
    auto [mapOriginX, mapOriginY, mapOriginPhi] = mapMsg.getMapOriginGrid();
    auto [sizeX, sizeY, mapResolution] = mapMsg.getMapDimensions();
    tuple<int, int, int> goalPosition= positionToGridPosition(poseX, poseY, posePhi, mapOriginX, mapOriginY, mapResolution);
    tuple<int, int, int> startPosition = positionMsg.getMapPosition();
    path.clear();

    RCLCPP_DEBUG_STREAM(get_logger(), "orinX: "<< mapOriginX << " orinY: " << mapOriginY << " res: "<<mapResolution);
    RCLCPP_DEBUG_STREAM(get_logger(), "startX: "<< get<0>(startPosition) << " startY: " << get<1>(startPosition) << " startPhi: "<<get<2>(startPosition));
    RCLCPP_DEBUG_STREAM(get_logger(), "goalX: "<< poseX << " goalY: " << poseY << " goalPhi: "<< posePhi);
    RCLCPP_DEBUG_STREAM(get_logger(), "goalmapX: "<< get<0>(goalPosition) << " goalmapY: " << get<1>(goalPosition) << " goalmapPhi: "<<get<2>(goalPosition));

    RCLCPP_DEBUG(get_logger(), "before A*");
    vector<int8_t> data = mapMsg.getMap();
    vector<vector<bool>> gridDil = createDelatatedMap(data, sizeX, sizeY, DILATATION);
    astar(gridDil, startPosition, goalPosition);
    RCLCPP_DEBUG(get_logger(), "after A*");

    this->publishPath();
} 

void Astar::mapCb(const nav_msgs::msg::OccupancyGrid::SharedPtr msg) { 
    RCLCPP_DEBUG_ONCE(get_logger(), "map received");
    mapMsg.receiveMsg(msg);

    // auto [sizeX, sizeY, resolution] = mapMsg.getMapDimensions();
    // auto [mapOriginX, mapOriginY, mapOriginPhi] = mapMsg.getMapOriginGrid();

    // RCLCPP_DEBUG_STREAM_ONCE(get_logger(), "originx: "<< mapOriginX << " originy: " << mapOriginY );

    // positionMsg.setMapDimensions(sizeX, sizeY, resolution);
    // positionMsg.setMapOrigin(mapOriginX, mapOriginY);

    // RCLCPP_DEBUG_STREAM_ONCE(get_logger(), "width: "<< widthMap << " height: " << heightMap << " resolution: "<<mapRes);
}

void Astar::poseCb(const services::msg::Position::SharedPtr msg) {
    RCLCPP_DEBUG_ONCE(get_logger(), "pose received");
    positionMsg.receiveMsg(msg);
}

void Astar::publishPath() {
    auto [originX, originY, originPhi]=mapMsg.getMapOriginGrid();
    auto [sizeX, sizeY, mapResolution] = mapMsg.getMapDimensions();
    pathMsg.setPath(path, originX, originY, mapResolution);
    pathMsg.publishMsg();

    // Aktualizace stavu
    closedPath = true;
    // checkpoints.clear();

    RCLCPP_DEBUG(this->get_logger(), "Path published.");
}

double Astar::heuristic(const pair<int, int>& node, const pair<int, int>& goal) {
    return sqrt((node.first - goal.first) * (node.first - goal.first) +(node.second - goal.second) * (node.second - goal.second));
}

vector<NodeStar> Astar::getNeighbor(const NodeStar& node, const vector<vector<bool>>& grid, 
                    vector<vector<array<int, 2>>>& way, const tuple<int, int, int>& start,
                    const tuple<int, int, int>& goal, int lastChangeDir) {
    // vector<pair<int, int>> movesAll = {{-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}};
    vector<pair<int, int>> movesAll = {{-1, 0}, {-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}};
    vector<pair<int, int>> moves = movesAll;
    vector<int> indexMoves;



    int ancestorX = way[node.x][node.y][0];
    int ancestorY = way[node.x][node.y][1];
    int dirX = node.x - ancestorX;
    int dirY = node.y - ancestorY;

    if (ancestorX != -1 && ancestorY != -1) {
        auto it = find(moves.begin(), moves.end(), make_pair(dirX, dirY));
        if (it != moves.end()) {
            int index = distance(moves.begin(), it);
            indexMoves = {(index - 1 + 8) % 8, index, (index + 1 ) % 8};
        }
    }
    else{
        indexMoves = {(get<2>(start) - 1 + 8) % 8, get<2>(start), (get<2>(start) + 1 ) % 8};

        moves = {moves[(get<2>(start) -1 +8) % 8], moves[get<2>(start)], moves[(get<2>(start) + 1) % 8]};
        // moves =  {moves[get<2>(start)]};
        RCLCPP_DEBUG_STREAM_ONCE(this->get_logger(), "start phi: " << get<2>(start));
        // RCLCPP_DEBUG_STREAM_ONCE(this->get_logger(), "moves: " << moves[0].first <<" "<<moves[0].second<<"\n"
        //                                                 << moves[1].first <<" "<<moves[1].second<<"\n"
        //                                                 << moves[2].first <<" "<<moves[2].second);

    }

    vector<NodeStar> ret;
    for(int indexMove: indexMoves){
    // for (int i = 0; i < static_cast<int>(indexMoves.size()); ++i) {
        pair<int, int> move = {movesAll[indexMove]};
        int newX = node.x + move.first;
        int newY = node.y + move.second;

        if (newX < 0 || newY < 0 || newX >= static_cast<int>(grid.size()) || newY >= static_cast<int>(grid[0].size()) || grid[newY][newX]) {
            // RCLCPP_DEBUG_STREAM(this->get_logger(), "obstacle on: "<<newX<<" "<<newY);
            continue;
        }

        // Penalization for direction change
        double penalty = 0.0;
        if (abs(move.first - dirX) != 0 && abs(move.second - dirY) != 0) {
            penalty = PENALTY_CHANGE_HIGH;
        } else if (abs(move.first - dirX) != 0 || abs(move.second - dirY) != 0) {
            penalty = PENALTY_CHANGE_LOW;
        }

        int startX = get<0>(start);
        int startY = get<1>(start);

        // bad output start penalization
        if (abs(newX - startX) <= 8 && abs(newY - startY) <= 8) {
            int index2 = get<2>(start);
            int distInd = min({abs(index2 - indexMove), abs((index2 + 8) - indexMove), abs(index2 - (indexMove + 8))});
            RCLCPP_DEBUG_STREAM_ONCE(this->get_logger(), "penalty start: " << distInd);
            penalty += distInd * PENALTY_OUTPUT_OUTPUT;
        }

        int goalX = get<0>(goal);
        int goalY = get<1>(goal);

        // bad input goal penalization
        if (abs(newX - goalX) <= 8 && abs(newY - goalY) <= 8) {

            int index2 = get<2>(goal);
            int distInd = min({abs(index2 - indexMove), abs((index2 + 8) - indexMove), abs(index2 - (indexMove + 8))});
            penalty += distInd * PENALTY_INPUT_OUTPUT;
        }
        int changeDir = 0;
        // penalization for direction change
        changeDir = (indexMove == indexMoves[1] && ancestorX != -1 && ancestorY != -1) ? lastChangeDir + 1 : 0;
        if (indexMove != indexMoves[1] && lastChangeDir <= 9 && ancestorX != -1 && ancestorY != -1) {
            penalty += PENALTY_LAST_CHANGE;
        }

        ret.push_back({sqrt(move.first * move.first + move.second * move.second) + penalty, 
                    newX, newY, node.x, node.y, changeDir});
    }
    return ret;
}


void Astar::makePath(const vector<vector<std::array<int, 2>>>& way,
                                        const pair<int, int>& start, const pair<int, int>& goal) {
    RCLCPP_INFO(get_logger(), "making path");
    int x = goal.first;
    int y = goal.second;

    while (x != start.first || y != start.second) {
        path.push_back(make_pair(x, y));
        int newX = way[x][y][0];
        int newY = way[x][y][1];
        x = newX;
        y = newY;
    }

    reverse(path.begin(), path.end());
}


void Astar::astar(const vector<vector<bool>> grid, const tuple<int, int, int>& start, const tuple<int, int, int>& goal) {
    
    vector<vector<double>> price(grid.size(), vector<double>(grid[0].size(), 1e9));
    RCLCPP_DEBUG(get_logger(), "before init");
    vector<std::vector<array<int, 2>>> way(grid.size(), vector<array<int, 2>>(grid[0].size(), {-1, -1}));
    priority_queue<NodeStar, vector<NodeStar>, greater<>> priorityQ;
    RCLCPP_DEBUG(get_logger(), "after init");

    // auto [sizeX, sizeY, resolution] = mapMsg.getMapDimensions();
    // RCLCPP_DEBUG(get_logger(), "before dilatation");
    // RCLCPP_DEBUG(get_logger(), "after dilatation");

    price[get<0>(start)][get<1>(start)] = 0;
    double cost = heuristic(make_pair(get<0>(start), get<1>(start)), {get<0>(goal), get<1>(goal)});
    priorityQ.push({cost, get<0>(start), get<1>(start),-1, -1, 0});
    while (!priorityQ.empty()) {
        NodeStar node = priorityQ.top();
        priorityQ.pop();

        if (node.x == get<0>(goal) && node.y == get<1>(goal)) {
            return makePath(way, make_pair(get<0>(start),get<1>(start)), {node.x, node.y});
        }

        vector<NodeStar> neighbors = getNeighbor(node, grid, way, start, goal, node.lastChangeDir);
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

}


