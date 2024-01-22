#pragma once

#define YDIM 4096
#define XDIM 4096

#define TESTITRS 10

void ComputeLaplacian(const float (&u)[XDIM][YDIM], float (&Lu)[XDIM][YDIM], int);

void ComputeLaplacianPtrArr(float **u, float **Lu, int);

void ComputeLaplacianFlip(const float (&u)[XDIM][YDIM], float (&Lu)[XDIM][YDIM], int);

void ComputeBitmapPtrArr(const float (&u)[XDIM][YDIM], float (&Lu)[XDIM][YDIM], int);

