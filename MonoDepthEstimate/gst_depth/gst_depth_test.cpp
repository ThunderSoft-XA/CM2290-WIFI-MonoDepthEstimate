#include <stdio.h>
#include <thread>

#include "camera_pipe.hpp"
#include "appsrc_pipe.hpp"
#include "depth_esitimation.hpp"

#include "utils.hpp"

#define LEFT_IMG "./left1.png"
#define RIGHT_IMG "./right1.png"
#define DISP_IMG "./stereo_disp.png"

extern Queue<cv::Mat> rgb_mat_queue;


int main(int argc, char **argv) 
{
	std::string json_file = "./binocular_config.json";
    // if( parse_arg(argc, argv, json_file) != 0) {
	// 	std::cout << "invalid argument, exit !!" << std::endl;
	// 	return -1;
	// }

    /* Initialize GStreamer */
    gst_init (nullptr, nullptr);
    GMainLoop *main_loop;  /* GLib's Main Loop */
    main_loop = g_main_loop_new (NULL, FALSE);

    // constructot required class
    CameraPipe camera_pipe(json_file);
    DepthEstimation depth_esi(json_file);
    APPSrc2UdpSink push_pipe(json_file);

    cv::Mat left_mat,right_mat,disp_mat,depth_estimation_mat;

    if( camera_pipe.checkElements() != 0) {
        goto error;
    }

    // start depth estimation
    if( depth_esi.checkMember() != 0 ) {
        goto error;
    }

    if(push_pipe.checkElements() == -1) {
        goto error;
	}

    camera_pipe.setProperty();

    // left_mat = cv::imread(LEFT_IMG);
    // right_mat = cv::imread(RIGHT_IMG);

    push_pipe.setProperty();

    camera_pipe.runPipeline();

    std::thread([&](){
        bool first_appsrc = true;
        while( 1 ) {
            if(rgb_mat_queue.empty() || rgb_mat_queue.size() < 2) {
                continue;
            }
            left_mat = rgb_mat_queue.try_pop();
            right_mat =  rgb_mat_queue.try_pop();

            if( left_mat.empty() || right_mat.empty() ) {
                continue;
            }

            disp_mat = depth_esi.computeDepth(left_mat,right_mat);
            cv::imwrite(DISP_IMG,disp_mat);

            // depth_estimation_mat = cv::imread("../../right.jpg");
            printf("mat data size = %d\n", (int)disp_mat.total());
            if(first_appsrc) {
                push_pipe.runPipe(disp_mat);
                push_pipe.pushMatData(disp_mat);
                first_appsrc = false;
            } else {
                push_pipe.pushMatData(disp_mat);
            }

        }
    }).detach();

    g_main_loop_run (main_loop);

error:
    camera_pipe.~CameraPipe();
    push_pipe.~APPSrc2UdpSink();
    g_main_loop_unref (main_loop);
    return 0;
    
}