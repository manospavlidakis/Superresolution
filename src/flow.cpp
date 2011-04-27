#include <stdio.h>
#include <iostream>
#include <fstream>
#include <stdlib.h>

#include "flow.h"

//#define UNKNOWN_FLOW_THRESH 1e9
#define MAXCOLS 60
//#define UNKNOWN_FLOW 1e10

//#define max( a, b ) ( ((a) > (b)) ? (a) : (b) )
//#define min( a, b ) ( ((a) < (b)) ? (a) : (b) )

using namespace CVD;
using namespace std;

typedef unsigned char uchar;

#define TAG_FLOAT 202021.25  // check for this when READING the file
#define TAG_STRING "PIEH"

int ncols = 0;

int colorwheel[MAXCOLS][3];


using namespace std;


bool unknown_flow(float u, float v) {
    return (fabs(u) >  UNKNOWN_FLOW_THRESH)
        || (fabs(v) >  UNKNOWN_FLOW_THRESH)
        || isnan(u) || isnan(v);
}

bool unknown_flow_f(float *f) {
    return unknown_flow(f[0], f[1]);
}

void setcols(int r, int g, int b, int k)
{
    colorwheel[k][0] = r;
    colorwheel[k][1] = g;
    colorwheel[k][2] = b;
}

void makecolorwheel()
{
    // relative lengths of color transitions:
    // these are chosen based on perceptual similarity
    // (e.g. one can distinguish more shades between red and yellow
    //  than between yellow and green)
    int RY = 15;
    int YG = 6;
    int GC = 4;
    int CB = 11;
    int BM = 13;
    int MR = 6;
    ncols = RY + YG + GC + CB + BM + MR;
    //printf("ncols = %d\n", ncols);
    if (ncols > MAXCOLS)
        exit(1);
    int i;
    int k = 0;
    for (i = 0; i < RY; i++) setcols(255,	   255*i/RY,	 0,	       k++);
    for (i = 0; i < YG; i++) setcols(255-255*i/YG, 255,		 0,	       k++);
    for (i = 0; i < GC; i++) setcols(0,		   255,		 255*i/GC,     k++);
    for (i = 0; i < CB; i++) setcols(0,		   255-255*i/CB, 255,	       k++);
    for (i = 0; i < BM; i++) setcols(255*i/BM,	   0,		 255,	       k++);
    for (i = 0; i < MR; i++) setcols(255,	   0,		 255-255*i/MR, k++);
}

void computeColor(float fx, float fy, uchar *pix)
{
    if (ncols == 0)
        makecolorwheel();

    float rad = sqrt(fx * fx + fy * fy);
    float a = atan2(-fy, -fx) / M_PI;
    float fk = (a + 1.0) / 2.0 * (ncols-1);
    int k0 = (int)fk;
    int k1 = (k0 + 1) % ncols;
    float f = fk - k0;
    //f = 0; // uncomment to see original color wheel
    for (int b = 0; b < 3; b++) {
        float col0 = colorwheel[k0][b] / 255.0;
        float col1 = colorwheel[k1][b] / 255.0;
        float col = (1 - f) * col0 + f * col1;
        if (rad <= 1)
            col = 1 - rad * (1 - col); // increase saturation with radius
        else
            col *= .75; // out of range
        pix[2 - b] = (int)(255.0 * col);
    }
}




void MotionToColor(float *u, float *v, int height, int width, CVD::Image< Rgb<byte> > &colim, float maxmotion)
{

    int x, y;
    // determine motion range:
    float maxx = -999, maxy = -999;
    float minx =  999, miny =  999;
    float maxrad = -1;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            float fx = u[x+y*width];
            float fy = v[x+y*width];

            if (unknown_flow(fx, fy))
                continue;
            maxx = max(maxx, fx);
            maxy = max(maxy, fy);
            minx = min(minx, fx);
            miny = min(miny, fy);
            float rad = sqrt(fx * fx + fy * fy);
            maxrad = max(maxrad, rad);
        }
    }
    printf("max motion: %.4f  motion range: u = %.3f .. %.3f;  v = %.3f .. %.3f\n",
           maxrad, minx, maxx, miny, maxy);


    if (maxmotion > 0) // i.e., specified on commandline
        maxrad = maxmotion;

    if (maxrad == 0) // if flow == 0 everywhere
        maxrad = 1;

    if (1)
        fprintf(stderr, "normalizing by %g\n", maxrad);

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            float fx = u[x+y*width];
            float fy = v[x+y*width];

            uchar pix[3];

            if (unknown_flow(fx, fy)) {

                CVD::Rgb<CVD::byte> rgbyte(0,0,0);
                colim[ImageRef(x,y)] = rgbyte;

            } else {
                computeColor(fx/maxrad, fy/maxrad, pix);
                CVD::Rgb<CVD::byte> rgbyte(pix[0],pix[1],pix[2]);

                colim[ImageRef(x,y)] = rgbyte;
            }
        }
    }
}

int read_horizontal_vertical_flow(float *u, float *v, int img_no, int N_rows_upimg, int N_cols_upimg)
{

    float *flowvalues_row = (float*)malloc(sizeof(float)*(N_cols_upimg*2));

    char filename[40];
    sprintf(filename,"../data/flows/flow%03d.flo",img_no);

    cout << filename << endl;


    int width_img, height_img;
    float tag;


    FILE *stream = fopen(filename, "rb");
    if (stream == 0)
        printf("ReadFlowFile: could not open %s", filename);

    if ((int)fread(&tag,    sizeof(float), 1, stream) != 1 ||
        (int)fread(&width_img,  sizeof(int),   1, stream) != 1 ||
        (int)fread(&height_img, sizeof(int),   1, stream) != 1)
        printf("ReadFlowFile: problem reading file %s", filename);

    if (tag != TAG_FLOAT) // simple test for correct endian-ness
        printf("ReadFlowFile(%s): wrong tag (possibly due to big-endian machine?)", filename);

    // another sanity check to see that integers were read correctly (99999 should do the trick...)
    if (width_img < 1 || width_img > 99999)
        printf("ReadFlowFile(%s): illegal width %d", filename, width_img);

    if (height_img < 1 || height_img > 99999)
        printf("ReadFlowFile(%s): illegal height %d", filename, height_img);

    int n = 2 * N_cols_upimg;

    for (int y = 0; y < N_rows_upimg; y++)
    {
        if ((int)fread(flowvalues_row, sizeof(float), n, stream) != n)
            printf("ReadFlowFile(%s): file is too short", filename);



        for(int col = 0 ; col < N_cols_upimg; col++)
        {
            u[col+y*N_cols_upimg] = flowvalues_row[2*col+0];
            v[col+y*N_cols_upimg] = flowvalues_row[2*col+1];
//            cout << "u = " << flowvalues_row[2*col] << "v = " << flowvalues_row[2*col+1] << endl;
        }

    }

    char ufilename[20];

    sprintf(ufilename,"u_%03d.txt",img_no);



    if (fgetc(stream) != EOF)
        printf("ReadFlowFile(%s): file is too long", filename);

    fclose(stream);

    ofstream outfile(ufilename);
    for(int i = 0 ; i < N_rows_upimg*N_cols_upimg; i++)
    {
        outfile << u[i] << " ";
    }
    cout << endl;


}


void buildWMatrixBilinearInterpolation(int N_imgs, int N_rows_upimg, int N_cols_upimg, float** valPtr, int** rowPtr, int** colPtr)
{

    float *u, *v;

    u = (float*)malloc(sizeof(float)*N_rows_upimg*N_cols_upimg);
    v = (float*)malloc(sizeof(float)*N_rows_upimg*N_cols_upimg);


    for (int img_no = 1 ; img_no <= N_imgs ; img_no++)
    {


        cout << "Reading the flow values" << endl;
        read_horizontal_vertical_flow(u,v,img_no, N_rows_upimg,N_cols_upimg);

        ImageRef size_upimg(N_cols_upimg,N_rows_upimg);
        CVD::Image< Rgb<byte> > colim(size_upimg);
        float maxmotion = 10;

        MotionToColor(u,v,N_rows_upimg,N_cols_upimg,colim,maxmotion);


        char fname[30];
        sprintf(fname,"flow_image%03d.png",img_no-1);
        std::string str_file = std::string(fname);

        img_save(colim,str_file);


        int index = 0;
        rowPtr[img_no-1][0]=0;

        int row_index = 1;

        for (int row = 0 ; row < N_rows_upimg ; row++)
        {
            for (int col = 0 ; col < N_cols_upimg ; col++)
            {
                // remember width  = N_cols_upimg
                // remember height = N_cols_upimg


                float horizontal_flow = u[col + row*N_cols_upimg];
                float   vertical_flow = v[col + row*N_cols_upimg];

                float x_ = min(N_cols_upimg*1.0f, max(0.0f,col + horizontal_flow));
                float y_ = min(N_rows_upimg*1.0f, max(0.0f,row +   vertical_flow));

                float x_ratio = x_ - floor(x_);
                float y_ratio = y_ - floor(y_);

                valPtr[img_no-1][index+0]  = (1-x_ratio)*(1-y_ratio);
                colPtr[img_no-1][index+0]  = ((int)x_ + (int)y_*N_cols_upimg);

                valPtr[img_no-1][index+1]  = (x_ratio)*(1-y_ratio);
                colPtr[img_no-1][index+1]  = ((int)x_+1 + (int)y_*N_cols_upimg);

                valPtr[img_no-1][index+2]  = (1-x_ratio)*y_ratio;
                colPtr[img_no-1][index+2]  = ((int)x_ + ((int)y_+1)*N_cols_upimg);

                valPtr[img_no-1][index+3]  = (x_ratio)*y_ratio;
                colPtr[img_no-1][index+3]  = ((int)x_+1 + ((int)y_+1)*N_cols_upimg);


                rowPtr[img_no-1][row_index] = index;
                row_index++;
                index = index+4;
            }

        }

        cout << "Read the Matrix values" << endl;

    }

    // Add last element as Nnz in the row index matrix
}


void buildDMatrixLebesgueMeasure(int N_imgs, int N_rows_low_img, int N_cols_upimg, float** DMatvalPtr, int** DMatrowPtr, int** DMatcolPtr)
{

    for (int img_no = 1 ; img_no <= N_imgs ; img_no++)
    {

        int row_index = 1;
        float prev_row = 0;

        for(float row_increment = 0; row_increment <= N_rows_upimg-1 ; row_increment+=scale_factor)
        {

             int prev_row_int = (int)(ceil(prev_row));
             int curr_row_int = (int)(ceil(row_increment));
             float row_vec[curr_row_int-prev_row_int+1];
             row_vec[0] = 1 - left_over_row;
             for (int i = 1; i < curr_row_int-prev_row_int ; i++ )
             {
                 row_vec[i] = 1;
                 sum_row_vec += 1;
             }
             row_vec[curr_row_int-prev_row_int] = scale_factor-sum_row_vec;


            for(float col_increment = 0; col_increment <= N_cols_upimg-1 ; col_increment+=scale_factor)
            {



                int prev_col_int = (int)(ceil(prev_col));
                int curr_col_int = (int)(ceil(col_increment));

                float col_vec[curr_col_int-prev_col_int+1];

                col_vec[0] = 1 - left_over_col;

                sum_row_vec = row_vec[0];

                for (int i = 1; i < curr_col_int-prev_col_int ; i++ )
                {
                    col_vec[i] = 1;
                    sum_col_vec += 1;
                }
                col_vec[curr_col_int-prev_col_int] = scale_factor-sum_col_vec;


                TooN::Vector<float> row_vector(row_vec);
                TooN::Vector<float> col_vector(col_vec);

                TooN::Matrix<>weightMat = row_vector.as_col()*col_vector.as_row();

                int sum = 0 ;
                for (int row_mat = 0 ; row_mat < weightMat.num_rows() ; row_mat++)
                {
                    for (int col_mat = 0 ; col_mat < weightMat.num_cols() ; col_mat++)
                    {
                        sumMat += weightMat(row_mat,col_mat);
                    }
                }

                weightMat = weightMat/ sum;















            }
        }

    }

}
