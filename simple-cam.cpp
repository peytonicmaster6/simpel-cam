/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2022, Peyton Howe
 *
 * A simple dual-camera libcamera capture program
 */

#include <iomanip>
#include <iostream>
#include <string> 
#include <memory>
#include <boost/lexical_cast.hpp>

#include "event_loop.h"
#include "preview.h"


struct options
{
	int dual_cameras;
	unsigned int width;
	unsigned int height;
	unsigned int prev_x, prev_y, prev_width, prev_height;
	float fps;
	float shutterSpeed;
	std::string exposure;
	int exposure_index;
	int timeout;
	int buffer_count;
};

std::unique_ptr<options> options_;
int cam_exposure_index;
uint64_t last_timestamp_ = 0;

using namespace libcamera;
static std::shared_ptr<Camera> camera;
static std::shared_ptr<Camera> camera2;
static EventLoop loop;

static void processRequest(Request *request);
static void processRequest2(Request *request);

static void requestComplete(Request *request)
{
	if (request->status() == Request::RequestCancelled)
		return;
	loop.callLater(std::bind(&processRequest, request));
}

static void requestComplete2(Request *request)
{
    if (request->status() == Request::RequestCancelled)
		return;
	loop.callLater(std::bind(&processRequest2, request));
}

static void processRequest(Request *request)
{
	//float framerate = 0;
	//auto ts = request->metadata().get(controls::SensorTimestamp);
	//uint64_t timestamp = ts ? *ts : request->buffers().begin()->second->metadata().timestamp;
	//if (last_timestamp_ == 0 || last_timestamp_ == timestamp)
		//framerate = 0;
	//else
		//framerate = 1e9 / (timestamp - last_timestamp_);
	//last_timestamp_ = timestamp;
	
	////if (framerate < 55)
	//std::cout << "Cam1 fps: " << framerate << '\n';
	
	const Request::BufferMap &buffers = request->buffers();
	for (auto bufferPair : buffers) {
		const Stream *stream = bufferPair.first;
		FrameBuffer *buffer = bufferPair.second;
		StreamConfiguration const &cfg = stream->configuration();
		int fd = buffer->planes()[0].fd.get();
		
		makeBuffer(fd, cfg, buffer, 1);
	}
	
	/* Re-queue the Request to the camera. */	
	request->reuse(Request::ReuseBuffers);
	camera->queueRequest(request);
}

static void processRequest2(Request *request)
{	
	const Request::BufferMap &buffers2 = request->buffers();
	for (auto bufferPair : buffers2) {
		const Stream *stream = bufferPair.first;
		FrameBuffer *buffer2 = bufferPair.second;
		StreamConfiguration const &cfg2 = stream->configuration();
		int fd2 = buffer2->planes()[0].fd.get();
		
		makeBuffer(fd2, cfg2, buffer2, 2);
	}
	
	/* Re-queue the Request to the camera. */
	request->reuse(Request::ReuseBuffers);
	camera2->queueRequest(request);
}

void configureCamera(std::shared_ptr<Camera>& camera, std::unique_ptr<CameraConfiguration>& config, options& params)
{
	if (!config)
		std::cout << "failed to generate viewfinder configuration\n";
	
    StreamConfiguration &streamConfig = config->at(0);
	std::cout << "Default viewfinder configuration is: " << streamConfig.toString() << std::endl;
	
	int val = camera->configure(config.get());
    if (val) {
        std::cout << "CONFIGURATION FAILED!" << std::endl;
        std::exit(EXIT_FAILURE);
    }
	
    Size size(1280, 960);
	auto area = camera->properties().get(properties::PixelArrayActiveAreas);
	if (params.width != 0 && params.height != 0) //width and height were input
		size=Size(params.width, params.height);
    else if (area)
	{
		// The idea here is that most sensors will have a 2x2 binned mode that
		// we can pick up.
		size = (*area)[0].size() / 2;
		//size = size.boundedToAspectRatio(Size(params.width, params.height));
		size.alignDownTo(2, 2); // YUV420 will want to be even
		std::cout << "Viewfinder size chosen is " << size.toString() << std::endl;
	}
	
	config->at(0).pixelFormat = libcamera::formats::YUV420;
	config->at(0).size = size;
	
	config->at(0).bufferCount = params.buffer_count;
	
	config->validate();
	std::cout << "Validated viewfinder configuration is: "
		  << streamConfig.toString() << std::endl;
		  
	val = camera->configure(config.get());
	if (val) {
		std::cout << "CONFIGURATION FAILED!" << std::endl;
		//return EXIT_FAILURE;
	}

	/*
	 * Once we have a validated configuration, we can apply it to the
	 * Camera.
	 */
	camera->configure(config.get());
}

int main(int argc, char **argv)
{
	options params = {
		.dual_cameras = 1,
		.width = 0, //default
		.height = 0, //default
		.prev_x = 0, 
		.prev_y = 0, 
		.prev_width = 1920, 
		.prev_height = 1080,
		.fps = 30.0,
		.shutterSpeed = 0,
		.exposure = "normal",
		.exposure_index = cam_exposure_index,
		.timeout = 10,
		.buffer_count = 4
	};
			
	int arg;
	while ((arg = getopt(argc, argv, "r:w:h:p:f:s:e:t:b:")) != -1)
	{
		switch (arg)
		{
			case 'd':
				params.dual_cameras = std::stoi(optarg);
				break;
			case 'w':
				params.width = std::stoi(optarg);
				break;
			case 'h':
				params.height = std::stoi(optarg);
				break;
			case 'p':
			    sscanf(optarg, "%u,%u,%u,%u", &params.prev_x, &params.prev_y, &params.prev_width, &params.prev_height);
				break;
			case 'f':
				params.fps = std::stoi(optarg);
				break;
			case 's':
				params.shutterSpeed = std::stoi(optarg);
				break;
			case 'e':
				if (strcmp(optarg, "normal") == 0) params.exposure = "normal";
				else if (strcmp(optarg, "sport") == 0) params.exposure = "sport";
				else if (strcmp(optarg, "short") == 0) params.exposure = "short";
				else if (strcmp(optarg, "long") == 0) params.exposure = "long";
				else if (strcmp(optarg, "custom") == 0) params.exposure = "custom";
				else printf("Unkown exposure mode, defaulting to noraml\n");
				break;
			case 't':
				params.timeout = std::stoi(optarg);
				break;
			case 'b':
				params.buffer_count = std::stoi(optarg);
				break;
			default:
				printf("Usage: %s [-d dual cameras] [-w width] [-h height] [-p x,y,width,height][-f fps] [-s shutter-speed-ns] [-e exposure] [-t timeout] \n", argv[0]);
				break;
		}
	}
	
	if (arg < 1)
		printf("Usage: %s [-d dual cameras] [-w width] [-h height] [-p width,height,x_off,y_off][-f fps] [-s shutter-speed-ns] [-e exposure] [-t timeout] \n", argv[0]);
	
	std::unique_ptr<CameraManager> cm = std::make_unique<CameraManager>();
	cm->start();

	if (cm->cameras().empty()) {
		std::cout << "No cameras were identified on the system."
			  << std::endl;
		cm->stop();
		return EXIT_FAILURE;
	}

	std::string cameraId = cm->cameras()[0]->id();
	camera = cm->get(cameraId);
	camera->acquire();
	
	std::string cameraId2 = cm->cameras()[1]->id();
	camera2 = cm->get(cameraId2);
	camera2->acquire();

	std::unique_ptr<CameraConfiguration> config =
		camera->generateConfiguration( { StreamRole::Viewfinder } );
		
	std::unique_ptr<CameraConfiguration> config2 =
		camera2->generateConfiguration( { StreamRole::Viewfinder } );
		
	configureCamera(camera, config, params);
	configureCamera(camera2, config2, params);
	
	std::shared_ptr<libcamera::CameraConfiguration> configs[2];
	configs[0] = std::move(config);
	configs[1] = std::move(config2);
	
	FrameBufferAllocator *allocators[2] = {new FrameBufferAllocator(camera), new FrameBufferAllocator(camera2)};
	for (int i = 0; i < 2; i++) {
		for (StreamConfiguration &cfg : *configs[i]) {
			int ret = allocators[i]->allocate(cfg.stream());
			if (ret < 0) {
				std::cerr << "Can't allocate buffers" << std::endl;
				return EXIT_FAILURE;
			}

			size_t allocated = allocators[i]->buffers(cfg.stream()).size();
			std::cout << "Allocated " << allocated << " buffers for stream" << std::endl;
		}
	}
	
	std::vector<std::unique_ptr<Request>> requests;
	std::vector<std::unique_ptr<Request>> requests2;
	for (int cameraIndex = 0; cameraIndex < 2; ++cameraIndex) {
		StreamConfiguration &streamConfig = configs[cameraIndex]->at(0);
		Stream *stream = streamConfig.stream();
		const std::vector<std::unique_ptr<FrameBuffer>> &buffers = allocators[cameraIndex]->buffers(stream);

		std::vector<std::unique_ptr<Request>>& currentRequests = (cameraIndex == 0) ? requests : requests2;

		for (unsigned int i = 0; i < buffers.size(); ++i) {
			std::unique_ptr<Request> request = (cameraIndex == 0) ? camera->createRequest() : camera2->createRequest();

			if (!request)
			{
				std::cerr << "Can't create request" << std::endl;
				return EXIT_FAILURE;
			}

			const std::unique_ptr<FrameBuffer> &buffer = buffers[i];
			int ret = request->addBuffer(stream, buffer.get());
			if (ret < 0)
			{
				std::cerr << "Can't set buffer for request" << std::endl;
				return EXIT_FAILURE;
			}

			currentRequests.push_back(std::move(request));
		}
	}
	
	camera->requestCompleted.connect(requestComplete);
	camera2->requestCompleted.connect(requestComplete2);
	
	ControlList controls;
	
	std::map<std::string, int> exposure_table =
		{ { "normal", libcamera::controls::ExposureNormal },
			{ "sport", libcamera::controls::ExposureShort },
			{ "short", libcamera::controls::ExposureShort },
			{ "long", libcamera::controls::ExposureLong },
			{ "custom", libcamera::controls::ExposureCustom } };
			
	if (exposure_table.count(params.exposure) == 0)
		throw std::runtime_error("Invalid exposure mode:" + params.exposure);
	cam_exposure_index = exposure_table[params.exposure];
	
	int64_t frame_time = 1000000 / params.fps; // in us
	
	controls.set(controls::AeExposureMode, cam_exposure_index);
	controls.set(controls::ExposureTime, params.shutterSpeed);
	controls.set(controls::FrameDurationLimits, libcamera::Span<const int64_t, 2>({ frame_time, frame_time }));
	
	//if (!controls.get(controls::Brightness)) // Adjust the brightness of the output images, in the range -1.0 to 1.0
	//	controls.set(controls::Brightness, 0.0);
	//if (!controls.get(controls::Contrast)) // Adjust the contrast of the output image, where 1.0 = normal contrast
	//	controls.set(controls::Contrast, 1.0);
    
    // Set the exposure time
    //controls.set(controls::ExposureTime, frame_time);
    
	camera->start(&controls);
	camera2->start(&controls);
	
	for (std::unique_ptr<Request> &request : requests)
		camera->queueRequest(request.get());
		
	for (std::unique_ptr<Request> &request2 : requests2)
		camera2->queueRequest(request2.get());
		
	// Setup EGL context
	makeWindow("simple-cam", params.prev_x, params.prev_y, params.prev_width, params.prev_height);

	loop.timeout(params.timeout);
	int ret = loop.exec(params.prev_width, params.prev_height, params.timeout);
	std::cout << "Capture ran for " << params.timeout << " seconds and "
		  << "stopped with exit status: " << ret << std::endl;

	camera->stop();
	camera2->stop();
	delete allocators[0];
	delete allocators[1];
	camera->release();
	camera2->release();
	camera.reset();
	camera2.reset();
	cm->stop();
	requests.clear();
	requests2.clear();
	cleanup();

	return EXIT_SUCCESS;
}
