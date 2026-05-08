#include <cstdio>
#include <cstdlib>
#include <cuda_runtime.h>

__global__ void saxpy_kernel(float a, const float *x, float *y, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        y[i] = a * x[i] + y[i];
    }
}

int main(int argc, char **argv) {
    const int n = (argc > 1) ? atoi(argv[1]) : (1 << 24);
    const int iters = (argc > 2) ? atoi(argv[2]) : 4000;
    const size_t bytes = (size_t)n * sizeof(float);
    float *x = nullptr;
    float *y = nullptr;
    float *dx = nullptr;
    float *dy = nullptr;

    x = (float *)malloc(bytes);
    y = (float *)malloc(bytes);
    if (!x || !y) {
        fprintf(stderr, "host allocation failed\n");
        return 1;
    }

    for (int i = 0; i < n; ++i) {
        x[i] = 1.0f;
        y[i] = 2.0f;
    }

    cudaMalloc(&dx, bytes);
    cudaMalloc(&dy, bytes);
    cudaMemcpy(dx, x, bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(dy, y, bytes, cudaMemcpyHostToDevice);

    dim3 block(256);
    dim3 grid((unsigned int)((n + block.x - 1) / block.x));

    for (int i = 0; i < iters; ++i) {
        saxpy_kernel<<<grid, block>>>(2.0f, dx, dy, n);
    }
    cudaDeviceSynchronize();
    cudaMemcpy(y, dy, bytes, cudaMemcpyDeviceToHost);

    printf("completed cuda heatload: n=%d iterations=%d sample=%f\n", n, iters, y[0]);

    cudaFree(dx);
    cudaFree(dy);
    free(x);
    free(y);
    return 0;
}
