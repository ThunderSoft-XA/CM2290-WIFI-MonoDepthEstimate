/**
 * @file depth_estimation.cpp
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-09-26
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "depth_esitimation.hpp"

#define ALGORITHM "bm"
#define BLOCK_SIZE 7
#define MAX_DISPARITY 32

static void print_help()
{
    printf("\nDemo stereo matching converting L and R images into disparity and point clouds\n");
    printf("\nUsage: <left_image> <right_image> [--algorithm=bm|sgbm|hh|hh4|sgbm3way] [--blocksize=<block_size>]\n"
           "[--max-disparity=<max_disparity>] [--scale=scale_factor>] [-i=<intrinsic_filename>] [-e=<extrinsic_filename>]\n"
           "[--no-display] [--color] [-o=<disparity_image>] [-p=<point_cloud_file>]\n");
}

static void saveXYZ(const char* filename, const Mat& mat)
{
    const double max_z = 1.0e4;
    FILE* fp = fopen(filename, "wt");
    for(int y = 0; y < mat.rows; y++)
    {
        for(int x = 0; x < mat.cols; x++)
        {
            Vec3f point = mat.at<Vec3f>(y, x);
            if(fabs(point[2] - max_z) < FLT_EPSILON || fabs(point[2]) > max_z) continue;
            fprintf(fp, "%f %f %f\n", point[0], point[1], point[2]);
        }
    }
    fclose(fp);
}

DepthEstimation::DepthEstimation(std::string json_file)
{
    Json::Value root;
    std::ifstream ifs;
    ifs.open(json_file);

    Json::CharReaderBuilder builder;
    builder["collectComments"] = true;
    JSONCPP_STRING errs;
    if (!parseFromStream(builder, ifs, &root, &errs)) {
        std::cout << errs << std::endl;
    }
    std::cout << "parse json file " << json_file << " successfully in "<< __FUNCTION__ << std::endl;
    ifs.close();
    
    int size = root.size();   // count of root node
    for (int index = 0; index < size; index++) {
    
        const Json::Value alg_obj = root["estimation"];

        if (alg_obj.isMember("algorithm")) {
            std::string _alg = alg_obj["algorithm"].asString();
            this->alg = _alg == "bm" ? STEREO_BM :
                _alg == "sgbm" ? STEREO_SGBM :
                _alg == "hh" ? STEREO_HH :
                _alg == "var" ? STEREO_VAR :
                _alg == "hh4" ? STEREO_HH4 :
                _alg == "sgbm3way" ? STEREO_3WAY : -1;
        }
        if (alg_obj.isMember("blocksize")) {
            this->SADWindowSize  = alg_obj["blocksize"].asInt();
        }
        sgbmWinSize = SADWindowSize > 0 ? SADWindowSize : 3;
        if (alg_obj.isMember("max_disparity")) {
            this->numberOfDisparities  = alg_obj["max_disparity"].asInt();
        }
        if (alg_obj.isMember("no_display")) {
            this->no_display  = alg_obj["no_display"].asBool();
        }
        if (alg_obj.isMember("scale")) {
            this->scale  = alg_obj["scale"].asFloat();
        }
        if (alg_obj.isMember("color")) {
            this->color_display  = alg_obj["color"].asBool();
        }
        if (alg_obj.isMember("intrinsic_filename")) {
            this->intrinsic_filename  = alg_obj["intrinsic_filename"].asString();
        }
        if (alg_obj.isMember("extrinsic_filename")) {
            this->extrinsic_filename  = alg_obj["extrinsic_filename"].asString();
        }
        if (alg_obj.isMember("disparity_filename")) {
            this->disparity_filename  = alg_obj["disparity_filename"].asString();
        }
        if (alg_obj.isMember("point_cloud_filename")) {
            this->point_cloud_filename  = alg_obj["point_cloud_filename"].asString();
        }
    }
}


int DepthEstimation::checkMember() {
    if( alg < 0 ) {
        printf("Command-line parameter error: Unknown stereo algorithm\n\n");
        print_help();
        return -1;
    }
    if ( numberOfDisparities < 1 || numberOfDisparities % 16 != 0 ) {
        printf("Command-line parameter error: The max disparity (--maxdisparity=<...>) must be a positive integer divisible by 16\n");
        print_help();
        return -1;
    }
    if (scale < 0)
    {
        printf("Command-line parameter error: The scale factor (--scale=<...>) must be a positive floating-point number\n");
        return -1;
    }
    if (SADWindowSize < 1 || SADWindowSize % 2 != 1)
    {
        printf("Command-line parameter error: The block size (--blocksize=<...>) must be a positive odd number\n");
        return -1;
    }
    if( (!intrinsic_filename.empty()) ^ (!extrinsic_filename.empty()) )
    {
        printf("Command-line parameter error: either both intrinsic and extrinsic parameters must be specified, or none of them (when the stereo pair is already rectified)\n");
        return -1;
    }

    if( extrinsic_filename.empty() && !point_cloud_filename.empty() )
    {
        FileStorage fs_in(intrinsic_filename, FileStorage::READ);
        if(!fs_in.isOpened())
        {
            printf("Failed to open file %s\n", intrinsic_filename.c_str());
            return -1;
        }

        FileStorage fs_ext(extrinsic_filename, FileStorage::READ);
        if(!fs_ext.isOpened())
        {
            printf("Failed to open file %s\n", extrinsic_filename.c_str());
            return -1;
        }
        printf("Command-line parameter error: extrinsic and intrinsic parameters must be specified to compute the point cloud\n");
        return -1;
    } 
    return 0;
}


void DepthEstimation::undistortImg(cv::Mat &img1, cv::Mat &img2, cv::Size img_size,cv::Mat Q, cv::Rect roi1, cv::Rect roi2)
{
    // reading intrinsic parameters
    FileStorage fs(intrinsic_filename, FileStorage::READ);

    Mat M1, D1, M2, D2;
    fs["M1"] >> M1;
    fs["D1"] >> D1;
    fs["M2"] >> M2;
    fs["D2"] >> D2;

    M1 *= scale;
    M2 *= scale;

    fs.open(extrinsic_filename, FileStorage::READ);

    Mat R, T, R1, P1, R2, P2;
    fs["R"] >> R;
    fs["T"] >> T;

    stereoRectify( M1, D1, M2, D2, img_size, R, T, R1, R2, P1, P2, Q, CALIB_ZERO_DISPARITY, -1, img_size, &roi1, &roi2 );

    Mat map11, map12, map21, map22;
    initUndistortRectifyMap(M1, D1, R1, P1, img_size, CV_16SC2, map11, map12);
    initUndistortRectifyMap(M2, D2, R2, P2, img_size, CV_16SC2, map21, map22);

    Mat img1r, img2r;
    remap(img1, img1r, map11, map12, INTER_LINEAR);
    remap(img2, img2r, map21, map22, INTER_LINEAR);

    img1 = img1r;
    img2 = img2r;
}


void DepthEstimation::savePointCloude(cv::Mat &disp, cv::Mat &Q)
{
    printf("storing the point cloud...");
    fflush(stdout);
    Mat xyz;
    Mat floatDisp;
    disp.convertTo(floatDisp, CV_32F, 1.0f / disparity_multiplier);
    reprojectImageTo3D(floatDisp, xyz, Q, true);
    saveXYZ(point_cloud_filename.c_str(), xyz);
}


cv::Mat DepthEstimation::computeDepth(cv::Mat &left_mat, cv::Mat &right_mat)
{

    Mat img1 = left_mat;
    Mat img2 = right_mat;

    CV_Assert(img1.empty() != true);
    CV_Assert(img2.empty() != true);

    // if(alg == STEREO_BM) {
    Mat temp1, temp2;
    cv::cvtColor(img1, temp1, COLOR_RGB2GRAY);
    img1 = temp1;
    cv::cvtColor(img2, temp2, COLOR_RGB2GRAY);
    img2 = temp2;
    // }

    if (scale != 1.f)
    {
        Mat temp1, temp2;
        int method = scale < 1 ? INTER_AREA : INTER_CUBIC;
        resize(img1, temp1, Size(), scale, scale, method);
        img1 = temp1;
        resize(img2, temp2, Size(), scale, scale, method);
        img2 = temp2;
    }

    Size img_size = img1.size();

    Rect roi1, roi2;
    Mat Q;

    if( !intrinsic_filename.empty() )
    {
        undistortImg(img1, img2, img_size, Q, roi1, roi2);
    }

    numberOfDisparities = numberOfDisparities > 0 ? numberOfDisparities : ((img_size.width/8) + 15) & -16;

    bm->setROI1(roi1);
    bm->setROI2(roi2);
    bm->setPreFilterCap(31);
    bm->setBlockSize(SADWindowSize > 0 ? SADWindowSize : 9);
    bm->setMinDisparity(0);
    bm->setNumDisparities(numberOfDisparities);
    bm->setTextureThreshold(10);
    bm->setUniquenessRatio(15);
    bm->setSpeckleWindowSize(100);
    bm->setSpeckleRange(32);
    bm->setDisp12MaxDiff(1);

    sgbm->setPreFilterCap(63);
    sgbm->setBlockSize(sgbmWinSize);

    int cn = img1.channels();

    sgbm->setP1(8*cn*sgbmWinSize*sgbmWinSize);
    sgbm->setP2(32*cn*sgbmWinSize*sgbmWinSize);
    sgbm->setMinDisparity(0);
    sgbm->setNumDisparities(numberOfDisparities);
    sgbm->setUniquenessRatio(10);
    sgbm->setSpeckleWindowSize(100);
    sgbm->setSpeckleRange(32);
    sgbm->setDisp12MaxDiff(1);
    if(alg==STEREO_HH)
        sgbm->setMode(StereoSGBM::MODE_HH);
    else if(alg==STEREO_SGBM)
        sgbm->setMode(StereoSGBM::MODE_SGBM);
    else if(alg==STEREO_HH4)
        sgbm->setMode(StereoSGBM::MODE_HH4);
    else if(alg==STEREO_3WAY)
        sgbm->setMode(StereoSGBM::MODE_SGBM_3WAY);

    Mat disp, disp8;
    //Mat img1p, img2p, dispp;
    //copyMakeBorder(img1, img1p, 0, 0, numberOfDisparities, 0, IPL_BORDER_REPLICATE);
    //copyMakeBorder(img2, img2p, 0, 0, numberOfDisparities, 0, IPL_BORDER_REPLICATE);

    int64 t = getTickCount();
    if( alg == STEREO_BM )
    {
        bm->compute(img1, img2, disp);
        if (disp.type() == CV_16S)
            disparity_multiplier = 16.0f;
    }
    else if( alg == STEREO_SGBM || alg == STEREO_HH || alg == STEREO_HH4 || alg == STEREO_3WAY )
    {
        sgbm->compute(img1, img2, disp);
        if (disp.type() == CV_16S)
            disparity_multiplier = 16.0f;
    }
    t = getTickCount() - t;
    printf("Time elapsed: %fms\n", t*1000/getTickFrequency());

    //disp = dispp.colRange(numberOfDisparities, img1p.cols);
    if( alg != STEREO_VAR )
        disp.convertTo(disp8, CV_8U, 255/(numberOfDisparities*16.));
    else
        disp.convertTo(disp8, CV_8U);

    Mat disp8_3c;
    if (color_display) {
        cv::applyColorMap(disp8, disp8_3c, cv::ColormapTypes::COLORMAP_HOT);
        disp = disp8_3c;
    }


    if(!disparity_filename.empty())
        imwrite(disparity_filename, color_display ? disp8_3c : disp8);

    if(!point_cloud_filename.empty())
    {
        savePointCloude(disp, Q);
    }
    return disp;
}