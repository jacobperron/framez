#include "zmq.hpp"
#include "timestamp.h"
#include <iostream>
#include <opencv2/highgui/highgui.hpp>

#define SHOW_FEED true 
#define TIMES true 

const int NUM_IO_THREADS = 4;

int main (int argc, char* argv[]) {
    std::string hostname = "localhost";
    std::string port = "5563";

    // Parse command line args
    if (argc > 1)
	hostname = std::string(argv[1]);
    if (argc > 2)
	port = std::string(argv[2]);

    std::string connection_info = "tcp://" + hostname + ":" + port;

    //  Prepare our context and subscriber
    std::cout << "preparing subscriber" << std::endl;
    zmq::context_t context(NUM_IO_THREADS);
    zmq::socket_t subscriber (context, ZMQ_SUB);
    std::cout << "connecting to " << hostname
	      << " on port " << port << std::endl;    
    subscriber.connect(connection_info.c_str());

    // Subscribe to specific topic(s)
    // empty, zero length means subscribe to all messages
    subscriber.setsockopt( ZMQ_SUBSCRIBE, "", 0);
    std::cout << "ready to recieve messages" << std::endl;

    cv::Mat* frame;
    timestamp_t timestamp;
#if TIMES
    timestamp_t s_time, e_time, prev_timestamp;
    prev_timestamp = 0;
#endif

    while (1) {
        zmq::message_t msg_timestamp;
        zmq::message_t msg_frame;
#if TIMES
    	s_time = get_timestamp();
#endif
        // Expect two messages: timestamp followed by image data
        // Read timestamp
        subscriber.recv(&msg_timestamp);//atof(s_recv(subscriber).c_str());
        timestamp = *static_cast<timestamp_t*>(msg_timestamp.data());

        //  Read message contents
        subscriber.recv(&msg_frame); // blocks until message received
#if TIMES
	    e_time = get_timestamp();
	    printf("received_msg: %llu", (e_time - s_time));
	    //printf("%llu\t%llu", timestamp, (e_time - s_time));
#endif
    	std::vector<uchar> frame_vector(msg_frame.size());
        std::memcpy(frame_vector.data(), msg_frame.data(), msg_frame.size());

        cv::Mat encoded_frame(frame_vector, true);
#if TIMES
	    printf("\tdata_size: %lu", msg_frame.size());
	    //printf("\t%lu", msg_frame.size());
#endif
        frame = new cv::Mat(cv::imdecode(frame_vector, 0));

#if SHOW_FEED
        cv::imshow("out", *frame);
        cv::waitKey(1);
#endif

#if TIMES
	    s_time = get_timestamp();
        // Assumes publisher and subscriber are time synchronized
	    printf("\tdiff_timestamp: %llu\n", s_time - timestamp); 
	    //printf("\t%llu\t%llu\n", s_time - timestamp); 
        prev_timestamp = timestamp;
#endif
        delete frame;
        frame = 0;
    }
    return 0;
}
