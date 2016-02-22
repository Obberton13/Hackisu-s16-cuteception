#include <algorithm>
#include <boost/network/protocol/http/server.hpp> 
#include <cmath>
#include <fstream>
#include <iostream>
#include <streambuf>
//#include <magick/api.h>
#include <nanoflann.hpp>
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

template <typename T>
struct PointCloud
{
	struct Point
	{
		T  x,y,z;
	};

	std::vector<Point>  pts;

	// Must return the number of data points
	inline size_t kdtree_get_point_count() const { return pts.size(); }

	// Returns the distance between the vector "p1[0:size-1]" and the data point with index "idx_p2" stored in the class:
	inline T kdtree_distance(const T *p1, const size_t idx_p2,size_t /*size*/) const
	{
		const T d0=p1[0]-pts[idx_p2].x;
		const T d1=p1[1]-pts[idx_p2].y;
		const T d2=p1[2]-pts[idx_p2].z;
		return d0*d0+d1*d1+d2*d2;

	}

	// Returns the dim'th component of the idx'th point in the class:
	// Since this is inlined and the "dim" argument is typically an immediate value, the
	//  "if/else's" are actually solved at compile time.
	inline T kdtree_get_pt(const size_t idx, int dim) const
	{
		if (dim==0) return pts[idx].x;
		else if (dim==1) return pts[idx].y;
		else return pts[idx].z;
	}

	// Optional bounding-box computation: return false to default to a standard bbox computation loop.
	//   Return true if the BBOX was already computed by the class and returned in "bb" so it can be avoided to redo it again.
	//   Look at bb.size() to find out the expected dimensionality (e.g. 2 or 3 for point clouds)
	template <class BBOX>
	bool kdtree_get_bbox(BBOX& /* bb */) const { return false; }

};

typedef nanoflann::KDTreeSingleIndexAdaptor<
		nanoflann::L2_Simple_Adaptor<float, PointCloud<float>> ,
		PointCloud<float>,
		3 /* dim */
		> my_kd_tree_t;

	std::unordered_multimap<int, std::string> colorImageMap;
	my_kd_tree_t* colorsIndex;
	PointCloud<float> cloud;


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

std::string stringsToJson(std::string*** strings, int width, int height)
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
	std::cout << "Number of features: " << std::to_string(colorsIndex->size()) << std::endl;
	std::cout << "Dimenstionality: " << std::to_string(colorsIndex->veclen()) << std::endl;
	std::cout.flush();


	int width = img->columns();
	int height = img->rows();
	img->quantizeColorSpace(RGBColorspace);
	const PixelPacket* pixels = img->getConstPixels(0, 0, width, height);
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {

			Color color(pixels[(i * width) + j]);
			float l, a, b;
			RGBtoLAB(color.redQuantum(), color.greenQuantum(), color.blueQuantum(), &l, &a, &b);
			l *= (255.f/100);
			a += 128;
			b += 128;


			unsigned char lc, ac, bc;

			size_t indices[5];
			float distances[5];
			float query[3] = {l, a, b};
			colorsIndex->knnSearch(query, 5, indices, distances);
			
			while(true) {
				int randIndex = indices[rand() % 5];
				float closeL = cloud.kdtree_get_pt(randIndex, 0);
				float closeA = cloud.kdtree_get_pt(randIndex, 1);
				float closeB = cloud.kdtree_get_pt(randIndex, 2);

				int closeLabKey = ((int)closeL << 16) | ((int)closeA << 8) | (int)closeB;

				bool putIn = false;
				auto range = colorImageMap.equal_range(closeLabKey);
				for(auto it = range.first; it != range.second; it++) {
					if (!putIn) {
						strings[i][j] = &it->second;
						putIn = true;
					} else {
						if (rand() % 10 > 5) 
							strings[i][j] = &it->second;
					}
				}

				if (i == 0 && j == 0)
					break;

				if (i == 0 && strings[i][j] != strings[i][j - 1])
					break;

				if (j == 0 && strings[i][j] != strings[i - 1][j])
					break;

				if (strings[i][j] != strings[i][j - 1] && strings[i][j] != strings[i - 1][j])
					break;
			}
		}
	}
	std::cout << "Generated" << std::endl;
	std::cout << "width: " << std::to_string(img->columns()) << std::endl;
	std::cout << "height: " << std::to_string(img->rows()) << std::endl;
}

my_kd_tree_t* allocIndexFromTextFile(const std::string& filename, std::unordered_multimap<int, std::string>* pointImageMap) {
	std::ifstream inFileStream(filename, std::ifstream::in);
	
	std::cout << "Loading file " + filename << std::endl;
	//malloc array
	std::string line;
	int i;
	for(i = 0; std::getline(inFileStream, line); ++i)
		;
	cloud.pts.resize(i);
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

			int pixel = (l << 16) | (a << 8) | b;
			
			pointImageMap->insert(std::make_pair(pixel, filename));
			cloud.pts[i].x = (float)l;
			cloud.pts[i].y = (float)a;
			cloud.pts[i].z = (float)b;
			++i;
		}

		std::cout << "Read " << std::to_string(i) << " lines" << std::endl;
	}

	auto tree = new my_kd_tree_t(3, cloud, nanoflann::KDTreeSingleIndexAdaptorParams(100));
	tree->buildIndex();
	return tree;
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
	colorsIndex = allocIndexFromTextFile("/Users/Nick/Desktop/hack/Hackisu-s16-cuteception/static/cropped/info.txt", &colorImageMap);
	handler myHandler;
	server::options myOptions(myHandler);
	server server(myOptions.port("4444"));
	server.run();
	
	return 0;
}
