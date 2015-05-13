#include "zmq.hpp"
#include "timestamp.h"
#include <iostream>
#include <sstream>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>

#define TIMES true

/**
 *  This program uses OpenCV to read and publish frames using ZeroMQ.
 *  Frames can be read using an ZeroMQ subscriber. Message are sent
 *  to subscribers in pairs: First a timestamp followed by the frame
 *  data. The frame data is sent as a vector of bytes.
 *
 *  Usage: ./framepub [cameraIndex]
 */

const int FRAME_BUF_SIZE = 10;
const int APPROX_FRAME_SIZE = 100000; // to avoid unnecessary mem realloc
const int NUM_IO_THREADS = 1;
const int HWM = 1; // high water mark

int cam_idx = 0; // camera device index

// .jpg CV_IMWRITE_JPEG_QUALITY (0-100) default 95
// .png CV_IMWRITE_PNG_COMPRESSION (0-9) default 3
std::string img_ext = ".jpg";
int jpg_quality = 60;

// Global camera thread variables
cv::Mat cam_frame[FRAME_BUF_SIZE];
cv::VideoCapture* video; 
int frame_buf_idx = 0;
timestamp_t frame_ts_buf[FRAME_BUF_SIZE];
bool frame_ready = false;
bool flg_sigint = false;

// sigint handler
void sigint_handler(int s) {
    flg_sigint = true;
    if (video != NULL) {
        video->release();
	//printf("releasing video\n");
    }
    exit(0);
}

// Camera thread
void* frame_reader(void* msg) {
    // Open camera for capture
    std::cout << "opening camera..." << std::endl;
    video = new cv::VideoCapture(cam_idx);
    if (video->isOpened()) {
        std::cout << "camera initialized." << std::endl;
    } else {
        std::cout << "problem opening camera." << std::endl;
        exit(1);
    }

#if TIMES
    timestamp_t s_time, e_time;
#endif
    
    cv::Mat frame;
    timestamp_t timestamp;
    bool frame_read;
    while (1) {
        if (flg_sigint) break;

#if TIMES
	    s_time = get_timestamp();
#endif
        // Read next frame from camera
	    frame_read = video->read(cam_frame[frame_buf_idx]);
        // Record time of retrieval
        timestamp = get_timestamp();
#if TIMES
    	e_time = get_timestamp();
	    //printf("retrieve: %llu", retrieve_time);
	    printf("%llu\t%llu\n", timestamp, e_time - s_time);
	    s_time = get_timestamp();
#endif

        if (!frame_read) continue;

        // Update global vars for parent thread
        frame_ts_buf[frame_buf_idx] = timestamp; 
        frame_buf_idx = (frame_buf_idx + 1) % FRAME_BUF_SIZE;
        frame_ready = true;
    }
}

int main (int argc, char* argv[]) {
    // Parse command line arguments
    if (argc > 1)
        cam_idx = atoi(argv[1]);

    // Initialize frame buffer
    for (int i = 0; i < FRAME_BUF_SIZE; i++) {
        //cam_frame[i] = new cv::Mat(APPROX_FRAME_SIZE);
        cam_frame[i] = *new cv::Mat();
    }

    // Create signal handler
    signal(SIGINT, sigint_handler);

    // Set compression quality
    std::vector<int> compression_params;
    compression_params.push_back(CV_IMWRITE_JPEG_QUALITY);
    compression_params.push_back(jpg_quality);

    // Start camera frame reader thread
    pthread_t cam_reader_thread;
    int rc = pthread_create(&cam_reader_thread, NULL, frame_reader, NULL);
    if (rc) {
        std::cout << "Failed to create camera thread" << std::endl;
        return 1;
    }

    //  Prepare our context and publisher
    zmq::context_t context(NUM_IO_THREADS);
    zmq::socket_t publisher(context, ZMQ_PUB);
    publisher.setsockopt(ZMQ_SNDHWM, &HWM, sizeof(HWM));
    publisher.bind("tcp://*:5563");

    std::cout << "publisher ready" << std::endl;

    bool frame_read;
    std::vector<uchar> frame_vector(APPROX_FRAME_SIZE);
    size_t frame_size;
    cv::Mat frame, gray;
    timestamp_t timestamp;
    while (1) {
        // Check if frame available from camera
        if (! frame_ready) continue;

        // Get frame and timestamp
        int prev_idx = (frame_buf_idx - 1) % FRAME_BUF_SIZE;
        if (prev_idx < 0) prev_idx = 9;
        frame = cam_frame[prev_idx].clone();
        timestamp = frame_ts_buf[prev_idx];
        frame_ready = false;
        
        // Convert to grayscale
        cv::cvtColor(frame, gray, CV_BGR2GRAY);

        // Compress frame
        cv::imencode(img_ext, gray, frame_vector, compression_params);

        // Frame size in bytes
        frame_size = sizeof(std::vector<uchar>) + sizeof(uchar) * frame_vector.size();

        // Create message with image contents
        zmq::message_t msg_frame(frame_size);
        memcpy(msg_frame.data(), frame_vector.data(), frame_size);

        // Create/send message with timestamp
        zmq::message_t msg_timestamp(sizeof(timestamp_t));
        memcpy(msg_timestamp.data(), &timestamp, sizeof(timestamp_t));

        // Send followup message with timestamp
        publisher.send(msg_timestamp, ZMQ_SNDMORE);
        publisher.send(msg_frame);
    }
    return 0;
}
