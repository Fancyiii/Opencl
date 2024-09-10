// Program courtesy : Oak Ridge National Labs (with modifications)

#define CL_TARGET_OPENCL_VERSION 120
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <CL/opencl.h>
#include <sys/time.h>
#include <string.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024

// OpenCL kernel. Each work item takes care of one element of c
// The kernel code is supplied as a string. This could be read from a text file (with extension .cl) using file I/O instead.
const char *kernelSource =                                       "\n" \
"#pragma OPENCL EXTENSION cl_khr_fp64 : enable                    \n" \
"__kernel void matrixMul(  __global int *a,                       \n" \
"                          __global int *b,                       \n" \
"                          __global int *c,                       \n" \
"                          const unsigned int width)              \n" \
"{                                                                \n" \
"    //Get our global thread ID                                   \n" \
"    int id = get_global_id(0);                                   \n" \
"                                                                 \n" \
"    //Make sure we do not go out of bounds                       \n" \
"                                                                 \n" \
"        for (int i = 0; i < width ; i++) {                       \n" \
"            c[id] += a[id * width + i] * b[i]/256;               \n" \
"        }                                                        \n" \
"}                                                                \n" \
                                                                 "\n" ;


// note : cl_khr_fp64 is needed for double precision floating point, but not for single precision
// I have used single precision here since intel integrated GPUs don't support double precision
// you can see the supported extensions via clinfo



#define BUFFER_SIZE 1024

bool read_csv(char* filename, int* matrix, int row, int col) {
    FILE *file;
    char buffer[BUFFER_SIZE];
    char *field;
    int current_row = 0;
    int current_col = 0;

    // Open the CSV file
    file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error opening file.\n");
        return false;
    }

    // Read each line of the CSV file
    while (fgets(buffer, BUFFER_SIZE, file) != NULL && current_row < row) {
        // Remove newline character if present
        if (buffer[strlen(buffer) - 1] == '\n') {
            buffer[strlen(buffer) - 1] = '\0';
        }

        // Split the line into fields
        field = strtok(buffer, ",");
        current_col = 0;
        while (field != NULL && current_col < col) {
            // Convert string to integer and store in the matrix
            matrix[current_row * col + current_col] = atoi(field);
            field = strtok(NULL, ",");
            current_col++;
        }
        current_row++;
    }

    // Close the file
    fclose(file);

    return true;
}

int main( int argc, char* argv[] )
{
    // Length of vectors
    unsigned int row = 64;
    unsigned int col = 8;

    // Host input vectors
    int *h_a;
    int *h_b;
    // Host output vector
    int *h_c;

    // Device input buffers
    cl_mem d_a;
    cl_mem d_b;
    // Device output buffer
    cl_mem d_c;

    cl_platform_id cpPlatform;        // OpenCL platform
    cl_device_id device_id;           // device ID
    cl_context context;               // context
    cl_command_queue queue;           // command queue
    cl_program program;               // program
    cl_kernel kernel;                 // kernel

    // Size, in bytes, of each vector
    size_t bytes_a = row*col*sizeof(int);
    size_t bytes_b = col*sizeof(int);
    size_t bytes_c = row*sizeof(int);

    // Allocate memory for each vector on host
    h_a = (int*)malloc(bytes_a);
    h_b = (int*)malloc(bytes_b);
    h_c = (int*)malloc(bytes_c);

    // Initialize vectors on host
    read_csv("A.csv", h_a, 64, 8);
    read_csv("B.csv", h_b, 8, 1);

    printf("==================A==========================");
    for (int i = 0; i<64; i++) {
        for (int j=0; j<8; j++) {
            printf("%d ", h_a[i*col+j]);
        }
        printf("\n");
    }
    printf("==================B==========================");
    printf("\n");
    for (int j=0; j<8; j++) {
        printf("%d ", h_b[j]);
    }
    printf("\n");

    size_t globalSize, localSize;
    cl_int err;

    // Number of work items in each local work group
    localSize = 64;

    // Number of total work items - localSize must be devisor
    globalSize = ceil(row/(int)localSize)*localSize;

    // Bind to platform
    err = clGetPlatformIDs(1, &cpPlatform, NULL);

    // Get ID for the device
    err = clGetDeviceIDs(cpPlatform, CL_DEVICE_TYPE_GPU, 1, &device_id, NULL);

    // Create a context
    context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);

    // Create a command queue
    queue = clCreateCommandQueue(context, device_id, 0, &err);

    // Create the compute program from the source buffer
    program = clCreateProgramWithSource(context, 1,
                            (const char **) & kernelSource, NULL, &err);

    // Build the program executable
    err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);

    // to print error info if your program doesn't compile - courtesy stackoverflow.
    if (err != CL_SUCCESS) {
    char *buff_erro;
    cl_int errcode;
    size_t build_log_len;
    errcode = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &build_log_len);
    if (errcode) {
                printf("clGetProgramBuildInfo failed at line %d\n", __LINE__);
                exit(-1);
            }

        buff_erro = malloc(build_log_len);
        if (!buff_erro) {
            printf("malloc failed at line %d\n", __LINE__);
            exit(-2);
        }

        errcode = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, build_log_len, buff_erro, NULL);
        if (errcode) {
            printf("clGetProgramBuildInfo failed at line %d\n", __LINE__);
            exit(-3);
        }

        fprintf(stderr,"Build log: \n%s\n", buff_erro); //Be careful with  the fprint
        free(buff_erro);
        fprintf(stderr,"clBuildProgram failed\n");
        exit(EXIT_FAILURE);
    }

    // Create the compute kernel in the program we wish to run
    kernel = clCreateKernel(program, "matrixMul", &err);

    // note down the time before the accelerator overhead starts
    struct timeval time_curr;
    unsigned int time1;
    gettimeofday(&time_curr, NULL);
    time1 = time_curr.tv_sec * (int)1e6 + time_curr.tv_usec;

    // Create the input and output arrays in device memory for our calculation
    d_a = clCreateBuffer(context, CL_MEM_READ_ONLY,  bytes_a, NULL, NULL);
    d_b = clCreateBuffer(context, CL_MEM_READ_ONLY,  bytes_b, NULL, NULL);
    d_c = clCreateBuffer(context, CL_MEM_WRITE_ONLY, bytes_c, NULL, NULL);

    // Write our data set into the input array in device memory
    err  = clEnqueueWriteBuffer(queue, d_a, CL_TRUE, 0, bytes_a, h_a, 0, NULL, NULL);
    err |= clEnqueueWriteBuffer(queue, d_b, CL_TRUE, 0, bytes_b, h_b, 0, NULL, NULL);

    // Set the arguments to our compute kernel
    err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_a);
    err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_b);
    err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &d_c);
    err |= clSetKernelArg(kernel, 3, sizeof(unsigned int), &col);

    // Execute the kernel over the entire range of the data set
    err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &globalSize, &localSize,
                                                              0, NULL, NULL);

    // Wait for the command queue to get serviced before reading back results
    clFinish(queue);

    // Read the results from the device
    clEnqueueReadBuffer(queue, d_c, CL_TRUE, 0,
                                bytes_c, h_c, 0, NULL, NULL );

    // note down the time after the accelerator is done
    unsigned int time2;
    gettimeofday(&time_curr, NULL);
    time2 = time_curr.tv_sec * (int)1e6 + time_curr.tv_usec;

    printf("%d\n", time2-time1);

    //Sum up vector c and print result divided by n, this should equal 1 within error
    float sum = 0;
    for(int i=0; i<row; i++)
        printf("c[%d]=%d\n", i, h_c[i]);

    // release OpenCL resources
    clReleaseMemObject(d_a);
    clReleaseMemObject(d_b);
    clReleaseMemObject(d_c);
    clReleaseProgram(program);
    clReleaseKernel(kernel);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    //release host memory
    free(h_a);
    free(h_b);
    free(h_c);

    return 0;
}
