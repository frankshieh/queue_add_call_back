#include <iostream>
#include <queue>
#include <chrono>

// Inludes common necessary includes for development using depthai library
#include "depthai/depthai.hpp"

using namespace std;
using namespace std::chrono;
using namespace std::literals; // enables the usage of 24h, 1ms, 1s instead of

struct callbackType {
	std::string name;
	cv::Mat frame;
	std::chrono::steady_clock::time_point t;
};
// global
// frame status
std::chrono::steady_clock::time_point lPrevTime, rPrevTime;
int lLostFrames = 0, rLostFrames = 0;
int lLost2Frames = 0, rLost2Frames = 0;
int lLost3Frames = 0, rLost3Frames = 0;
int startFlag = 0;

int main() {
	// Create pipeline
	dai::Pipeline pipeline;

	// Add all three cameras
	// auto camRgb = pipeline.create<dai::node::ColorCamera>();
	auto left = pipeline.create<dai::node::MonoCamera>();
	auto right = pipeline.create<dai::node::MonoCamera>();

	// Create XLink output
	auto xout = pipeline.create<dai::node::XLinkOut>();
	xout->setStreamName("frames");

	// Properties
	// camRgb->setPreviewSize(300, 300);
	left->setBoardSocket(dai::CameraBoardSocket::LEFT);
	left->setResolution(dai::MonoCameraProperties::SensorResolution::THE_400_P);
	right->setBoardSocket(dai::CameraBoardSocket::RIGHT);
	right->setResolution(dai::MonoCameraProperties::SensorResolution::THE_400_P);
	left->setFps(110);
	right->setFps(110);


	// Stream all the camera streams through the same XLink node
	// camRgb->preview.link(xout->input);
	left->out.link(xout->input);
	right->out.link(xout->input);

	auto queue = std::queue<callbackType>();
	auto queueBuf = std::queue<callbackType>();
	std::mutex queueMtx;

	// Connect to device and start pipeline
	dai::Device device(pipeline);
	device.setXLinkChunkSize(0);	

	auto newFrame = [&queueMtx, &queue](std::shared_ptr<dai::ADatatype> callback) {
		if (dynamic_cast<dai::ImgFrame*>(callback.get()) != nullptr) {
			std::unique_lock<std::mutex> lock(queueMtx);
			callbackType cb;
			dai::ImgFrame* imgFrame = static_cast<dai::ImgFrame*>(callback.get());
			auto num = imgFrame->getInstanceNum();
			auto t1 = imgFrame->getTimestamp();
			cb.name = num == 0 ? "color" : (num == 1 ? "left" : "right");
			cb.frame = imgFrame->getCvFrame();
			cb.t = t1;
			queue.push(cb);
			if (queue.size() > 1000) // keep only 1000 frames
				queue.pop();
			if (0 == startFlag) {
				lPrevTime = t1;
				rPrevTime = t1;
				startFlag = 1;
			}
			if (num == 1) {  // left
				if ((t1 - lPrevTime) / 1ms > 10) lLostFrames++;
				if ((t1 - lPrevTime) / 1ms > 18) lLost2Frames++;
				if ((t1 - lPrevTime) / 1ms > 27) lLost3Frames++;
				lPrevTime = t1;
			}
			if (num == 2) {  // right
				if ((t1 - rPrevTime) / 1ms > 10) rLostFrames++;
				if ((t1 - rPrevTime) / 1ms > 18) rLost2Frames++;
				if ((t1 - rPrevTime) / 1ms > 27) rLost3Frames++;
				rPrevTime = t1;
			}
				
		}
	};

	// Add callback to the output queue "frames" for all newly arrived frames (color, left, right)
	device.getOutputQueue("frames", 4, false)->addCallback(newFrame);
	
	while (queueBuf.size()<1000) {
		callbackType data;
		{
			std::unique_lock<std::mutex> lock(queueMtx);
			if (!queue.empty() ) {  // get new data if space pressed
				data = queue.front();
				queueBuf.push(data);
				queue.pop();
			}
		}

		if (!data.frame.empty()) {
			cv::imshow(data.name.c_str(), data.frame);
		}
	}
	std::chrono::steady_clock::time_point StartTime, FrameTime;
	StartTime = queueBuf.front().t;
	cout << "Press space for next, 'q' to quit" << endl;
	cout << "R lost frames:" << rLostFrames << endl;
	cout << "L lost frames:" << lLostFrames << endl;
	cout << "R lost >=2 frames:" << rLost2Frames << endl;
	cout << "L lost >=2 frames:" << lLost2Frames << endl;
	cout << "R lost >=3 frames:" << rLost3Frames << endl;
	cout << "L lost >=3 frames:" << lLost3Frames << endl;
 	while (queueBuf.size() >0) {
		callbackType data;

		int key = cv::waitKey(1);
		if (key == 'q' || key == 'Q') {
			return 0;
		}
		if (' '==key) {
			if (!queueBuf.empty()) {  // get new data if space pressed
				data = queueBuf.front();
				queueBuf.pop();

				FrameTime = data.t;
				if (data.name == "left") {
					cout << "Frame:" << data.name.c_str() << endl;
					cout << "Time:" << (FrameTime - StartTime) / 1ms << "ms! " << endl;
				}
				else {
					cout << "               Frame:" << data.name.c_str() << endl;
					cout << "               Time:" << (FrameTime - StartTime) / 1ms << "ms!" << endl;
				}
			}
		}

		if (!data.frame.empty()) {
			cv::imshow(data.name.c_str(), data.frame);
		}
	}
 	return 0;
}
