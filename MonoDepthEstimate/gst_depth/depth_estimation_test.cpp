#include "depth_esitimation.hpp"
#include "utils.hpp"

#define LEFT_IMG "./left1.png"
#define RIGHT_IMG "./right1.png"
#define DISP_IMG "./stereo_disp.png"

int main(int argc, char **argv)
{
	std::string json_file = "./binocular_config.json";
    // if( parse_arg(argc, argv, json_file) != 0) {
	// 	std::cout << "invalid argument, exit !!" << std::endl;
	// 	return -1;
	// }
    DepthEstimation depth_esi(json_file);

    if( depth_esi.checkMember() != 0 ) {
        return -1;
    }

    cv::Mat left_mat = cv::imread(LEFT_IMG);
    cv::Mat right_mat = cv::imread(RIGHT_IMG);

    cv::Mat disp_mat = depth_esi.computeDepth(left_mat,right_mat);

    cv::imwrite(DISP_IMG,disp_mat);
    
}