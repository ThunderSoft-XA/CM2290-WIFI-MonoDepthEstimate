#ifndef DEPTH_ESITIMATION_HPP
#define DEPTH_ESITIMATION_HPP

#include <iostream>
#include <fstream>
#include <stdio.h>
#include <sstream>

#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/core/utility.hpp"

#include "json/json.h"

using namespace cv;

enum { 
    STEREO_BM=0, 
    STEREO_SGBM=1, 
    STEREO_HH=2, 
    STEREO_VAR=3, 
    STEREO_3WAY=4, 
    STEREO_HH4=5 
};

class DepthEstimation {

public:
    DepthEstimation(std::string json_file);

    int checkMember();

    void undistortImg(cv::Mat &img1, cv::Mat &img2, cv::Size img_size,cv::Mat Q, cv::Rect roi1, cv::Rect roi2);
    cv::Mat computeDepth(cv::Mat &left_mat, cv::Mat &right_mat);

    void savePointCloude(cv::Mat &disp, cv::Mat &Q);

private:
    int alg = STEREO_SGBM;
    int SADWindowSize, numberOfDisparities;
    int sgbmWinSize;
    bool no_display;
    bool color_display;
    float disparity_multiplier = 1.0f;
    float scale;

    Ptr<StereoBM> bm = StereoBM::create(16,9);
    Ptr<StereoSGBM> sgbm = StereoSGBM::create(0,16,3);

    std::string intrinsic_filename = "";
    std::string extrinsic_filename = "";
    std::string disparity_filename = "";
    std::string point_cloud_filename = "";

};

#endif //DEPTH_ESITIMATION_HPP