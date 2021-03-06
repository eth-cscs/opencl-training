//Stencil/convolution w and w/o images;
//Author: Ugo Varetto
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <cmath>
#include <sstream>
#include "clutil.h"

#ifdef USE_DOUBLE
typedef double real_t;
#else
typedef float real_t;
#endif

//there is no image data type: only mem objects
typedef cl_mem cl_image;

//------------------------------------------------------------------------------
std::vector< real_t > create_filter() {
    real_t f[3][3] = { 1, 1, 1,
                       1, 0, 1,
                       1, 1, 1 }; 
    return std::vector< real_t >((real_t*)(f), (real_t*)(f) + sizeof(f));
}

//------------------------------------------------------------------------------
std::vector< real_t > create_2d_grid(int width, int height,
                                     int xOffset, int yOffset) {
	std::vector< real_t > g(width * height);
	srand(time(0));
    for(int y = 0; y != height; ++y) {
        for(int x = 0; x != width; ++x) {
            g[y * width + x] = (rand() % 10);// / 10.0;
        }
    }
	return g;
}


//------------------------------------------------------------------------------
void host_apply_stencil(const std::vector< real_t >& in,
                        int size, 
	                    const std::vector< real_t >& filter,
                        int filterSize,
	                    std::vector< real_t >& out) { 
    for(int y = filterSize / 2; y < size - filterSize / 2; ++y) {
        for(int x = filterSize / 2; x < size - filterSize / 2; ++x) {
            real_t e = real_t(0);
            for(int fy = -filterSize / 2; fy <= filterSize / 2; ++fy) {
                for(int fx = -filterSize / 2; fx <= filterSize / 2; ++fx) {
                    e += in[(y + fy) * size + x + fx]
                         * filter[(filterSize / 2 + fy) * filterSize
                                  + filterSize / 2 + fx];                 
                }
            }
            out[y * size + x] = e / real_t(filterSize * filterSize);  
		}
	}
}

//------------------------------------------------------------------------------
double device_apply_stencil(const std::vector< real_t >& in,
                            int size, 
                            const std::vector< real_t >& filter,
                            int filterSize,
                            std::vector< real_t >& out,
                            const CLEnv& clenv,
                            const size_t globalWorkSize[2],
                            const size_t localWorkSize[2]) {

    const int FILTER_SIZE = filterSize;
    const int FILTER_BYTE_SIZE = sizeof(real_t) * FILTER_SIZE * FILTER_SIZE;
    const int SIZE = size;
    const size_t BYTE_SIZE = SIZE * SIZE * sizeof(real_t);

    cl_int status;
    //allocate output buffer on OpenCL device
    cl_mem devOut = clCreateBuffer(clenv.context,
                                   CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR,
                                   BYTE_SIZE,
                                   const_cast< real_t* >(&out[0]),
                                   &status);
    check_cl_error(status, "clCreateBuffer");

    //allocate input buffers on OpenCL devices and copy data
    cl_mem devIn = clCreateBuffer(clenv.context,
                                  CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                  BYTE_SIZE,
                                  const_cast< real_t* >(&in[0]),
                                  &status);
    check_cl_error(status, "clCreateBuffer");
    cl_mem devFilter = clCreateBuffer(clenv.context,
                                  CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                  FILTER_BYTE_SIZE,
                                  const_cast< real_t* >(&filter[0]),
                                  &status);
    check_cl_error(status, "clCreateBuffer");


    //set kernel parameters
    status = clSetKernelArg(clenv.kernel, //kernel
                            0,      //parameter id
                            sizeof(cl_mem), //size of parameter
                            &devIn); //pointer to parameter
    check_cl_error(status, "clSetKernelArg(in)");
    status = clSetKernelArg(clenv.kernel, //kernel
                            1,      //parameter id
                            sizeof(int), //size of parameter
                            &SIZE); //pointer to parameter
    check_cl_error(status, "clSetKernelArg(size)");
    status = clSetKernelArg(clenv.kernel, //kernel
                            2,      //parameter id
                            sizeof(cl_mem), //size of parameter
                            &devFilter); //pointer to parameter
    check_cl_error(status, "clSetKernelArg(filter)");
    status = clSetKernelArg(clenv.kernel, //kernel
                            3,      //parameter id
                            sizeof(int), //size of parameter
                            &FILTER_SIZE); //pointer to parameter
    check_cl_error(status, "clSetKernelArg(SIZE)");
    status = clSetKernelArg(clenv.kernel, //kernel
                            4,      //parameter id
                            sizeof(cl_mem), //size of parameter
                            &devOut); //pointer to parameter
    check_cl_error(status, "clSetKernelArg(out)");


    //launch and time kernel
    const double timems = timeEnqueueNDRangeKernel(
                                    clenv.commandQueue, //queue
                                    clenv.kernel, //kernel                                   
                                    2, //number of dimensions for work-items
                                    0, //global work offset
                                    globalWorkSize, //total number of threads
                                    localWorkSize, //threads per workgroup
                                    0, //number of events that need to
                                       //complete before kernel executed
                                    0); //list of events that need to complete
                                        //before kernel executed

    check_cl_error(status, "clEnqueueNDRangeKernel");
    
    //read data from device
    status = clEnqueueReadBuffer(clenv.commandQueue,
                                 devOut,
                                 CL_TRUE, //blocking read
                                 0, //offset
                                 BYTE_SIZE, //byte size of data
                                 &out[0], //destination buffer in host memory
                                 0, //number of events that need to
                                    //complete before transfer executed
                                 0, //list of events that need to complete
                                    //before transfer executed
                                 0); //event identifying this specific operation
    check_cl_error(status, "clEnqueueReadBuffer");
    check_cl_error(clReleaseMemObject(devIn), "clReleaseMemObject");
    check_cl_error(clReleaseMemObject(devFilter), "clReleaseMemObject");
    check_cl_error(clReleaseMemObject(devOut), "clReleaseMemObject");
    return timems;
}


//------------------------------------------------------------------------------
double device_apply_stencil_image(const std::vector< real_t >& in,
                                  int size, 
                                  const std::vector< real_t >& filter,
                                  int filterSize,
                                  std::vector< real_t >& out,
                                  const CLEnv& clenv,
                                  const size_t globalWorkSize[2],
                                  const size_t localWorkSize[2]) {

    const int FILTER_SIZE = filterSize;
    const int FILTER_BYTE_SIZE = sizeof(real_t) * FILTER_SIZE * FILTER_SIZE;
    const int SIZE = size;
    const size_t BYTE_SIZE = SIZE * SIZE * sizeof(real_t);

    cl_int status;
    cl_image_format format;
    format.image_channel_order = CL_INTENSITY;
    format.image_channel_data_type = CL_FLOAT;
    //allocate output buffer on OpenCL device
#ifdef WRITE_TO_IMAGE
    cl_image devOut = clCreateImage2D(clenv.context,
                                    CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR,
                                    &format,
                                    size,
                                    size,
                                    0, // row stride,
                                       // if 0 = size * sizeof(element type)
                                    const_cast< real_t* >(&out[0]),
                                    &status);
    check_cl_error(status, "clCreateImage2D");
#else    
    cl_mem devOut = clCreateBuffer(clenv.context,
                                   CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR,
                                   BYTE_SIZE,
                                   const_cast< real_t* >(&out[0]),
                                   &status);
    check_cl_error(status, "clCreateBuffer");
#endif    

    //allocate input buffers on OpenCL devices and copy data
   
    cl_image devIn = clCreateImage2D(clenv.context,
                                     CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                     &format,
                                     size,
                                     size,
                                     0, // row stride,
                                        // if 0 = size * sizeof(element type)
                                     const_cast< real_t* >(&in[0]),
                                     &status);
    check_cl_error(status, "clCreateImage2D");
    cl_image devFilter = clCreateImage2D(clenv.context,
                                    CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                    &format,
                                    filterSize,
                                    filterSize,
                                    0, // row stride,
                                       // if 0 = size * sizeof(element type)
                                    const_cast< real_t* >(&filter[0]),
                                    &status);
    check_cl_error(status, "clCreateImage2D");


    //set kernel parameters
    status = clSetKernelArg(clenv.kernel, //kernel
                            0,      //parameter id
                            sizeof(cl_mem), //size of parameter
                            &devIn); //pointer to parameter
    check_cl_error(status, "clSetKernelArg(size)");
    status = clSetKernelArg(clenv.kernel, //kernel
                            1,      //parameter id
                            sizeof(cl_mem), //size of parameter
                            &devFilter); //pointer to parameter
    check_cl_error(status, "clSetKernelArg(SIZE)");
    status = clSetKernelArg(clenv.kernel, //kernel
                            2,      //parameter id
                            sizeof(cl_mem), //size of parameter
                            &devOut); //pointer to parameter
    check_cl_error(status, "clSetKernelArg(out)");


    //launch and time kernel
    const double timems = timeEnqueueNDRangeKernel(
                                    clenv.commandQueue, //queue
                                    clenv.kernel, //kernel                                   
                                    2, //number of dimensions for work-items
                                    0, //global work offset
                                    globalWorkSize, //total number of threads
                                    localWorkSize, //threads per workgroup
                                    0, //number of events that need to
                                       //complete before kernel executed
                                    0); //list of events that need to complete
                                        //before kernel executed

    check_cl_error(status, "clEnqueueNDRangeKernel");

#ifdef WRITE_TO_IMAGE
    //read data from device
    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {SIZE, SIZE, 1};
    //const size_t rowPitch = SIZE * sizeof(real_t);

    //not required for 2d
    //const size_t slicePitch = SIZE * SIZE * sizeof(real_t);
    status = clEnqueueReadImage(clenv.commandQueue,
                                 devOut,
                                 CL_TRUE, //blocking read
                                 &origin[0],
                                 &region[0],
                                 0,//rowPitch
                                 0,//slicePitch,
                                 &out[0], //destination buffer in host memory
                                 0, //number of events that need to
                                    //complete before transfer executed
                                 0, //list of events that need to complete
                                    //before transfer executed
                                 0); //event identifying this specific operation
    check_cl_error(status, "clEnqueueReadImage");

#else    
    status = clEnqueueReadBuffer(clenv.commandQueue,
                                 devOut,
                                 CL_TRUE, //blocking read
                                 0, //offset
                                 BYTE_SIZE, //byte size of data
                                 &out[0], //destination buffer in host memory
                                 0, //number of events that need to
                                    //complete before transfer executed
                                 0, //list of events that need to complete
                                    //before transfer executed
                                 0); //event identifying this specific operation
    check_cl_error(status, "clEnqueueReadBuffer");
#endif    
    check_cl_error(clReleaseMemObject(devOut), "clReleaseMemObject");
    check_cl_error(clReleaseMemObject(devIn), "clReleaseMemObject");
    check_cl_error(clReleaseMemObject(devFilter), "clReleaseMemObject");
    return timems;
}


//------------------------------------------------------------------------------
bool check_result(const std::vector< real_t >& v1,
	              const std::vector< real_t >& v2,
	              double eps) {
    for(int i = 0; i != v1.size(); ++i) {
    	if(double(std::fabs(v1[i] - v2[i])) > eps) return false;
    }
    return true;
}

//------------------------------------------------------------------------------
int main(int argc, char** argv) {
    if(argc < 9) {
        std::cerr << "usage:\n" << argv[0] << '\n'
                  << "  <platform name>\n"
                     "  <device type = default | cpu | gpu | acc | all>\n"
                     "  <device num>\n"
                     "  <OpenCL source file path>\n"
                     "  <kernel name>\n"
                     "  <size>\n"
                     "  <workgroup size>\n"
                     "  <std|image>\n"
                     "  [build parameters passed to the OpenCL compiler]\n"
                     "  filter size is 3x3; size - halo region size must be"
                     " evenly divisible by the workgroup size"
                  << std::endl;
        exit(EXIT_FAILURE);   
    }
    bool image = false;
    if(std::string(argv[8]) == "image") {
#ifdef USE_DOUBLE
        std::cerr << "Double precision not supported by 1-element float images"
                  << std::endl;
        exit(EXIT_FAILURE);
#endif                  
        image = true; 
    }
    std::string options;
    for(int a = 9; a < argc; ++a) {
        options += argv[a];
    }
#ifdef WRITE_TO_IMAGE
    options += " -DWRITE_TO_IMAGE";
#endif
    const int FILTER_SIZE = 3; //3x3
    const int SIZE = atoi(argv[6]);
    const int BLOCK_SIZE = atoi(argv[7]);
    if((SIZE - (2 * (FILTER_SIZE / 2))) % BLOCK_SIZE != 0) {
        std::cerr << "size(" << SIZE << ") - " << (2 * (FILTER_SIZE / 2))
                  << " must be evenly divisible by the workgroup size("
                  << BLOCK_SIZE << ")" << std::endl;
        exit(EXIT_FAILURE);          
    }   
    //setup kernel launch configuration
    //total number of threads == number of array elements in core space i.e.
    //image - border (= 2 x (filter size DIV 2) != filter size)
    const size_t globalWorkSize[2] = {SIZE - 2 * (FILTER_SIZE / 2), 
                                      SIZE - 2 * (FILTER_SIZE / 2)};
    //number of per-workgroup local threads
    const size_t localWorkSize[2]  = {BLOCK_SIZE, BLOCK_SIZE}; 
    //setup text header that will be prefixed to opencl code
    std::ostringstream clheaderStream;
#ifdef USE_DOUBLE    
    clheaderStream << "#define DOUBLE\n";
    const double EPS = 0.000000001;
#else
    const double EPS = 0.00001;
#endif    
    CLEnv clenv = create_clenv(argv[1], //platform name
                               argv[2], //device type
                               atoi(argv[3]), //device id
                               true, //profiling
                               argv[4], //cl source code
                               argv[5], //kernel name
                               "", //source code prefix text
                               options.c_str()); //compiler options
   
    cl_int status;
    //create input and output matrices
    std::vector<real_t> in = create_2d_grid(SIZE, SIZE,
                                            FILTER_SIZE / 2, FILTER_SIZE / 2);
    std::vector<real_t> filter = create_filter();
    std::vector<real_t> out(SIZE * SIZE,real_t(0));
    std::vector<real_t> refOut(SIZE * SIZE,real_t(0));        
    
    //launch kernels and check results
    double timems = 0;
    if(image) {
        timems = device_apply_stencil_image(in, SIZE, filter, FILTER_SIZE,
                             out, clenv, globalWorkSize, localWorkSize);
    } else {
        timems = device_apply_stencil(in, SIZE, filter, FILTER_SIZE,
                             out, clenv, globalWorkSize, localWorkSize);
    }
    
    host_apply_stencil(in, SIZE, filter, FILTER_SIZE, refOut);

    if(check_result(out, refOut, EPS)) {
        std::cout << "Elapsed time: " << timems << " ms" << std::endl;
    	std::cout << "PASSED" << std::endl;
    } else {
    	std::cout << "FAILED" << std::endl;
    }	

    release_clenv(clenv);
   
    return 0;
}
