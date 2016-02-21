#include <algorithm>
#include <boost/network/protocol/http/server.hpp> 
#include <cmath>
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
#include <string>
#include <cmath>

#define  labXr_32f  0.433953f /* = xyzXr_32f / 0.950456 */
#define  labXg_32f  0.376219f /* = xyzXg_32f / 0.950456 */
#define  labXb_32f  0.189828f /* = xyzXb_32f / 0.950456 */
#define  labYr_32f  0.212671f /* = xyzYr_32f */
#define  labYg_32f  0.715160f /* = xyzYg_32f */ 
#define  labYb_32f  0.072169f /* = xyzYb_32f */ 
#define  labZr_32f  0.017758f /* = xyzZr_32f / 1.088754 */
#define  labZg_32f  0.109477f /* = xyzZg_32f / 1.088754 */
#define  labZb_32f  0.872766f /* = xyzZb_32f / 1.088754 */
#define  labRx_32f  3.0799327f  /* = xyzRx_32f * 0.950456 */
#define  labRy_32f  (-1.53715f) /* = xyzRy_32f */
#define  labRz_32f  (-0.542782f)/* = xyzRz_32f * 1.088754 */
#define  labGx_32f  (-0.921235f)/* = xyzGx_32f * 0.950456 */
#define  labGy_32f  1.875991f   /* = xyzGy_32f */ 
#define  labGz_32f  0.04524426f /* = xyzGz_32f * 1.088754 */
#define  labBx_32f  0.0528909755f /* = xyzBx_32f * 0.950456 */
#define  labBy_32f  (-0.204043f)  /* = xyzBy_32f */
#define  labBz_32f  1.15115158f   /* = xyzBz_32f * 1.088754 */
#define  labT_32f   0.008856f

#define labSmallScale_32f  7.787f
#define labSmallShift_32f  0.13793103448275862f  /* 16/116 */
#define labLScale_32f      116.f
#define labLShift_32f      16.f
#define labLScale2_32f     903.3f

using namespace Magick;
namespace http = boost::network::http;
struct handler;
typedef boost::network::http::server<handler> server;

std::unordered_multimap<int, std::string> colorImageMap;
flann::Index<flann::L2_3D<unsigned char> >* colorsIndex;


Blob getBlobFromString(std::string const& source)
{
	void* data = (void*)source.data();
	int length = source.length();
	Blob blob(data, length);
	return blob;
}

std::string generateFilename()
{
	static const char* alphanum = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";
	std::string str = "";
	for(int i = 0; i<8; i++)
	{
		str += alphanum[rand() % 62];
	}
	return str;
}

void saveFile(std::string const& body, std::string const& fileName)
{
	std::ofstream file(fileName);
	file << body;
	file.close();
}

std::string stringsToJson(std::string*** strings, int height, int width)
{
	std::string toReturn;
	toReturn += "[";
	for(int x = 0; x < height-1; x++)
	{
		toReturn += "[";
		for(int y = 0; y < width-1; y++)
		{
			toReturn += "\"" + *strings[x][y] + "\",";
		}
		toReturn += "\"" +*strings[x][width-1]+ "\"],";
	}
	toReturn += "[";
	for(int y = 0; y < width-1; y++)
	{
		toReturn += "\"" + *strings[height-1][y] + "\",";
	}
	toReturn += "\"" +*strings[height-1][width-1]+ "\"]]";
	return toReturn;
}

void RGBtoLAB(unsigned char r, unsigned char g, unsigned char b, float *l, float *a, float *bb)
{
	float fr, fg, fb;
	//for some reason, the site gave us the RGB parameters backwards, 
	//so we do BGR instead of RGB, and I'm too lazy to actually 
	//change all of them, so I'm only changing them here.
	fr = (float)b / 255.0f;
	fg = (float)g / 255.0f;
	fb = (float)r / 255.0f;

	if(fr > .04045) fr = pow((fr+.055f)/1.055f, 2.4f);
	else fr = fr/12.92;
	if(fg > .04045) fg = pow((fg+.055f)/1.055f, 2.4f);
	else fg = fg/12.92;
	if(fb > .04045) fb = pow((fb+.055f)/1.055f, 2.4f);
	else fb = fb/12.92;
	
	fr *= 100.f;
	fg *= 100.f;
	fb *= 100.f;

	float x, y, z;
	x = fb * 0.4124f + fg * 0.3576f + fr * 0.1805f;
	y = fb * 0.2126f + fg * 0.7152f + fr * 0.0722f;
	z = fb * 0.0193f + fg * 0.1192f + fr * 0.9505f;

	x = x/95.047f;
	y = y/100.0f;
	z = z/108.883;

	float f = 16.f/116.f;

	if(x > 0.008856f) x = cbrt(x);
	else x = 7.787f * x + f;
	if(y > 0.008856f) y = cbrt(y);
	else y = 7.787f * y + f;
	if(z > 0.008856f) z = cbrt(z);
	else z = 7.787f * z + f;

	*l = 116.f * y - 16.f;
	*a = 500.f * (x - y);
	*bb = 200.f * (y - z);
}

void generateFinalImage(Image* img, std::string*** strings) {
	int width = img->columns();
	int height = img->rows();
	img->colorSpace(RGBColorspace);
	const PixelPacket* pixels = img->getConstPixels(0, 0, width, height);
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			Color color(pixels[(i * width) + height]);
			float l, a, b;
			RGBtoLAB(color.redQuantum(), color.greenQuantum(), color.blueQuantum(), &l, &a, &b);
			
			unsigned char lc, ac, bc;
			lc = (unsigned char) roundf(l * (255.f / 100));
			ac = (unsigned char) roundf(a + 128);
			bc = (unsigned char) roundf(b + 128);


			int labKey = (lc << 16) & (ac << 8) & bc;
			unsigned char matD[3] = {lc, ac, bc};
			int matResD[15];
			typedef flann::Index<flann::L2_3D<unsigned char> >::DistanceType TDist;

			TDist matDistD[15];
			flann::Matrix<unsigned char> mat(matD, 1, 3);
			flann::Matrix<int> matRes(matResD, 5, 3);
			flann::Matrix<TDist> matDist(matDistD, 5, 3);

			colorsIndex->knnSearch(mat, matRes, matDist, 5, flann::SearchParams());
			int randIndex = rand() * 5;

			unsigned char closeL = *(colorsIndex->getPoint(*matRes[3 * randIndex]));
			unsigned char closeA = *(colorsIndex->getPoint(*matRes[3 * randIndex]));
			unsigned char closeB = *(colorsIndex->getPoint(*matRes[3 * randIndex]));

			int closeLabKey = (closeL << 16) & (closeA << 8) & closeB;

			bool putIn = false;
			auto range = colorImageMap.equal_range(closeLabKey);
			for(auto it = range.first; it != range.second; it++) {
				if (!putIn) {
					strings[i][j] = &it->second;
					putIn = true;
				} else {
					if (rand() > 0.5)
						strings[i][j] = &it->second;
				}
			}
		}
	}
}

flann::Index<flann::L2_3D<unsigned char> >* allocIndexFromTextFile(const std::string& filename, std::unordered_multimap<int, std::string>* pointImageMap) {
	std::ifstream inFileStream(filename, std::ifstream::in);
	
	std::cout << "Loading file " + filename << std::endl;
	//malloc array
	std::string line;
	int i;
	for(i = 0; std::getline(inFileStream, line);++i)
		;
	unsigned char *matrix = (unsigned char *) malloc(sizeof(unsigned char) * i*3);
	
	i = 0;
	std::ifstream file(filename, std::ifstream::in);
	if(file.is_open()){
		while( getline(file,line)){
			std::istringstream ss(line);
			std::istream_iterator<std::string> begin(ss), end;
			std::vector<std::string> words(begin, end);
			std::string filename = words[0];
			if(words.size() < 4)
				continue;
			printf("%s\n", line.c_str());
			std::string rs = words[1];
			std::string gs = words[2];
			std::string bs = words[3];

			unsigned char r = (unsigned char) atoi(rs.c_str());
			unsigned char g = (unsigned char) atoi(gs.c_str());
			unsigned char b = (unsigned char) atoi(bs.c_str());

			float lf, af, bf;
			RGBtoLAB(r, g, b, &lf, &af, &bf);
			unsigned char l = (unsigned char)roundf(lf * (255.f / 100));
			unsigned char a = (unsigned char)roundf(af + 128);
						  b = (unsigned char)roundf(bf + 128);


			uint32_t pixel = 0;
			pixel += l;
			pixel = pixel << 8;
			pixel += a;
			pixel = pixel << 8;
			pixel += b;
			
			pointImageMap->insert(std::make_pair(pixel, filename));
			matrix[i*3 + 0] = l;
			matrix[i*3 + 1] = a;
			matrix[i*3 + 2] = b;
			++i;
		}
	}

	flann::Matrix<unsigned char> fMat(matrix, i, 3);
	auto index = new flann::Index<flann::L2_3D<unsigned char>>(fMat, flann::KDTreeSingleIndexParams());
	free(matrix);
	return index;
}

struct handler {
	void operator()(server::request const& req, server::response& res) {
		if(req.method == "GET")
		{
			//serve HTML file.
			std::ifstream htmlfile("client.html");
			std::string str((std::istreambuf_iterator<char>(htmlfile)), std::istreambuf_iterator<char>());
			res = server::response::stock_reply(server::response::ok, str);
			htmlfile.close();
			return;
		}
		std::string filename = generateFilename();
		Blob blob = getBlobFromString(req.body);
		Image img(blob);
		Geometry geo("128x128");
		img.filterType(MagickLib::FilterTypes::LanczosFilter);
		img.zoom(geo);

		int width = img.columns();
		int height = img.rows();

		std::string** strData = (std::string**) malloc(width * height * sizeof(std::string*));
		std::string*** strRows = (std::string***) malloc(height * sizeof(std::string**));

		for (int i = 0; i < height; i++)
			strRows[i] = &strData[width * i];

		generateFinalImage(&img, strRows);
		std::string json = stringsToJson(strRows, width, height);
		free(strData);
		free(strRows);

		res = server::response::stock_reply(server::response::ok, json);
	}
	void log(const server::string_type& message) {
		std::cerr << "ERROR: " << message << std::endl;
	}
};

int main() {
	InitializeMagick(NULL); 
	colorsIndex = allocIndexFromTextFile("static", &colorImageMap);
	handler myHandler;
	server::options myOptions(myHandler);
	server server(myOptions.port("1234"));
	server.run();
	
	return 0;
}
