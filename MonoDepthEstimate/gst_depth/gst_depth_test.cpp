#include <stdio.h>
#include <unistd.h>
#include <thread>

#include "camera_pipe.hpp"
#include "appsrc_pipe.hpp"
#include "depth_esitimation.hpp"
#include "similarity.hpp"

#include "utils.hpp"

#define LEFT_IMG "./left1.png"
#define RIGHT_IMG "./right1.png"
#define DISP_IMG "./stereo_disp.png"

extern Queue<cv::Mat> rgb_mat_queue;


int main(int argc, char **argv) 
{
    fflush(stdout);
    setvbuf(stdout, NULL, _IONBF, 0);
    std::cout.rdbuf()->pubsetbuf( NULL, 0 );
	std::string json_file = "./monocular_config.json";
    // if( parse_arg(argc, argv, json_file) != 0) {
	// 	std::cout << "invalid argument, exit !!" << std::endl;
	// 	return -1;
	// }

    /* Initialize GStreamer */
    gst_init (nullptr, nullptr);
    GMainLoop *main_loop;  /* GLib's Main Loop */
    main_loop = g_main_loop_new (NULL, FALSE);

    // constructot required class
    Queue<cv::Mat> push_queue;
    CameraPipe camera_pipe(json_file);
    DepthEstimation depth_esi(json_file);
    APPSrc2UdpSink push_pipe(json_file);
    push_pipe.push_mat_queue = push_queue;

    cv::Mat left_mat,right_mat,disp_mat,depth_estimation_mat;

    if( camera_pipe.initPipe() == -1 || (push_pipe.initPipe() == -1) ) {
        std::cout << "init pipeline failed , exit" << std::endl;
        return -1;
    }

    std::cout << "init all pipeline finished" << std::endl;

    if( camera_pipe.checkElements() != 0 || 
        depth_esi.checkMember() != 0  || push_pipe.checkElements() != 0) {
        std::cout << "member or element check failed , exit" << std::endl;
        return -1;
    }

    std::cout << "all member or element check  finished" << std::endl;

    camera_pipe.setProperty();
    // left_mat = cv::imread(LEFT_IMG);
    // right_mat = cv::imread(RIGHT_IMG);
    push_pipe.setProperty();
    std::cout << "all pipeline's property set finished" << std::endl;

    camera_pipe.runPipeline();
    std::cout << "runing camera pipeline .............."<< __FILE__ << "!!!"  << std::endl;

#if 1
    std::thread([&](){
        int similarity_value;
        static bool first_run = true;
        cv::Mat left_temp, right_temp;
        while( 1 ) {
            if(rgb_mat_queue.empty() || rgb_mat_queue.size() < 2) {
                std::cout << "rgb_mat_queue is empty or size < 2 " << std::endl;
                continue;
            }
            if ( first_run) {
                left_mat = rgb_mat_queue.pop();
                right_mat =  rgb_mat_queue.pop();
                first_run = false;
            } else {
                right_mat =  rgb_mat_queue.pop();
            }


            if( left_mat.empty() || right_mat.empty() ) {
                std::cout << "rgb_mat is invalid " << std::endl;
                continue;
            }

            left_mat.copyTo(left_temp);
            right_mat.copyTo(right_temp);
            similarity_value = image_similarity(left_temp, right_temp);
            if( 10 > similarity_value && 6 < similarity_value) {
                std::cout << "similarity_value = " << similarity_value << ", runing computeDepth" << std::endl;
                disp_mat = depth_esi.computeDepth(left_mat,right_mat);
                push_pipe.push_mat_queue.push_back(disp_mat);
            } else if(similarity_value > 10) {
                    left_mat = right_mat;
            } else {
                continue;
            }

            // disp_mat = depth_esi.computeDepth(left_mat,right_mat);
            // // cv::imwrite(DISP_IMG,disp_mat);

            // push_pipe.push_mat_queue.push_back(disp_mat);

        }
    }).detach();
#else
    std::thread([&](){
        cv::Mat key_mat;
        int similarity_value = 0;
        while( 1 ) {
            if(rgb_mat_queue.empty() || rgb_mat_queue.size() < 2) {
                std::cout << "rgb_mat_queue is empty or size < 2 " << std::endl;
                continue;
            }
            if( key_mat.empty() != true) {
                left_mat = rgb_mat_queue.pop();
            } else {
                key_mat.copyTo(left_mat);
            }
            
            right_mat =  rgb_mat_queue.pop();

            if( left_mat.empty() || right_mat.empty() ) {
                std::cout << "rgb_mat is invalid " << std::endl;
                continue;
            }

            similarity_value = image_similarity(left_mat, right_mat);
            if( 10 > similarity_value && 6 < similarity_value) {
                disp_mat = depth_esi.computeDepth(left_mat,right_mat);
                push_pipe.push_mat_queue.push_back(disp_mat);
            } else if(similarity_value > 10) {
                if( variance_of_laplacian_cv(left_mat) >= 50 ) {
                    key_mat = left_mat;
                } else if ( variance_of_laplacian_cv(right_mat) >= 50 ) {
                    key_mat = right_mat;
                }
            } else {
                continue;
            }
            // cv::imwrite(DISP_IMG,disp_mat);

        }
    }).detach();
#endif

    push_pipe.updateCaps(push_pipe.push_mat_queue.pop().cols, push_pipe.push_mat_queue.pop().rows);
    std::cout << " push_queue size = " << push_pipe.push_mat_queue.size() << std::endl;
	std::thread([&](){
        sleep(2);
		push_pipe.runPipe();
    }).detach();

    g_main_loop_run (main_loop);

    camera_pipe.~CameraPipe();
    push_pipe.~APPSrc2UdpSink();
    g_main_loop_unref (main_loop);
    return 0;
    
}