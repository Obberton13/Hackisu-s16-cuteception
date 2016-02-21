#include "opencv2/imgproc/imgproc.hpp"
#include "flann/flann.h"
#include "opencv2/flann/flann_base.hpp"
#include "opencv2/flann/miniflann.hpp"
#include <iostream>
#include <map>
#include <fstream>
#include <string>
#include <iterator>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <vector>
#include <ostream>

using namespace cv; 
//using namespace flann;

int main(int argc, char** argv){
	std::multimap<uint32_t, std::string> map;
	//malloc array
	std::string line;
	std::ifstream f(argv[1]);
	int i;
	for(i = 0; std::getline(f,line);++i)
		;
	int *matrix = (int *) malloc(sizeof(int) * i*3);
	
	i = 0;
	std::ifstream file(argv[1]);
	if(file.is_open()){
		while( getline(file,line)){
			std::istringstream ss(line);
		//	printf("%s\n", line.c_str());
			std::istream_iterator<std::string> begin(ss), end;
			std::vector<std::string> words(begin, end);
			std::string filename = words[0];
			if(words.size() < 4)
				continue;
			printf("%s\n", line.c_str());
			std::string l = words[1];
			std::string a = words[2];
			std::string b = words[3];
			
			uint32_t pixel = 0;
			pixel += atoi(l.c_str());
			pixel = pixel << 8;
			pixel += atoi(a.c_str());
			pixel = pixel << 8;
			pixel += atoi(b.c_str()) + 128;
			
			map.insert(std::make_pair(pixel, filename));
			matrix[i*3 + 0] = atoi(l.c_str());
			matrix[i*3 + 1] = atoi(a.c_str());
			matrix[i*3 + 2] = atoi(b.c_str()) + 128;
			++i;
		}
	}
			Mat m = Mat(i,3, CV_32S, matrix);
			cv::flann::AutotunedIndexParams p;
			
			cv::flann::Index index;
			index.build(m, p );
			//index.Index_(m, &p);
}

