#include <iostream>
#include <bitset>
#include <string>
#include <iomanip>

#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/core.hpp>

using namespace std;
using namespace cv;

static void calchash(Mat_<double> image, const int& height, const int& width, bitset<64>& dhash)
{
	for (int i = 0; i < height; i++)
	{
		int pos = i * height;
		for (int j = 0; j < width - 1; j++)
		{
			dhash[pos + j] = (image(i, j) > image(i, j + 1)) ? 1 : 0;
		}
	}
}

static int hammingDistance(const bitset<64>& dhash1, const bitset<64>& dhash2)
{
	int distance = 0;
	for (int i = 0; i < 64; i++) {
		distance += (dhash1[i] == dhash2[i] ? 0 : 1);
	}
	return distance;
}


int image_similarity(cv::Mat &img1, cv::Mat &img2)
{
	if (!img1.data|| img2.empty() || !img2.data || img2.empty())
	{
		cout << "the image is not exist or is empty !" << endl;
		return -1;
	}

	int rows = 8, cols = 9;
	resize(img1, img1, Size(cols, rows));   // 将图片缩放为8*9
	cvtColor(img1, img1, COLOR_BGR2GRAY);      // 图片进行灰度化
	
	bitset<64> dhash1;
	calchash(img1, rows, cols, dhash1);
	
	resize(img2, img2, Size(cols, rows));   // 将图片缩放为8*9
	cvtColor(img2, img2, COLOR_BGR2GRAY);      // 图片进行灰度化
	
	bitset<64> dhash2;
	calchash(img2, rows, cols, dhash2);
	
	int distance = hammingDistance(dhash1, dhash2);      // 计算汉明距离

	return distance;
}

double variance_of_laplacian_cv(cv::Mat &img)
{
    if (img.data == NULL || img.empty()) {
        return 0.0;
    }

    cv::Mat gray;
    if (img.channels() == 3) {
        cv::cvtColor(img, gray, cv::COLOR_BGR2BGRA);
    } else if (img.channels()== 1) {
        img.copyTo(gray);
    } else {
        cv::cvtColor(img, gray, cv::COLOR_BGR2BGRA);
    }

    cv::Mat lap, abs_lap;
	cv::Laplacian(gray,lap,CV_16S,3);


	cv::convertScaleAbs( lap, abs_lap );
    cv::Mat tmp_m, tmp_sd;
    double sd = 0;
    cv::meanStdDev(lap, tmp_m, tmp_sd);
    sd = tmp_sd.at<double>(0,0); // 方差
    return sd;

    // cv::Scalar lap_mean, lap_stddev;
	// cv::meanStdDev(lap,lap_mean,lap_stddev);

    // return tmp_sd.val[0];
}