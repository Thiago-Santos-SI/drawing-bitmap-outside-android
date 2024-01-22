#include "Laplacian.h"
#include <omp.h>
#include <jni.h>
#include <android/bitmap.h>
#include <vector>


void ComputeLaplacian(const float (&u)[XDIM][YDIM], float (&Lu)[XDIM][YDIM], int thread_count)
{

    omp_set_num_threads(thread_count);
#pragma omp parallel for
    for (int i = 1; i < XDIM-1; i++)
        for (int j = 1; j < YDIM-1; j++)
            Lu[i][j] =
                    -4 * u[i][j]
                    + u[i+1][j]
                    + u[i-1][j]
                    + u[i][j+1]
                    + u[i][j-1];

}

void ComputeLaplacianFlip(const float (&u)[XDIM][YDIM], float (&Lu)[XDIM][YDIM], int thread_count)
{

    omp_set_num_threads(thread_count);
#pragma omp parallel for
    for (int i = 1; i < YDIM-1; i++)
        for (int j = 1; j < XDIM-1; j++)
            Lu[i][j] =
                    -4 * u[i][j]
                    + u[i+1][j]
                    + u[i-1][j]
                    + u[i][j+1]
                    + u[i][j-1];

}

void ComputeLaplacianPtrArr(float **u, float **Lu, int thread_count)
{
    omp_set_num_threads(thread_count);
#pragma omp parallel for
    for (int i = 1; i < XDIM-1; i++)
        for (int j = 1; j < YDIM-1; j++)
            Lu[i][j] =
                    -4 * u[i][j]
                    + u[i+1][j]
                    + u[i-1][j]
                    + u[i][j+1]
                    + u[i][j-1];

}

void ComputeBitmapPtrArr(JNIEnv *env, int thread_count, jobject bitmap)
{

    AndroidBitmapInfo info;
    void *pixels;

    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) {
    }

    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
    }

    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) {
    }


    std::vector<uint32_t> coresUnicas;
    int contadorPixels = 0;


    omp_set_num_threads(thread_count);
#pragma omp parallel for
    for (int y = 0; y < info.height; ++y) {
        for (int x = 0; x < info.width; ++x) {
            uint32_t *pixel = (uint32_t *)((char *)pixels + y * info.stride + x * 4);
            uint32_t cor = *pixel;

            if (std::find(coresUnicas.begin(), coresUnicas.end(), cor) == coresUnicas.end()) {
                coresUnicas.push_back(cor);
            }

            contadorPixels++; // Incrementa o contador de pixels
        }
    }

    AndroidBitmap_unlockPixels(env, bitmap);

}





