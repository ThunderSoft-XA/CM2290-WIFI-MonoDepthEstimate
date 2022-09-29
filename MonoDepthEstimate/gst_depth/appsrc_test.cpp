#include <unistd.h>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <opencv2/opencv.hpp>

#include "appsrc_pipe.hpp"
#include "utils.hpp"

gint main (gint argc, gchar **argv)
{
	std::string json_file = "./binocular_config.json";
    // if( parse_arg(argc, argv, json_file) != 0) {
	// 	std::cout << "invalid argument, exit !!" << std::endl;
	// 	return -1;
	// }
	gst_init (nullptr, nullptr);
    GMainLoop *loop;
	loop = g_main_loop_new (NULL, FALSE);

    std::cout << "json_file : " << json_file << std::endl; 
	APPSrc2UdpSink push_pipe(json_file);

	if(push_pipe.checkElements() == -1) {
		return 0;
	}

	push_pipe.setProperty();

	cv::Mat depth_estimation_mat = cv::imread("./left.jpg");
	printf("mat data size = %d\n", (int)depth_estimation_mat.total());
	push_pipe.runPipe(depth_estimation_mat);
	// while (true)
	// {
	// 	push_pipe.pushMatData(depth_estimation_mat);
	// 	usleep(35);
	// }
	g_main_loop_run(loop);

	push_pipe.~APPSrc2UdpSink();
	g_main_loop_unref (loop);

	return 0;
}


