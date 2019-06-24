#include <iostream>

#include <curl/curl.h>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <vector>
#include <queue>
#include <stack>
#include <algorithm>
#include <chrono>
#include <fstream>

using namespace std;

/**Who knows... Used by the weird curl thing*/
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}

/**@param url - the URL you want to get
 * @return the GET body (by default, no headers)*/
string curlGET(const char* url) {
	CURL *curl = curl_easy_init();
	string readBuffer;
	if (curl) {
		CURLcode res;
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		res = curl_easy_perform(curl);
		//TODO care about curl res
		curl_easy_cleanup(curl);
	}

	return readBuffer;
}

/**@param url - the URL you want to get
 * @return the GET body (by default, no headers)*/
string curlPOST(const char* url, const char* postdata) {
	CURL *curl = curl_easy_init();
	string readBuffer;
	if (curl) {
		CURLcode res;
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata);

		//set content type to JSON
		struct curl_slist *hs = nullptr;
		hs = curl_slist_append(hs, "Content-Type: application/json");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hs);


		res = curl_easy_perform(curl);
		//TODO care about curl res
		curl_easy_cleanup(curl);
	}

	return readBuffer;
}

/**Contains location and link to neighbors
 *
 * To generate a node, only look at up and left (down and right are (possibly) set by future nodes*/
class MazeNode {
public:
	//defines how short/fast of a path it is. Could use for solving
	int distance = 0;
	//when solving, this is where it's shortest path is currently known to be from. Could be updated in Dikstras
	MazeNode* distanceUsing = nullptr;

	//neighbors
	MazeNode* left = nullptr;
	MazeNode* right = nullptr;
	MazeNode* up = nullptr;
	MazeNode* down = nullptr;

	//location on grid
	int col;
	int row;
	MazeNode(int col, int row) {
		this->col = col;
		this->row = row;
	}
};

/**Represents the entire maze to be solved, where it starts, and where it ends*/
class Maze {
public:
	int fromX;
	int fromY;
	int toX;
	int toY;

	MazeNode* startNode;
	MazeNode* endNode;

	rapidjson::Document maze;
	int sideSize;
	vector<vector<MazeNode*>> nodeGrid;

	Maze(const string& json) {
		//cout<<"MAZE JSON "<<json<<'\n';
		maze.Parse(json.c_str());
		sideSize = maze["map"].Size();
		nodeGrid = vector<vector<MazeNode*>>(sideSize);
		fromX = maze["startingPosition"][0].GetInt();
		fromY = maze["startingPosition"][1].GetInt();
		toX = maze["endingPosition"][0].GetInt();
		toY = maze["endingPosition"][1].GetInt();

		//main the grid that temporarily stores the nodes for initialization.
		for (auto& vectorRow : nodeGrid) {
			vectorRow.resize(sideSize);
			for (auto& node : vectorRow) {
				node = nullptr;
			}
		}
	}

	/**Don't touch start/end nodes
	 *
	 * Merge nodes with only two neighbors
	 * Remove nodes with one neighbor*/
	void simplifyNodes() {

		//TODO the reason this isn't called is because it was unnecessary for the small Mazebot mazes. With dimensions of around 1500x1500, this could likely cut off around 20% of excess nodes

		for (int y=0;y<sideSize;y++) {
			for (int x=0;x<sideSize;x++) {
				MazeNode* node = nodeGrid[x][y];
				if (node == nullptr) continue;
				//todo if start/end, continue
				if ((x == fromX && y == fromY) || (x==toX && y==toY)) {
					continue;
				}

				int neighbors = 0;
				if (node->left != nullptr) neighbors++;
				if (node->right != nullptr) neighbors++;
				if (node->up != nullptr) neighbors++;
				if (node->down != nullptr) neighbors++;
				if (neighbors <= 1) {
					//shouldn't exist (useless in solving)
					nodeGrid[x][y] = nullptr;
					delete &node;
					/*cerr<<"NODE WITH "<<neighbors<<"<=1 NEIGHBORS ("<<node->locX<<","<<node->locY<<")"
							<<(node->left != nullptr)<<","
							<<(node->right != nullptr)<<","
							<<(node->up != nullptr)<<","
							<<(node->down != nullptr)<<",\n";*/
				} else if (neighbors == 2) {
					//todo after solving for efficiency
				}
			}
		}
	}

	/**@param row, col - used for location
	 * @param checkAllNeighbors - Used for placing player object, since it needs to scan below and to right, also
	 *  @return the added node
	 * */
	MazeNode* addNode(int row, int col, bool drawO = true) {
		if (drawO) maze["map"][row][col].SetString("O");
		nodeGrid[row][col] = new MazeNode(col,row);

		//neighbors of same column
		for (int rowA = row-1;rowA>=0;rowA--) {
			if (maze["map"][rowA][col].GetString()[0]=='X') break;//wall; break connection
			//if air gap, continue
			if (nodeGrid[rowA][col] == nullptr) continue;
			nodeGrid[row][col]->left  = nodeGrid[rowA][col];
			nodeGrid[rowA][col]->right = nodeGrid[row][col];
			break;
		}
		//neighbors of same row
		for (int colA = col-1;colA>=0;colA--) {
			if (maze["map"][row][colA].GetString()[0]=='X') break;//wall; break connection
			//if air gap, continue
			if (nodeGrid[row][colA] == nullptr) continue;
			nodeGrid[row][col]->up  = nodeGrid[row][colA];
			nodeGrid[row][colA]->down = nodeGrid[row][col];
			break;
		}
		return nodeGrid[row][col];
	}

	//just draw the maze of X's and spaces. Don't draw nodes
	void printGridSimple() {
		//main print normal grid
		for (int row = 0; row < sideSize; row++) {
			for (int col = 0; col < sideSize; col++) {
				char c = (maze["map"][row][col].GetString()[0]);
				if (c=='O') c=' ';
				cout << c;
			}
			cout << '\n';
		}
	}

	//print detailed grid, along with nodes and node locations
	void printGrid() {
		//main print normal grid
		for (int row=0;row<sideSize;row++) {
			for (int col=0;col<sideSize;col++) {
				cout << (maze["map"][row][col].GetString()[0]);
			}
			cout<<'\n';
		}

		//main print coord
		for (int row=0;row<sideSize;row++) {
			for (int col=0;col<sideSize;col++) {
				if (nodeGrid[row][col] != nullptr) {
					cout << '(' << nodeGrid[row][col]->col << ',' << nodeGrid[row][col]->row << "), ";
				}
			}
			cout<<'\n';
		}

	}

	/**Used in generating a maze. Converts a grid maze into a mesh of nodes, each of which knows its neighbor locaiton*/
	void makeNodes() {
		cout<<"Solving maze "<<maze["name"].GetString()<<'\n';
		cout<<"Maze Path "<<maze["mazePath"].GetString()<<'\n';
		cout<<"From ("<<maze["startingPosition"][0].GetInt()<<","<<maze["startingPosition"][1].GetInt()<<")\n";

		//main add nodes when necessary
		for (int row=0;row<sideSize;row++) {
			for (int col=0;col<sideSize;col++) {

				if (col == fromX && row == fromY) {
					//main make start node
					startNode = addNode(row,col, false);
					//testing cout << '(' << nodeGrid[row][col]->col << ',' << nodeGrid[row][col]->row << "), ";

					continue;
				} else if (col==toX && row==toY) {
					//main make end node
					endNode = addNode(row,col, false);
					//testing cout << '(' << nodeGrid[row][col]->col << ',' << nodeGrid[row][col]->row << "), ";

					continue;
				}

				//walls can't be nodes
				if (maze["map"][row][col].GetString()[0]=='X') continue;

				//main don't make corridors or endpoints nodes
				bool wallToLeft   = row>0?            (maze["map"][row-1][col].GetString()[0]=='X')     :true;
				bool wallToRight  = row<sideSize-1?   (maze["map"][row+1][col].GetString()[0]=='X')     :true;
				if (wallToLeft && wallToRight) continue;

				bool wallToTop    = col>0?            (maze["map"][row][col-1].GetString()[0]=='X')     :true;
				bool wallToBottom = col<sideSize-1?   (maze["map"][row][col+1].GetString()[0]=='X')     :true;
				if (wallToTop && wallToBottom) continue;


				//main make a node and establish neighbors
				addNode(row,col);

				//testing cout << '(' << nodeGrid[row][col]->col << ',' << nodeGrid[row][col]->row << "), ";
			}
		}
//		cout<<"Pre Screening\n";
//		printGrid();
//		cout<<"\n\n";

		//add start and end nodes
//		addNode(maze["startingPosition"][0].GetInt(), maze["startingPosition"][1].GetInt(), true);
//		addNode(maze["endingPosition"][0].GetInt(), maze["endingPosition"][1].GetInt(), true);
	}

	/**Used in solveBreadth by scanNode to see if it is in the visitedStack */
	template <class T> bool vectHolds(vector<T>& v, T mightContain) {
		if (mightContain == nullptr) return false;
		return find(v.begin(), v.end(), mightContain) != v.end();
	}

	/**Used by solveBreadth to see what to do with a neighbor node (to save duplicate code for each direction) */
	void scanNode(vector<MazeNode*>& visitedStack, stack<MazeNode*>& breadthStack, MazeNode* nodeFrom, MazeNode* toVisit, const string& dirName) {
		if (nodeFrom== nullptr) {
			cerr<<"nodeFrom is null\n";
			return;
		}
		//don't visit null nodes
		if (toVisit==nullptr) {
			//testing cout<<"Going "<<dirName<<" was null\n";
			return;
		}
		//don't visit nodes we've already seen
		if (vectHolds(visitedStack, toVisit)) {
			//testing cout<<"Going "<<dirName<<" was already travelled\n";
			return;
		}
		//testing cout<<"Going "<<dirName;
		//testing cout<<" from ("<<nodeFrom<<','<<nodeFrom->col<<','<<nodeFrom->row<<")";
		//testing cout<<" to ("<<toVisit<<','<<toVisit->col<<','<<toVisit->row<<")\n";
		toVisit->distanceUsing = nodeFrom;
		breadthStack.push(toVisit);

	}
	/**using the endNode, it looks at that distanceUsing and generates a string until it reaches the start*/
	string generateDistanceUsingResult() {
		string res = "";

		MazeNode* curNode = endNode;

		while (curNode != startNode) {
			if (curNode == nullptr) {
				cerr << "Null node in the distanceUsing pathway\n";
				return "ERR";
			}

			MazeNode* previousNode = curNode->distanceUsing;

			//a vector of sorts because it has direction and speed. EX: "EEEE" would be speed 4 direction (from the) east
			string distanceVector;

			int dist;
			if (previousNode->row < curNode->row) {
				//go south to get to next node (as most recent step - prepend to string)
				distanceVector = "S";
				dist = curNode->row - previousNode->row;
			} else if (previousNode->row > curNode->row) {
				distanceVector = "N";
				dist = previousNode->row - curNode->row;
			} else if (previousNode->col < curNode->col) {
				distanceVector = "E";
				dist = curNode->col - previousNode->col;
			} else if (previousNode->col > curNode->col) {
				distanceVector = "W";
				dist = previousNode->col - curNode->col;
			} else {
				return "ERR - nodes appear to be in same place?";
			}

			//multiply by the magnitude, account for how it is already one
			while (--dist > 0) {
				distanceVector+=distanceVector[0];
			}

			//add distanceVector to res
			res = distanceVector + res;

			//go down the stack
			curNode = previousNode;
		}
		return res;
	}
	/**
	 * Big-O notation is where N is the cell count
	 *
	 * Takes O(N) storage
	 * Takes O(N^2) time:
	 *      For every node, it checks if it's already in the stack before adding it. N*(N/2)
	 *
	 * */
	string solveBreadth() {

		vector<MazeNode*> visitedStack;
		stack<MazeNode*> breadthStack;

		breadthStack.push(startNode);
		int run=0;
		do {
			run++;
			//testing cout<<"RUNS"<<run<<'\n';
			//get and remove first node
			MazeNode* nodeFrom = breadthStack.top();

			if (nodeFrom == endNode) {
				return generateDistanceUsingResult();
			}

			breadthStack.pop();

			//add rest to breadth stack
			scanNode(visitedStack, breadthStack, nodeFrom, nodeFrom->left,"left");
			scanNode(visitedStack, breadthStack, nodeFrom, nodeFrom->right,"right");
			scanNode(visitedStack, breadthStack, nodeFrom, nodeFrom->up,"up");
			scanNode(visitedStack, breadthStack, nodeFrom, nodeFrom->down,"down");

			visitedStack.push_back(nodeFrom);
		} while (!breadthStack.empty());
		return "ERR Didn't make it.";
	}

	string solveDikstra() {
		//todo dikstra maze solving (was unnecessary for the small mazes from mazebot, since I solved them with breadth first in around a second each)
		return "";
	}
};

//a simple class that tells you how long it took for it to be deconstructed
class Timer {
	using clock = std::chrono::high_resolution_clock;
	std::chrono::time_point<clock> start;
public:
	Timer() : start(clock::now()) {}
	double getElapsed() const {
		return chrono::duration_cast<chrono::duration<double,ratio<1>>>(clock::now() - start).count();
	}
	~Timer() {
		cout<<"Time: "<<getElapsed()<<"s";
	}
};

/**Have a file, maze.csv. In this, each maze URL is a line of text, where the bottom one is unsolved (or the last maze)
 * Read the last line, solve it, append a new line, repeat.
 *
 * */
class Racer {
public:
	Racer() {
		string firstMaze = postInitAccount();
		addMazeToFile(firstMaze);
		//to stop the IDE from flagging me
		bool keepGoing = true;
		while (keepGoing) {
			string mazeUrl = "https://api.noopschallenge.com"+readLastMaze();
			vector<char> buff(mazeUrl.begin(), mazeUrl.end());
			string nextURL = raceThisURL(&buff[0]);
			addMazeToFile(nextURL);
		}
	}
	void addMazeToFile(const string& url) {
		ofstream out;
		out.open("maze.csv", ios_base::app);
		out << "\n" << url;
	}

	//read the most recent maze in the maze.csv file
	string readLastMaze() {
		ifstream fin;
		fin.open("maze.csv");
		if(fin.is_open()) {
			fin.seekg(-1,ios_base::end);                // go to one spot before the EOF
			bool keepLooping = true;
			while(keepLooping) {
				char ch;
				fin.get(ch);                            // Get current byte's data
				if((int)fin.tellg() <= 1) {             // If the data was at or before the 0th byte
					fin.seekg(0);                       // The first line is the last line
					keepLooping = false;                // So stop there
				}
				else if(ch == '\n') {                   // If the data was a newline
					keepLooping = false;                // Stop at the current position.
				}
				else {                                  // If the data was neither a newline nor at the 0 byte
					fin.seekg(-2,ios_base::cur);        // Move to the front of that data, then to the front of the data before it
				}
			}

			string lastLine;
			getline(fin,lastLine);                      // Read the current line
			fin.close();
			return lastLine;     // Display it
		}
		return "ERR";
	}

	string postInitAccount() {
		string postres = curlPOST("https://api.noopschallenge.com/mazebot/race/start",
				"{\n \"login\": \"zaners123\"\n}");
		cout<<"YOUR MAZE: "<<postres<<'\n';
		rapidjson::Document maze;
		maze.Parse(postres.c_str());
		return maze["nextMaze"].GetString();
	}

	/**@param url - the URL of an unsolved maze
	 * @return the next maze
	 * */
	string raceThisURL(const char *url) {
		//main get maze layout
		string mazeJSONStr = curlGET(url);
		//main generate maze
		Maze maze = Maze(mazeJSONStr);
		maze.makeNodes();
		//maze.simplifyNodes();
		//main print maze for reference
		//testing maze.printGridSimple();
		//testing cout<<endl<<endl;
		//main solve maze (TODO use something better than breadth)
		string solve = maze.solveBreadth();
		cout<<"Solved \""<<solve<<"\"\n";
		//main post solution
		string solution = "{\n\"directions\": \""+solve+"\"\n}";
		string solutionResponce = curlPOST(url, solution.c_str());

		cout<<"RESPONCE"<<solutionResponce<<'\n';

		rapidjson::Document resp;
		resp.Parse(solutionResponce.c_str());
		return resp["nextMaze"].GetString();
	}
};

/**
 * This is made for https://api.noopschallenge.com/mazebot/random
 *
 *
 * */
int main() {
	//start the clock
	Timer t;
	//main start the race
	Racer();
	return 0;
}