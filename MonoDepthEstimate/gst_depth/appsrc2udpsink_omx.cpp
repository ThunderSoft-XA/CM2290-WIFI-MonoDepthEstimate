#include "appsrc_pipe.hpp"

static int caps_width;
static int caps_height;

static void cb_need_data(GstAppSrc *appsrc, guint unused_size, gpointer user_data)
{
	g_print("In %s\n", __func__);
	static GstClockTime timestamp = 0;
	GstBuffer *buffer;
	guint size;
	GstFlowReturn ret;
	GstMapInfo map;

	size = caps_width * caps_height * 3;

	buffer = gst_buffer_new_allocate (NULL, size, NULL);

	/* this makes the image black/white */
	// gst_buffer_memset (buffer, 0, white ? 0xff : 0x0, size);
	// gst_buffer_memset (buffer, 0, *data, size);

//这两个方法都可以
#if 0
	g_print("In %s: %i\n", __FILE__,__LINE__);
    gst_buffer_fill(buffer, 0, &user_data, size);
#else
    gst_buffer_map (buffer, &map, GST_MAP_WRITE);
	g_print("In %s: %i\n", __FILE__,__LINE__);
	g_print("map.data address = %p\n",map.data);
	g_print("user_data address = %p\n",user_data);
	g_print("size = %d\n", size);//gst_buffer_get_size(buffer)
    memcpy( (guchar *)map.data, (guchar *)user_data, size );

	gst_buffer_unmap (buffer, &map);	//解除映射
#endif

	GST_BUFFER_PTS (buffer) = timestamp;
	GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, 2);

	timestamp += GST_BUFFER_DURATION (buffer);

	g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);
	// ret = gst_app_src_push_buffer(appsrc, buffer);
	gst_buffer_unref (buffer);	//释放资源

	if (ret != GST_FLOW_OK) {
		/* something wrong, stop pushing */
		// g_main_loop_quit (loop);
	}
}

static void cb_enough_data(GstAppSrc *src, gpointer user_data)
{
	g_print("In %s\n", __func__);
}

static gboolean cb_seek_data(GstAppSrc *src, guint64 offset, gpointer user_data)
{
	g_print("In %s\n", __func__);
	return TRUE;
}

static gboolean cb_bus (GstBus *bus, GstMessage *message, gpointer data) 
{
	g_print("Handle gstreamer pipeline bus message\n");
	GstElement *pipeline = (GstElement *)(data);
	switch (GST_MESSAGE_TYPE (message)) {
	case GST_MESSAGE_ERROR: {
		GError *err;
		gchar *debug;

		gst_message_parse_error (message, &err, &debug);
		g_print ("Error: %s\n", err->message);
		g_error_free (err);
		g_free (debug);

		gst_element_set_state(pipeline,GST_STATE_READY);
		break;
	} case GST_MESSAGE_EOS:
		/* end-of-stream */
		gst_element_set_state(pipeline,GST_STATE_NULL);
		break;
	default:
	/* unhandled message */
		break;
	}
	/* we want to be notified again the next time there is a message
	* on the bus, so returning TRUE (FALSE means we want to stop watching
	* for messages on the bus and our callback should not be called again)
	*/
	return TRUE;
}


APPSrc2UdpSink::APPSrc2UdpSink(std::string json_file) 
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
    
        const Json::Value streams_obj = root["gstreamer"]["streams"];
        int streams_size = streams_obj.size();

        for(int streams_index = 0; streams_index < streams_size; streams_index++) {
            if (streams_obj[streams_index].isMember("pipe_name")) {
                this->pipe_name = streams_obj[streams_index]["pipe_name"].asString();
            }
            if (streams_obj[streams_index].isMember("host")) {
                this->host = streams_obj[streams_index]["host"].asString();
            }
            if (streams_obj[streams_index].isMember("port")) {
                this->port = streams_obj[streams_index]["port"].asInt();
            }
        }
    }
/**
 * @brief appsrc ! videoconvert ! nvvideoconvert ! nvv4l2h264enc ! h264parse ! rtph264pay ! udpsink host=127.0.0.1 port=6000
 *  https://blog.csdn.net/Collin_D/article/details/108177398
 */

    pipeline = gst_pipeline_new (this->pipe_name.c_str());
    appsrc = gst_element_factory_make ("appsrc", "source");
	parse = gst_element_factory_make("h264parse", "parse");
	decoder = gst_element_factory_make("avdec_h264", "decoder");
	scale = gst_element_factory_make("videoscale", "scale");
	filter1 = gst_element_factory_make("capsfilter", "filter1");
	conv = gst_element_factory_make("videoconvert", "conv");
	encoder = gst_element_factory_make("omxh264enc", "encoder");
	filter2 = gst_element_factory_make("capsfilter", "filter2");
	rtppay = gst_element_factory_make("rtph264pay", "rtppay");
	udpsink = gst_element_factory_make("udpsink", "sink");
}


int APPSrc2UdpSink::checkElements() 
{
    if (!this->pipeline) {
        std::cout << "element pipeline could be created." << std::endl;
        return -1;
    }
    if (!this->appsrc) {
        std::cout << "element appsrc could be created." << std::endl;
        return -1;
    }
    if (!this->parse) {
        std::cout << "element h264parse could be created." << std::endl;
        return -1;
    }    
    if (!this->decoder) {
        std::cout << "element avdec_h264 could be created." << std::endl;
        return -1;
    }    
    if (!this->scale) {
        std::cout << "element videoscale could be created." << std::endl;
        return -1;
    }    
    if (!this->filter1) {
        std::cout << "element capsfilter1 could be created." << std::endl;
        return -1;
    }    
    if (!this->conv) {
        std::cout << "element videoconvert could be created." << std::endl;
        return -1;
    }    
    if (!this->encoder) {
        std::cout << "element omxh264enc could be created." << std::endl;
        return -1;
    }
    if (!this->filter2) {
        std::cout << "element capsfilter2  could be created." << std::endl;
        return -1;
    }
    if (!this->rtppay) {
        std::cout << "element rtph264pay could be created." << std::endl;
        return -1;
    }
    if (!this->udpsink) {
        std::cout << "element udpsink could be created." << std::endl;
        return -1;
    }
    return 0;
}

void APPSrc2UdpSink::setProperty() 
{
    /* setup */
    caps_width = DEFAULT_WIDTH;
    caps_height = DEFAULT_HEIGHT;
    // g_object_set (G_OBJECT (appsrc), "caps",
    //     gst_caps_new_simple ("video/x-raw",
    //         "format", G_TYPE_STRING, "RGB",
    //         "width", G_TYPE_INT, DEFAULT_WIDTH,
    //         "height", G_TYPE_INT, DEFAULT_HEIGHT,
    //         "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
    //         "framerate", GST_TYPE_FRACTION, 30, 1,
    //         NULL),
    //     NULL);
    // g_object_set (G_OBJECT (appsrc),
    //     "stream-type", 0,
    //     "is-live", TRUE,
    //     "format", GST_FORMAT_TIME, NULL);

	g_object_set(G_OBJECT(filter1), "caps", 
        gst_caps_new_simple("video/x-raw",
            "width", G_TYPE_INT, DEFAULT_WIDTH,
            "height", G_TYPE_INT, DEFAULT_HEIGHT,
            NULL), 
        NULL);
	g_object_set(G_OBJECT(filter2), "caps", 
        gst_caps_new_simple("video/x-h264",
            "stream-format", G_TYPE_STRING, "byte-stream",
            NULL), 
        NULL);

	g_object_set(udpsink, "host", this->host.c_str(), NULL);
	g_object_set(udpsink, "port",  this->port, NULL);
	g_object_set(udpsink, "sync", false, NULL);
	g_object_set(udpsink, "async", false, NULL);

    cbs.need_data = cb_need_data;
    cbs.enough_data = cb_enough_data;
    cbs.seek_data = cb_seek_data;
}


int APPSrc2UdpSink::runPipe(cv::Mat &depth_estimation_mat)
{
    assert(depth_estimation_mat.empty() != true);
    if( (depth_estimation_mat.cols != DEFAULT_WIDTH) || (depth_estimation_mat.rows != DEFAULT_HEIGHT)) {
        /* update caps */
        caps_width = depth_estimation_mat.cols;
        caps_height = depth_estimation_mat.rows;
        g_object_set (G_OBJECT (appsrc), "caps",
        gst_caps_new_simple ("video/x-raw",
            "format", G_TYPE_STRING, "RGB",
            "width", G_TYPE_INT, caps_width,
            "height", G_TYPE_INT, caps_height,
            "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
            "framerate", GST_TYPE_FRACTION, 30, 1,
            NULL), NULL);
    }
    /* TEST WITH udpsink */
    /* NOT WORKING AS EXPECTED, DISPLAYING A BLURRY VIDEO */
    gst_bin_add_many (GST_BIN (pipeline), appsrc, parse, decoder, scale,
        filter1, conv, encoder, filter2, rtppay, udpsink, NULL);

    if (gst_element_link_many (appsrc, conv, parse, decoder, scale,
        filter1, conv, encoder, filter2, rtppay, udpsink, NULL)!= TRUE) {
            g_printerr ("Elements could not be linked.\n");
            gst_object_unref (pipeline);
            return -1;
    }
    // gst_app_src_set_callbacks(GST_APP_SRC_CAST(appsrc), &cbs, depth_estimation_mat.data, NULL);

    bus = gst_element_get_bus(pipeline);
    gst_bus_add_signal_watch (bus);
    g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)cb_bus, this);
    gst_object_unref(bus);

    /* play */
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    return 0;
}

bool APPSrc2UdpSink::pushMatData(cv::Mat &mat_data)
{
	g_print("In %s\n", __func__);
	static GstClockTime timestamp = 0;
	GstBuffer *buffer;
	guint size;
	GstFlowReturn ret;
	GstMapInfo map;

	size = caps_width * caps_height * 3;

	buffer = gst_buffer_new_allocate (NULL, size, NULL);

	/* this makes the image black/white */
	// gst_buffer_memset (buffer, 0, white ? 0xff : 0x0, size);
	// gst_buffer_memset (buffer, 0, *data, size);

//这两个方法都可以
#if 0
	g_print("In %s: %i\n", __FILE__,__LINE__);
    gst_buffer_fill(buffer, 0, &user_data, size);
#else
    gst_buffer_map (buffer, &map, GST_MAP_WRITE);

	g_print("size = %d\n", size);//gst_buffer_get_size(buffer)
    memcpy( (guchar *)map.data, (guchar *)mat_data.data, size );

	gst_buffer_unmap (buffer, &map);	//解除映射
#endif

	GST_BUFFER_PTS (buffer) = timestamp;
	GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, 2);

	timestamp += GST_BUFFER_DURATION (buffer);

	g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);
	// ret = gst_app_src_push_buffer(appsrc, buffer);
	gst_buffer_unref (buffer);	//释放资源

	if (ret != GST_FLOW_OK) {
		/* something wrong, stop pushing */
		// g_main_loop_quit (loop);
	}

    return true;

}


APPSrc2UdpSink::~APPSrc2UdpSink()
{
    /* clean up */
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}