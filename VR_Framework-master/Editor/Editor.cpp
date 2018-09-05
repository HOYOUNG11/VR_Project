#include "Editor.h"
#include <iostream>
#include <regex>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <intrin.h>
#include <math.h>
#include <vector>
#include <array>
#include <typeinfo>
#include <CL/cl.hpp>
#include <cstdint>
using namespace std;

int testslider_up = 0;
int testslider_right = 0;
bool check1 = 0;
int minimum = -1000;
int maximum = 3000;

typedef struct point {
	cl_float x = 0; 
	cl_float y = 0; 
	cl_float z = 0;
};

typedef struct ray {
	point start; 
	cl_float x; 
	cl_float y; 
	cl_float z;
};

Editor::Editor(uint32_t width, uint32_t height)
	:m_window(nullptr),
	m_context(nullptr),
	m_width(width),
	m_height(height),
	m_isRunning(true),
	m_hasTexture(false) {
}

Editor::~Editor() {
	ImGui_ImplSdlGL3_Shutdown();
	SDL_GL_DeleteContext(m_context);
	SDL_DestroyWindow(m_window);
	SDL_Quit();
}

bool Editor::Initialize() {
	// Setup SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
		std::cout << ("Error: %s\n", SDL_GetError()) << std::endl;
		return false;
	}

	// Setup window
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_DisplayMode current;
	SDL_GetCurrentDisplayMode(0, &current);
	m_window = SDL_CreateWindow("Volume Renderer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, m_width, m_height, SDL_WINDOW_OPENGL);
	SDL_GLContext glcontext = SDL_GL_CreateContext(m_window);
	glewInit();

	// Setup ImGui binding 
	ImGui_ImplSdlGL3_Init(m_window);
	Process();
	return true; // Return initialization result
}

void Editor::Run() {
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	while (m_isRunning) {
		// Handle SDL events
		SDL_Event event;
		if (SDL_PollEvent(&event)) {
			ImGui_ImplSdlGL3_ProcessEvent(&event);
			HandleSDLEvent(&event);
		}
		ImGui_ImplSdlGL3_NewFrame(m_window);
		// Editor
		{
			ControlPanel(m_width - 720, 720);
			Scene(720, 720);
			// Code sample of ImGui (Remove comment when you want to see it)
			// ImGui::ShowTestWindow();
		}
		// Rendering
		glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
		glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);

		ImGui::Render();
		SDL_GL_SwapWindow(m_window);
	}
}

void Editor::UpdateTexture(const void * buffer, int width, int height) {
	if (!m_hasTexture) {
		auto err = glGetError();
		glGenTextures(1, &m_textureID);
		if (err != GL_NO_ERROR) {
			throw std::runtime_error("Not able to create texture from buffer" + std::to_string(glGetError()));
		}
		else {
			m_hasTexture = true;
		}
	}
	glBindTexture(GL_TEXTURE_2D, m_textureID);
	// set texture sampling methods
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Editor::Process() {
	// there is approximately 15000000 shorts in 512 * 512 * 56 
	short* raw_data = new short[15000000];

	// load file into the array raw_data
	ifstream inStream("volume1_512x512x56-short-bigendian.raw", std::fstream::binary);
	inStream.seekg(0, std::ios_base::end);
	streamsize size = inStream.tellg();
	inStream.seekg(0, std::ios::beg);
	inStream.read((char *)raw_data, size);

	// flip array due to endianess	
	for (int i = 0; i < 14680064; i++) {
		raw_data[i] = ((raw_data[i] & 0x00FF) << 8) | ((raw_data[i] & 0xFF00) >> 8);
	}

	// Ray Casting:
	// Initialize camera & center points
	point camera, center;
	center.x = 256;
	center.y = 256;
	center.z = 28;
	int right = testslider_right;
	if (testslider_right > 90 && testslider_right < 180) {
		right += 180;
	}
	else if (testslider_right > 270 && testslider_right < 360) {
		right -= 180;
	}
	camera.x = 500 * sin(testslider_up * M_PI / 180) * cos(right * M_PI / 180) + center.x; 
	camera.y = 500 * sin(testslider_up * M_PI / 180) * sin(right * M_PI / 180) + center.y;
	camera.z = 500 * cos(testslider_up * M_PI / 180) + center.z;

	// Calculate the slope of view ray with steepest being 1
	ray view_ray;
	view_ray.start = camera;
	view_ray.x = center.x - camera.x; view_ray.y = center.y - camera.y; view_ray.z = center.z - camera.z;
	float unit = sqrt(pow(view_ray.x, 2) + pow(view_ray.y, 2) + pow(view_ray.z, 2));
	view_ray.x /= unit*3;
	view_ray.y /= unit*3;
	view_ray.z /= unit*3;
	
	// Find Ray1 and Ray2 (up vector and right vector)
	// Finding unit1 and unit2 allows the image view to be 512 * 512

	// Find Ray1 (Dot product of Ray1 and View Ray)
	// With Ray1 we have to consider different cases. Since the direction has to be consistent in viewer's 
	// perspective, there must be various instances.
	ray ray1;
	ray1.start = camera;

	if (view_ray.x == 0) {
		ray1.x = 1; ray1.y = 0; ray1.z = 0;
		if (view_ray.y != 0) { ray1.y = view_ray.x / view_ray.y; }
		if (view_ray.z != 0) { ray1.z = view_ray.x / view_ray.z; }
	}
	else if (view_ray.y == 0) {
		ray1.x = 0; ray1.y = 1; ray1.z = 0;
		if (view_ray.x != 0) { ray1.x = view_ray.y / view_ray.x; }
		if (view_ray.z != 0) { ray1.z = view_ray.y / view_ray.z; }
	}
	else if (view_ray.z == 0) {
		ray1.x = 0; ray1.y = 0; ray1.z = 1;
		if (view_ray.x != 0) { ray1.x = view_ray.z / view_ray.x; }
		if (view_ray.y != 0) { ray1.y = view_ray.z / view_ray.y; }
	}
	else {
		ray1.z = 1; ray1.y = -view_ray.z / view_ray.y; ray1.x = -view_ray.z / view_ray.x;
	}

	unit = sqrt(pow(ray1.x, 2) + pow(ray1.y, 2) + pow(ray1.z, 2));
	ray1.x /= (unit);
	ray1.y /= (unit);
	ray1.z /= (unit);

	// Find Ray2 (Cross product of Ray1 and View Ray)
	ray ray2;
	ray2.start = camera;
	ray2.x = ray1.y * view_ray.z - ray1.z * view_ray.y;
	ray2.y = -(ray1.x * view_ray.z - ray1.z * view_ray.x);
	ray2.z = ray1.x * view_ray.y - ray1.y * view_ray.x;

	unit = sqrt(pow(ray2.x, 2) + pow(ray2.y, 2) + pow(ray2.z, 2));
	ray2.x /= (unit);
	ray2.y /= (unit);
	ray2.z /= (unit);

	if ((testslider_right == 0 || testslider_right == 360) && testslider_up !=0 && testslider_up != 360) {
		ray1.x *= -1;
		ray1.y *= -1;
		ray1.z *= -1;

		ray2.x *= -1;
		ray2.y *= -1;
		ray2.z *= -1;
	}

	if (testslider_up > 180 && testslider_up < 360 && testslider_right != 0 && testslider_right != 360) {
		ray1.x *= -1;
		ray1.y *= -1;
		ray1.z *= -1;

		ray2.x *= -1;
		ray2.y *= -1;
		ray2.z *= -1;
	}
	// Find bottom left end point considering that our image size is 512 * 512 
	point end;
	end.x = camera.x - ray1.x * 256 - ray2.x * 256;
	end.y = camera.y - ray1.y * 256 - ray2.y * 256;
	end.z = camera.z - ray1.z * 28 - ray2.z * 28;

	// Setup OpenCL
	vector<cl::Platform> platforms;
	cl::Platform::get(&platforms);

	auto platform = platforms.front();
	vector<cl::Device> devices;
	platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);

	auto device = devices.front();

	ifstream testFile("ProcessViewRay.cl");
	string src(istreambuf_iterator<char>(testFile), (std::istreambuf_iterator<char>()));

	cl::Program::Sources sources(1, make_pair(src.c_str(), src.length() + 1));

	cl::Context context(devices);
	cl::Program program(context, sources);

	int err = program.build("-cl-std=CL1.2");
	std::cout << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device) << std::endl;
	

	const int image_length = 512;
	int count = image_length * image_length;

	unsigned char* RGB_converted = new unsigned char[count * 4];
	float alpha = 0;
	float color = 0;
	
	point *points = new point[count];
	point *cl_end = &end;
	ray *cl_ray1 = &ray1;
	ray *cl_ray2 = &ray2;
	ray *cl_view_ray = &view_ray;
	int *cl_minimum = &minimum;
	int *cl_maximum = &maximum;

	float *watch = new float[count];
	for (int i = 0; i < count; i++) {
		watch[i] = 10;
	}

	cl::Buffer buf1(context, CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(point) * count, points);
	cl::Buffer buf2(context, CL_MEM_READ_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(point), cl_end);
	cl::Buffer buf3(context, CL_MEM_READ_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(ray), cl_ray1);
	cl::Buffer buf4(context, CL_MEM_READ_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(ray), cl_ray2);
	cl::Buffer buf5(context, CL_MEM_READ_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(ray), cl_view_ray);
	cl::Buffer buf6(context, CL_MEM_READ_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(int), cl_minimum);
	cl::Buffer buf7(context, CL_MEM_READ_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(int), cl_maximum);
	cl::Buffer buf8(context, CL_MEM_READ_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(short) * 15000000, raw_data);
	cl::Buffer buf9(context, CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(unsigned char) * count * 4, RGB_converted);
	cl::Buffer buf10(context, CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(float)*count , watch);


	cl::Kernel kernel(program, "ProcessImageView");
	kernel.setArg(0, buf1);
	kernel.setArg(1, buf2);
	kernel.setArg(2, buf3);
	kernel.setArg(3, buf4);
	kernel.setArg(4, buf5);
	kernel.setArg(5, buf6);
	kernel.setArg(6, buf7);
	kernel.setArg(7, buf8);
	kernel.setArg(8, buf9);
	kernel.setArg(9, buf10);

	cl_int error_code = 0;
	cl::CommandQueue queue(context, device);
	error_code = queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(image_length, image_length));
	queue.enqueueReadBuffer(buf1, CL_TRUE, 0, sizeof(point) * count, points);
	queue.enqueueReadBuffer(buf9, CL_TRUE, 0, sizeof(unsigned char) * count * 4, RGB_converted);
	queue.enqueueReadBuffer(buf10, CL_TRUE, 0, sizeof(float) * count, watch);


	//// Send view rays
	//#pragma omp parallel for 
	//for (int i = 0; i < 512; i++) {
	//	for (int j = 0; j < 512; j++) {
	//		// craete temporary starting point from view image
	//		point temp;
	//		temp.x = end.x + i * ray1.x + j * ray2.x;
	//		temp.y = end.y + i * ray1.y + j * ray2.y;
	//		temp.z = end.z + i * float(56)/float(512) * ray1.z + j * float(56)/float(512) * ray2.z;
	//		
	//		float alpha = 0;
	//		float color = 0;

	//		///*bool pass = false;*/
	//		//vector<float> intersect;
	//		//if (view_ray.x != 0) {
	//		//	float t1 = (0 - temp.x) / view_ray.x;
	//		//	/*if (0 < t1 && t1 < 512) { break; }*/
	//		//	float t2 = (512 - temp.x) / view_ray.x;

	//		//	intersect.push_back(t1);
	//		//	intersect.push_back(t2);
	//		//}
	//		//if (view_ray.y != 0) {
	//		//	float t1 = (0 - temp.y) / view_ray.y;
	//		//	/*if (0 < t1 && t1 < 512) { break; }*/
	//		//	float t2 = (512 - temp.y) / view_ray.y;

	//		//	intersect.push_back(t1);
	//		//	intersect.push_back(t2);
	//		//}
	//		//if (view_ray.z != 0) {
	//		//	float t1 = (0 - temp.z) / view_ray.z;
	//		//	/*if (0 < t1 && t1 < 56) { break; }*/
	//		//	float t2 = (56 - temp.z) / view_ray.z;

	//		//	intersect.push_back(t1);
	//		//	intersect.push_back(t2);
	//		//}

	//		///*if (intersect.size() == 0) { continue; 
	//		//cout << " !! " << endl;
	//		//}
	//		//else { pass = true; }

	//		//sort(intersect.begin(), intersect.end());

	//		//point start, finish;

	//		//start.x = view_ray.x * intersect[intersect.size() / 2 - 1] + temp.x;
	//		//start.y = view_ray.y * intersect[intersect.size() / 2 - 1] + temp.y;
	//		//start.z = view_ray.z * intersect[intersect.size() / 2 - 1] + temp.z;
	//		//
	//		//finish.x = view_ray.x * intersect[intersect.size() / 2] + temp.x;
	//		//finish.y = view_ray.y * intersect[intersect.size() / 2] + temp.y;
	//		//finish.z = view_ray.z * intersect[intersect.size() / 2] + temp.z;

	//		//temp = start;
	//		
	//		for (int k = 0; k < 10000; k++) {
	//			temp.x += view_ray.x;
	//			temp.y += view_ray.y;
	//			temp.z += view_ray.z;

	//			//// if reach the endpoint calculated break out of loop
	//			//if (temp.x == finish.x && temp.y == finish.y && temp.z == finish.z) { break; }
	//											
	//			// x + 512*y + 262144 * z
	//			if ((temp.x >= 0 && temp.x <= 512) && (temp.y >= 0 && temp.y <= 512) && (temp.z >= 0 && temp.z <= 56)) {
	//				int index = (int)temp.x + 512 * (int)temp.y + 262144 * (int)temp.z;
	//				if ((index <= 14680064 && index >= 0)) {
	//					// Trilinear Interpolation
	//					float ix1 = (temp.x - (int)temp.x) * raw_data[index] + ((int)temp.x - temp.x + 1) * raw_data[index + 1];
	//					float ix2 = (temp.x - (int)temp.x) * raw_data[index + 512] + ((int)temp.x - temp.x + 1) * raw_data[index + 513];
	//					float ix3 = (temp.x - (int)temp.x) * raw_data[index + 262144] + ((int)temp.x - temp.x + 1) * raw_data[index + 262145];
	//					float ix4 = (temp.x - (int)temp.x) * raw_data[index + 262656] + ((int)temp.x - temp.x + 1) * raw_data[index + 262657];

	//					float iy1 = (temp.y - (int)temp.y) * ix1 + ((int)temp.y - temp.y + 1) * ix3;
	//					float iy2 = (temp.y - (int)temp.y) * ix2 + ((int)temp.y - temp.y + 1) * ix4;

	//					pixel = (temp.z - (int)temp.z) * iy1 + ((int)temp.z - temp.z + 1) * iy2;
	//					
	//					// C'_i = C'_i+1 + (1 - A'_i+1) * C_i
	//					// A'_i = A'_i+1 + (1 - A'_i+1) * A_i
	//					float pixel_converted = (pixel - minimum) / (maximum - minimum);
	//					if (pixel < minimum) { pixel_converted = 0; }
	//					if (pixel > maximum) { pixel_converted = 0; }
	//					alpha = alpha + (1 - alpha) * pixel_converted;
	//					color = color + (1 - alpha) * pixel_converted;
	//					if (alpha >= 0.95) { break; }
	//				}
	//			}
	//		}
	//		RGB_converted[(i * 512 + j) * 4] = (unsigned char)(color * 255);
	//		RGB_converted[(i * 512 + j) * 4 + 1] = (unsigned char)(color * 255);
	//		RGB_converted[(i * 512 + j) * 4 + 2] = (unsigned char)(color * 255);
	//		RGB_converted[(i * 512 + j) * 4 + 3] = (unsigned char)(alpha * 255);
	//	}
	//}

	UpdateTexture(RGB_converted, 512, 512);
	
	inStream.close();
}

void Editor::ControlPanel(uint32_t width, uint32_t height) {
	// Control Panel Window
	ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::Begin("Control Panel", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

	//// TODO: Wrtie UI functions
	//int testsliderx = 255;
	//int testslidery = 255;
	//int testsliderz = 28;
	if (ImGui::SliderInt("Up", &testslider_up, 0, 360) || ImGui::SliderInt("Right", &testslider_right, 0, 360)) {
		Process();
	}

	
	ImGui::PushItemWidth(100);
	if (ImGui::InputInt("min", &minimum, 100, -1000, ImGuiInputTextFlags_EnterReturnsTrue)) {
		Process();
	}

	if (ImGui::InputInt("max", &maximum, 100, 3000, ImGuiInputTextFlags_EnterReturnsTrue)) {
		Process();
	}
	ImGui::PopItemWidth();
	ImGui::End();
}

void Editor::Scene(uint32_t width, uint32_t height) {
	// Scene Window
	ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));
	ImGui::SetNextWindowPos(ImVec2((float)(m_width - width), 0.f));
	ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
	// Draw texture if there is one
	if (m_hasTexture) {
		ImGui::Image(ImTextureID(m_textureID), ImGui::GetContentRegionAvail());
	}

	ImGui::End();
}

void Editor::OnResize(uint32_t width, uint32_t height) {
	m_width = width;
	m_height = height;
}

void Editor::HandleSDLEvent(SDL_Event * event) {
	// SDL_Event wiki : https://wiki.libsdl.org/SDL_Event
	static bool mouseIsDown = false;
	static bool isDragging = false;
	int degreeStep = 5;
	switch (event->type) {
	case SDL_QUIT:
		m_isRunning = false;
		break;
	case SDL_KEYDOWN:
		break;
	case SDL_MOUSEWHEEL:
		break;
	case SDL_MOUSEMOTION:
		break;
	case SDL_MOUSEBUTTONDOWN:
		break;
	case SDL_MOUSEBUTTONUP:
		mouseIsDown = false;
		break;
	case SDL_WINDOWEVENT:
		switch (event->window.event) {
		case SDL_WINDOWEVENT_RESIZED:
			OnResize(event->window.data1, event->window.data2);
			break;
		default:
			break;
		}
	default:
		break;
	}
}
