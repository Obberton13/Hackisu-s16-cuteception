#include <boost/network/protocol/http/server.hpp> 
#include <flann/flann.hpp>
#include <flann/io/hdf5.h>
#include <fstream>
#include <iostream>
#include <streambuf>
//#include <magick/api.h>
#include <Magick++.h>
#include <memory>
#include <unordered_map>
#include <stdio.h>

using namespace Magick;
namespace http = boost::network::http;
struct handler;
typedef boost::network::http::server<handler> server;

flann::Index<char>* allocIndexFromTextFile(const std::string& filename, std::unordered_multimap<int, std::string>* pointImageMap) {
	std::ifstream inFileStream(filename, std::ifstream::in);
	//TODO
	
	std::cout << "Loading file " + filename << std::endl;
	
	return NULL;
}

Blob getBlobFromString(std::string const& source)
{
	void* data = (void*)source.data();
	int length = source.length();
	Blob blob(data, length);
	return blob;
}

void generateFinalImage(Image* img, std::string*** strings) {
	
}

std::string generateFilename()
{
	const char* alphanum = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";
	std::string str = "";
	for(int i = 0; i<8; i++)
	{
		str += alphanum[rand() % (sizeof(alphanum) - 1)];
	}
	return str;
}

void saveFile(std::string const& body, std::string const& fileName)
{
	std::ofstream file(fileName);
	file << body;
	file.close();
}

std::unordered_multimap<int, std::string> colorImageMap;
flann::Index<char>* colors = allocIndexFromTextFile("/Users/Nick/averagedPhotos/averageColors.txt", &colorImageMap);

struct handler {
	void operator()(server::request const& req, server::response& res) {
		if(req.method == "GET")
		{
			//serve HTML file.
			std::cout << "Serving HTML file: " << std::endl;
			std::ifstream htmlfile("client.html");
			std::string str((std::istreambuf_iterator<char>(htmlfile)), std::istreambuf_iterator<char>());
			std::cout << str << std::endl;
			res = server::response::stock_reply(server::response::ok, str);
			htmlfile.close();
			return;
		}
		std::string filename = generateFilename();
		Blob blob = getBlobFromString(req.body);
		Image img(blob);
		img.magick("JPEG");
		//Geometry geo("128x128");
		//img.sample(geo);
		//img.filterType(FilterType.Lanczos);
		
		//TODO take the image and do things to it to make it look cool.
		//this would be the whole flann thing

		res = server::response::stock_reply(server::response::ok, req.body);
	}
	void log(const server::string_type& message) {
		std::cerr << "ERROR: " << message << std::endl;
	}
};

int main() {
	InitializeMagick(NULL); 
	handler myHandler;
	server::options myOptions(myHandler);
	server server(myOptions.port("1234"));
	server.run();
	return 0;
}
