#include <boost/network/protocol/http/server.hpp> 
#include <flann/flann.hpp>
#include <flann/io/hdf5.h>
#include <fstream>
#include <iostream>
#include <magick/api.h>
#include <memory>
#include <unordered_map>

#include <stdio.h>

namespace http = boost::network::http;
struct handler;
typedef boost::network::http::server<handler> server;

flann::Index<char>* allocIndexFromTextFile(const std::string& filename, std::unordered_multimap<int, std::string>* pointImageMap) {
	std::ifstream inFileStream(filename, std::ifstream::in);

	//TODO
}

Image* allocResizedImageFromFile(const std::string& imageFile) {

}

template<size_t TWidth, size_t THeight>
void generateFinalImage(std::string*[TWidth][THeight]) {

}

std::unordered_multimap<int, std::string> colorImageMap;
flann::Index<char>* colors = allocIndexFromTextFile("/Users/Nick/averagedPhotos/averageColors.txt", &colorImageMap);

struct handler {
	void operator()(server::request const & req, server::response& res) {
		res = server::response::stock_reply(server::response::ok, "got it!");
	}
	void log(const server::string_type& message) {
        std::cerr << "ERROR: " << message << std::endl;
    }
};

int main() {
	handler myHandler;
	server::options myOptions(myHandler);
	server server(myOptions.port("1234"));
	server.run();
}