export module VkFftWrapper;
#if 0
#define VKFFT_BACKEND 3 // 3 is for OpenCL
#include <VkFFT/vkFFT.h>
#include <vector>

int foo() {
    const int N = 512; // Width
    const int M = 512; // Height
    const size_t num_elements = N * M;
    const size_t buffer_size = num_elements * 2 * sizeof(float); // C2C: Real and Imaginary

    // 1. Prepare Host Data (Interleaved Real/Imaginary)
    std::vector<float> host_input(num_elements * 2, 0.0f);
    std::vector<float> host_kernel(num_elements * 2, 0.0f);

    float sigma = 5.0f;
    for (int y = 0; y < M; y++) {
        for (int x = 0; x < N; x++) {
            // Sample input (a simple square in the middle)
            if (x > N / 4 && x < 3 * N / 4 && y > M / 4 && y < 3 * M / 4)
                host_input[2 * (y * N + x)] = 1.0f;

            // Gaussian Kernel (centered at 0,0 for FFT alignment)
            int dx = (x < N / 2) ? x : x - N;
            int dy = (y < M / 2) ? y : y - M;
            float val = expf(-(dx * dx + dy * dy) / (2 * sigma * sigma));
            host_kernel[2 * (y * N + x)] = val;
        }
    }

    // 2. OpenCL Setup
    cl_int err;
    cl_platform_id platform;
    clGetPlatformIDs(1, &platform, NULL);
    cl_device_id device;
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    cl_command_queue queue = clCreateCommandQueue(context, device, 0, &err);

    // 3. GPU Buffer Allocation & Copy
    cl_mem d_input = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, buffer_size, host_input.data(), &err);
    cl_mem d_kernel = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, buffer_size, host_kernel.data(), &err);

    // 4. VkFFT Configuration
    VkFFTConfiguration config = {};
    config.device = &device;
    config.context = &context;
    config.commandQueue = &queue;
    config.FFTdim = 2;
    config.size[0] = N;
    config.size[1] = M;
    config.performConvolution = 1; // Tells VkFFT we are doing convolution
    config.kernelSize[0] = N;
    config.kernelSize[1] = M;
    config.kernel = &d_kernel; // The buffer containing the kernel

    // 5. Initialize VkFFT
    VkFFTApp app = {};
    VkFFTResult res = initializeVkFFT(&app, config);
    if (res != VKFFT_SUCCESS) return 1;

    // 6. Execute Convolution
    // This performs: FFT(input) -> Pointwise Multiply with FFT(kernel) -> IFFT(result)
    VkFFTLaunchParams launchParams = {};
    launchParams.buffer = &d_input;
    launchParams.kernel = &d_kernel;

    VkFFTLaunchConvolution(&app, &launchParams);

    // 7. Copy Result Back
    clEnqueueReadBuffer(queue, d_input, CL_TRUE, 0, buffer_size, host_input.data(), 0, NULL, NULL);

    // Cleanup
    deleteVkFFT(&app);
    clReleaseMemObject(d_input);
    clReleaseMemObject(d_kernel);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    std::cout << "Convolution complete. Value at center: " << host_input[2 * ((M / 2) * N + (N / 2))] << std::endl;

    return 0;
}
#endif