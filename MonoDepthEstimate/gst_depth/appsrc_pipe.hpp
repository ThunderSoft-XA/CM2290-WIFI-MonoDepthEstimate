#ifndef APPSRC_PIPE_HPP
#define APPSRC_PIPE_HPP

#include <stdlib.h>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <opencv2/opencv.hpp>

#include "json/json.h"

class APPSrc2UdpSink {

private: 
	GstElement *pipeline, *appsrc, *videoconvert, *qtic2venc, *h264parse, *rtph264pay, *udpsink;
	GstAppSrcCallbacks cbs;
	/* Pointer to hold bus address */
	GstBus *bus = nullptr;

	std::string pipe_name;
	int width, height;
	std::string host = "127.0.0.1";
	int port = 5000;

public:
	APPSrc2UdpSink(std::string json_file);

	int checkElements();

	void setProperty();

	int runPipe(cv::Mat &depth_estimation_mat);
	bool pushMatData(cv::Mat &mat_data);
	~APPSrc2UdpSink();

};

#endif //APPSRC_PIPE_HPP