#ifndef _KERNEL_UPDATE_U_
#define _KERNEL_UPDATE_U_

#include <stdio.h>
#include <cutil_inline.h>
#ifndef max
#define max( a, b ) ( ((a) > (b)) ? (a) : (b) )
#endif

#ifndef min
#define min( a, b ) ( ((a) < (b)) ? (a) : (b) )
#endif


_global__ void kernel_derivative_u(float *ux_, float *uy_, float *u_, unsigned int stride, unsigned int width, unsigned int height )
{
    unsigned int x = blockIdx.x*blockDim.x + threadIdx.x;
    unsigned int y = blockIdx.y*blockDim.y + threadIdx.y;

    if ( x + 1 < width ) //&& y < height )
    {
        ux_[y*stride+x] = u_[y*stride+(x+1)] - u_[y*stride+x];
    }

    if ( y + 1 < height )
    {
        uy_[y*stride+x] = u_[(y+1)*stride+x] - u_[y*stride+x];
    }

}

extern "C" void launch_kernel_derivative_u(float* ux_, float *uy_, float* u_, unsigned int stride, unsigned int mesh_width, unsigned int mesh_height)
{
    // execute the kernel
    dim3 block(8, 8, 1);
    dim3 grid(mesh_width / block.x, mesh_height / block.y, 1);
    kernel_derivative_u<<< grid, block>>>(ux_, uy_, u_, stride, mesh_width, mesh_height);
    cutilCheckMsg("execution failed\n");

}


//extern "C" void launch_kernel_update_u(float *px, float *py, float *u, float *u_, float* g, unsigned int stride, unsigned int mesh_width, unsigned int mesh_height, float tau, float lambda)
//{
//    // execute the kernel
//    dim3 block(8, 8, 1);
//    dim3 grid(mesh_width / block.x, mesh_height / block.y, 1);
//    kernel_update_u<<< grid, block>>>(px,py,u, u_, g, stride, mesh_width, mesh_height, tau, lambda);
//    cutilCheckMsg("execution failed\n");
//}



__global__ void kernel_primalu(float tau, float *px, float *py, float xisqr, float *WiT_BiT_DiT_qi, int width, int height,
                             unsigned int stride, int N_imgs)
{

    //u^{n+1} = u^{n} - tau( -divp^{n+1} + \xi^{2} \sum_{i=1}^{N}(W^{T}B^{T}D^{T}q^{n+1} )

    unsigned int x = blockIdx.x*blockDim.x + threadIdx.x;
    unsigned int y = blockIdx.y*blockDim.y + threadIdx.y;

    // write output vertex



    float dxp = 0 , dyp = 0;

    if ( x >= 1 && x < width )  dxp = d_px[y*stride+x] - d_px[y*stride+(x-1)];

    if ( y >= 1 && y < height ) dyp = d_py[y*stride+x] - d_py[(y-1)*stride+x];

    float divp = dxp + dyp;

    float prev_u = u[y*stride+x];

    int image_size = width*height;

    float sum_WiT_BiT_DiT_qi = 0;

    for(int i = 0 ; i < N_imgs; i++)
    {
        sum_WiT_BiT_DiT_qi += WiT_BiT_DiT_qi[(y*stride+x)+(image_size)*i]
    }


    u[y*stride+x] = prev_u - tau*( -divp + xisqr*WiT_BiT_DiT_qi[y*stride+x]);

    u_[y*stride+x] = 2*u[y*stride+x] - prev;

}


// Wrapper for the __global__ call that sets up the kernel call
extern "C" void launch_kernel_primal_u(float *px, float *py, float* ux_, float *uy_, float epsilon_u, float sigma, float lambda,
                                              unsigned int stride, unsigned int mesh_width, unsigned int mesh_height)
{
    // execute the kernel
    dim3 block(8, 8, 1);
    dim3 grid(mesh_width / block.x, mesh_height / block.y, 1);
    kernel_primalu<<< grid, block>>>(tau, px, py, xisqr, WiT_BiT_DiT_q, width, height, stride);
    cutilCheckMsg("execution failed\n");
}

#endif // #ifndef _SIMPLEGL_KERNEL_H_
