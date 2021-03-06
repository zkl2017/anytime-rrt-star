#include <iostream>
#include <fstream> 
#include <string>
#include <vector>
#include <random>
#include <cmath>
#include <math.h> 
#include <algorithm> 
#include <chrono>
#include "Node.h"
#include "Point.h"
#include "World.h"

//returns 2d euclidean distance between 2 nodes  
double distance(Node* nodeA, Node* nodeB){
	double diffX = nodeA->x - nodeB->x;
	double diffY = nodeA->y - nodeB->y;
	return hypot(diffX, diffY);
}


//calculates cost of a node from root 
double depth(Node* node){
	if(node->parent == NULL)
		return 0.0; //root
	return distance(node, node->parent) + depth(node->parent);
}

//print solution trajectory: series of x y points preceeded by number of points in solution
//last node is always goal, keep referring up and then reverse to be in order 
void printSolution(Node* foundSolutionNode, std::vector<Node*> tree, int64_t duration){
	std::vector<Node*> solutionTrajectory;
	Node* cur = foundSolutionNode; 
	while(cur->parent){
		solutionTrajectory.push_back(cur);
		cur = cur->parent;
	}
	std::reverse(solutionTrajectory.begin(), solutionTrajectory.end());
	std::cout << solutionTrajectory.size()+1 << '\n';
	std::cout << duration << '\n';
	std::cout << depth(foundSolutionNode) << '\n'; 
	std::cout << cur->location.x << ' ' << cur->location.y << std::endl;
	for(Node* n : solutionTrajectory)
		std::cout << n->x << ' ' << n->y << '\n';
	

}

//print entire explore tree in line segments: x1 y1 x2 y2 preceeded by number of lines 
void printTree(std::vector<Node*> tree){
	std::cout << tree.size()-1 << '\n'; 
	for(Node* n : tree)
		if(n->parent)//root wont segfault
			std::cout << n->x << ' ' << n->y << ' ' << n->parent->x << ' ' << n->parent->y << '\n';
}

//prints status of motion planning in format for visualizer 
void reportScene(World world, Point start, Point goal, std::vector<Node*> tree){
	world.print(); 
	std::cout << start.x << '\n' << start.y << std::endl;
	std::cout << goal.x << '\n' << goal.y << std::endl;
	if(tree.size())
		printTree(tree); 
}

//Uses DDA algorithm to check if line intersects blocked area
//DDA (Digital Differential Analyzer) takes two points and calculates
//series of points along their line segment 
//returns true if clear, false if collision  
bool validTrajectory(Node* nodeA, Node* nodeB, World world){
  double step,curX,curY;
  double x1 = nodeA->x;
  double y1 = nodeA->y;
  double x2 = nodeB->x;
  double y2 = nodeB->y; 
  double dx = (x2 - x1);
  double dy = (y2 - y1);
  step = 50; 
  dx = (dx/step);
  dy = (dy/step);
  curX = x1;
  curY = y1;
  for(int i = 0; i <= step; i++) {
	if(world.world[(int(curY)*world.width)+int(curX)] == '#')
		return 0; 
    curX += dx;
    curY += dy;
  }
  return 1; 
}


//calculates depth of a node considering another as its parent
double testDepth(Node* node, Node* givenParent){
	return distance(node, givenParent) + depth(givenParent); 
}

//return the nearest existing node to a given node in the world 
//O(n) compares all nodes and saves returns closest 
Node* nearestNeighbor(Node* node, std::vector<Node*> exploredNodes){
	Node* nearest;
	double nearestDist = INFINITY;  
	for(Node* n : exploredNodes){
		double dist = distance(node, n);
		if(dist < nearestDist){
			nearest = n; 
			nearestDist = dist;
		}
	}
	return nearest;
}


//All nodes in neighboord consider the new sample as their parent
//sample becomes their parent if total cost from root is reduced  
void rewire(Node* sample, std::vector<Node*> neighborhood, World world){
	for(Node* neighbor : neighborhood)
		if(testDepth(neighbor, sample) < depth(neighbor))
			if(validTrajectory(sample, neighbor, world))
				neighbor->parent = sample; 
}

//return node in neighborhood where node gets parent of least cost from root 
Node* chooseParent(Node* sample, std::vector<Node*> neighborhood, std::vector<Node*> exploredNodes, World world){
	if(neighborhood.size() == 0)
		return nearestNeighbor(sample, exploredNodes);
	Node* bestParent = neighborhood[0]; 
	for( Node* neighbor : neighborhood)
		if(testDepth(sample, neighbor) < testDepth(sample, bestParent))
				bestParent = neighbor; 
	return bestParent; 
}

//O(n) distance search among all explored nodes to create vector of valid nodes within
//given radius to sample
std::vector<Node*> getNeighborhood(Node* center, std::vector<Node*> exploredNodes, double searchRadius, World world){
	std::vector<Node*> neighborhood; 
	for(Node* n : exploredNodes)
		if(distance(center, n) < searchRadius)
			if(validTrajectory(center, n, world))
				neighborhood.push_back(n);
	return neighborhood; 
}

//RRT with optimization. Sample node chooses parent with of least cost from root 
//Then all neighbors within a radius rewire considering sample as their parent 
//time limit in milliseconds 
std::vector<Node*> rrtstar(World world, Point start, Point goalPoint, int timeLimit){
	std::vector<Node*> nodes;
	Node* root = new Node(NULL, start);
	nodes.push_back(root);
	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	Node* foundSolutionNode = NULL;  
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - begin).count();
	while(duration < timeLimit){
		reportScene(world, start, goalPoint, nodes);
		Node* sample = world.getNodeSample(20, goalPoint);
		std::vector<Node*> neighborhood = getNeighborhood(sample, nodes, 0.5, world);
		Node* parent = chooseParent(sample, neighborhood, nodes, world);
		if(validTrajectory(sample, parent, world)){
			sample->parent = parent; 
			rewire(sample, neighborhood, world);
			nodes.push_back(sample);
			if(sample->x == goalPoint.x && sample->y == goalPoint.y)
				foundSolutionNode = sample;  
		} 
		now = std::chrono::steady_clock::now();
		duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - begin).count();
		if(foundSolutionNode)
			printSolution(foundSolutionNode, nodes, duration); 
		else
			std::cout << '0' << '\n';
	}
	return nodes;
}


//planner takes world on stdin and performs planner 
int main(int argc, char* argv[]){

	//header info  
	char width[10];
	char height[10];
	std::cin.getline(width, 10);
	std::cin.getline(height, 10);
	int numColumns = std::stoi(width);
	int numRows = std::stoi(height);
	
	//read world into 1d: # = blocked, _ = open 
	std::vector<char> input;
	for(int i = 0; i < numRows*numColumns; i++){
		char cur; 
		std::cin >> cur; 
		input.push_back(cur);
	}

	//setup for sampler
	World world = World(input, numRows, numColumns);
	Point start = Point(std::stod(argv[2]), std::stod(argv[3]) );
	Point goal = Point(std::stod(argv[4]), std::stod(argv[5]) );
	std::cout << world.width << '\n' << world.height << std::endl;
	
	std::vector<Node*> tree = rrtstar(world, start, goal, std::stoi(argv[1]));
	
	reportScene(world, start, goal, tree);
	
	return 0;
}
