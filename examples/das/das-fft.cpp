//
// This Code is for calculating the FFT/IFFT for DAS data
// It is on top of ArrayUDF with the main strucutre:
//       A: DAS array
//       A->Apply(FFT_UDF) : the TTF_UDF is the main function for FFT/IFFT on each channel
// Please use the "-h" to get the usage information
// Major steps
//      1, get master vector and its FFT on each MPI processes
//      2, Run FFT_UDF on each channel, parallized on all MPI processes
//         2.1  Get data of each channel
//         2.2  FFT
//         2.3  spec Correlation with  master
//         2.4  IFFT
//         2.5  Subset correlation
//
// Author: Bin Dong  2019 (Reviewed by Xin Xing)
//
#include <iostream>
#include <stdarg.h>
#include <vector>
#include <stdlib.h>
#include <math.h> /* ceil  and floor*/
#include <cstring>
#include "array_udf.h"

//Comment out if you don't have FFTW
//In that case, we use Kiss FFT (included already in this distribution)
#define FFTW_LIB_AVAILABLE 1

#ifndef FFTW_LIB_AVAILABLE
#include "fft/kiss_fft.h"
kiss_fft_cpx *fft_in_temp;
kiss_fft_cpx *fft_out_temp;
kiss_fft_cpx *master_vector_fft;
unsigned int fft_in_legnth;
#else
#include <fftw3.h>
fftw_complex *fft_in_temp;
fftw_complex *fft_out_temp;
fftw_complex *master_vector_fft;
unsigned int fft_in_legnth;
#endif

using namespace std;

//Some global variables
unsigned long long m_TIME_SERIESE_LENGTH = 30000;                              //window size
unsigned long long M_TIME_SERIESE_LENGTH_EXTENDED = m_TIME_SERIESE_LENGTH * 2; //size of extended window for FFT/IFFT
unsigned long long x_GATHER_X_CORR_LENGTH = m_TIME_SERIESE_LENGTH * 2 - 1;     //Result size

//When time series is spelit into windows
// following variables are used
int window_batch = 1;
unsigned long long x_GATHER_X_CORR_LENGTH_total = x_GATHER_X_CORR_LENGTH * window_batch;
unsigned long long M_TIME_SERIESE_LENGTH_EXTENDED_total = M_TIME_SERIESE_LENGTH_EXTENDED * window_batch;

//Maste channel
unsigned long long MASTER_INDEX = 0;

/*
 *Some help functions
 */
unsigned long long find_m(unsigned long long minimum_m);
#define NAME_LENGTH 1024
void convert_str_vector(int n, char *str, int *vector);
void printf_help(char *cmd);

//direction: FFTW_FORWARD,  FFTW_BACKWARD
#define FFT_HELP_W(N, fft_in, fft_out, direction)                               \
    {                                                                           \
        fftw_plan fft_p;                                                        \
        fft_p = fftw_plan_dft_1d(N, fft_in, fft_out, direction, FFTW_ESTIMATE); \
        fftw_execute(fft_p);                                                    \
        fftw_destroy_plan(fft_p);                                               \
    }
//direction: 0,  1
#define FFT_HELP_K(N, fft_in, fft_out, direction)       \
    {                                                   \
        kiss_fft_cfg cfg;                               \
        cfg = kiss_fft_alloc(N, direction, NULL, NULL); \
        kiss_fft(cfg, fft_in, fft_out);                 \
        free(cfg);                                      \
    }

//Variable used inside FFT_UDF
//Put here for performnace resason
std::vector<float> gatherXcorr_per_batch;
std::vector<float> gatherXcorr_final;

/*
 * This function is the place implement FFT/IFFT
 *  Actually, it describes the operation per channel
 *  The return value is a coorelation vector
 */
inline std::vector<float> FFT_UDF(const Stencil<float> &c)
{
    for (int bi = 0; bi < window_batch; bi++)
    {
        //Clear memory for new analysis
        memset(fft_in_temp, 0, fft_in_legnth);
        memset(fft_out_temp, 0, fft_in_legnth);

        //Get the input time series on a single channel
        for (unsigned long long i = 0; i < m_TIME_SERIESE_LENGTH; i++)
        {
#ifndef FFTW_LIB_AVAILABLE
            fft_in_temp[i].r = c(i, 0);
            fft_in_temp[i].i = 0;
#else
            fft_in_temp[i][0] = c(i, 0);
            fft_in_temp[i][1] = 0;
#endif
        }

        //FFT on the channel
#ifndef FFTW_LIB_AVAILABLE
        FFT_HELP_K(M_TIME_SERIESE_LENGTH_EXTENDED, fft_in_temp, fft_out_temp, 0);
#else
        FFT_HELP_W(M_TIME_SERIESE_LENGTH_EXTENDED, fft_in_temp, fft_out_temp, FFTW_FORWARD);
#endif
        //specXcorr
        for (unsigned long long j = 0; j < M_TIME_SERIESE_LENGTH_EXTENDED; j++)
        {
#ifndef FFTW_LIB_AVAILABLE
            fft_in_temp[j].r = master_vector_fft[j].r * fft_out_temp[j].r + master_vector_fft[j].i * fft_out_temp[j].i;
            fft_in_temp[j].i = master_vector_fft[j].i * fft_out_temp[j].r - master_vector_fft[j].r * fft_out_temp[j].i;
#else
            fft_in_temp[j][0] = master_vector_fft[j][0] * fft_out_temp[j][0] + master_vector_fft[j][1] * fft_out_temp[j][1];
            fft_in_temp[j][1] = master_vector_fft[j][1] * fft_out_temp[j][0] - master_vector_fft[j][0] * fft_out_temp[j][1];
#endif
        }

        //memset(fft_out_temp, 0, fft_in_legnth);
        //IFFT, result_v also holds the result (only real part for performance)
#ifndef FFTW_LIB_AVAILABLE
        FFT_HELP_K(M_TIME_SERIESE_LENGTH_EXTENDED, fft_in_temp, fft_out_temp, 1);
#else
        FFT_HELP_W(M_TIME_SERIESE_LENGTH_EXTENDED, fft_in_temp, fft_out_temp, FFTW_BACKWARD);
#endif
        //Subset specXcorr
        unsigned long long gatherXcorr_index = 0;
        for (unsigned long long k = M_TIME_SERIESE_LENGTH_EXTENDED - m_TIME_SERIESE_LENGTH + 1; k < M_TIME_SERIESE_LENGTH_EXTENDED; k++)
        {
#ifndef FFTW_LIB_AVAILABLE
            //https://stackoverflow.com/questions/39109615/fftw-c-computes-fft-wrong-compared-to-matlab
            gatherXcorr_per_batch[gatherXcorr_index] = fft_out_temp[k].r / M_TIME_SERIESE_LENGTH_EXTENDED;
#else
            gatherXcorr_per_batch[gatherXcorr_index] = fft_out_temp[k][0] / M_TIME_SERIESE_LENGTH_EXTENDED;
#endif
            gatherXcorr_index++;
        }
        for (unsigned long long l = 0; l < m_TIME_SERIESE_LENGTH; l++)
        {
#ifndef FFTW_LIB_AVAILABLE
            gatherXcorr_per_batch[gatherXcorr_index] = fft_out_temp[l].r / M_TIME_SERIESE_LENGTH_EXTENDED;
#else
            gatherXcorr_per_batch[gatherXcorr_index] = fft_out_temp[l][0] / M_TIME_SERIESE_LENGTH_EXTENDED;
#endif
            gatherXcorr_index++;
        }

        assert(gatherXcorr_index == x_GATHER_X_CORR_LENGTH);
        std::copy_n(gatherXcorr_per_batch.begin(), x_GATHER_X_CORR_LENGTH, gatherXcorr_final.begin() + bi * x_GATHER_X_CORR_LENGTH);
    }
    return gatherXcorr_final;
}

int main(int argc, char *argv[])
{
    char i_file[NAME_LENGTH] = "test-data/fft-test.h5";
    char o_file[NAME_LENGTH] = "test-data/fft-test.arrayudf.h5";
    char group[NAME_LENGTH] = "/"; //both input and output file share the same group and dataset name
    char i_dataset[NAME_LENGTH] = "/white";
    char o_dataset[NAME_LENGTH] = "/Xcorr";

    char chunk_size_str[NAME_LENGTH];
    std::vector<int> ghost_size;
    std::vector<int> chunk_size;
    std::vector<int> strip_size;
    int array_ranks = 2;

    ghost_size.resize(array_ranks);
    ghost_size[0] = 0;
    ghost_size[1] = 0;
    chunk_size.resize(array_ranks);
    chunk_size[0] = 7500;
    chunk_size[1] = 101;
    strip_size.resize(array_ranks);
    strip_size[0] = 7500;
    strip_size[1] = 1;
    m_TIME_SERIESE_LENGTH = chunk_size[0];

    unsigned long long user_window_size;
    int set_window_size_flag = 0;
    int copt, mpi_rank, mpi_size;
    while ((copt = getopt(argc, argv, "o:i:g:t:x:m:w:h")) != -1)
        switch (copt)
        {
        case 'o':
            memset(o_file, 0, sizeof(o_file));
            strcpy(o_file, optarg);
            break;
        case 'i':
            memset(i_file, 0, sizeof(i_file));
            strcpy(i_file, optarg);
            break;
        case 'g':
            memset(group, 0, sizeof(group));
            strcpy(group, optarg);
            break;
        case 't':
            memset(i_dataset, 0, sizeof(i_dataset));
            strcpy(i_dataset, optarg);
            break;
        case 'x':
            memset(o_dataset, 0, sizeof(o_dataset));
            strcpy(o_dataset, optarg);
            break;
        case 'w':
            set_window_size_flag = 1;
            user_window_size = atoll(optarg);
            break;
        case 'm':
            MASTER_INDEX = atoi(optarg);
            break;
        case 'h':
            printf_help(argv[0]);
            exit(0);
            break;
        default:
            printf("Wrong option [%c] for %s \n", copt, argv[0]);
            printf_help(argv[0]);
            exit(-1);
            break;
        }

    //Do some intializatin work
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    //Declare the input and output array
    Array<float, std::vector<float>> *IFILE = new Array<float, std::vector<float>>(AU_NVS, AU_HDF5, i_file, group, i_dataset, chunk_size, ghost_size);
    Array<std::vector<float>> *OFILE = new Array<std::vector<float>>(AU_COMPUTED, AU_HDF5, o_file, group, o_dataset, chunk_size, ghost_size);

    std::vector<unsigned long long> i_file_dim = IFILE->GetDimSize();

    //Find and set chunks_size to split array for parallel processing
    chunk_size[0] = i_file_dim[0];
    if (i_file_dim[1] % mpi_size == 0)
    {
        chunk_size[1] = i_file_dim[1] / mpi_size;
    }
    else
    {
        chunk_size[1] = i_file_dim[1] / mpi_size + 1;
    }
    IFILE->SetChunkSize(chunk_size);

    //Set other parameters for parallel processng
    strip_size[0] = chunk_size[0];         //skip per chunk
    strip_size[1] = 1;                     //per channel
    m_TIME_SERIESE_LENGTH = chunk_size[0]; // window size = chunk_size[0] by default

    //If user provides a window size and it is not cover the whole time domain per channel
    window_batch = 1;
    if (set_window_size_flag && (user_window_size != chunk_size[0]))
    {
        if (user_window_size < chunk_size[0])
        {
            m_TIME_SERIESE_LENGTH = user_window_size;
            if (chunk_size[0] % user_window_size == 0)
            {
                window_batch = chunk_size[0] / user_window_size;
                if (!mpi_rank)
                {
                    printf("user_window_size = %lld, window_batch = %d \n", user_window_size, window_batch);
                }
            }
            else
            {
                //Todo: consider impact of this
                window_batch = chunk_size[0] / user_window_size + 1;
            }
        }
        else
        {
            printf("Window size (-w) must be smaller than chunk_size[0] (-c) \n");
            exit(-1);
        }
    }

    //Find out the result size and extended vector size of FFT.
    x_GATHER_X_CORR_LENGTH = 2 * m_TIME_SERIESE_LENGTH - 1;
    M_TIME_SERIESE_LENGTH_EXTENDED = find_m(x_GATHER_X_CORR_LENGTH);
    if (!mpi_rank)
        printf("M value is : %lld\n", M_TIME_SERIESE_LENGTH_EXTENDED);

    x_GATHER_X_CORR_LENGTH_total = x_GATHER_X_CORR_LENGTH * window_batch;
    M_TIME_SERIESE_LENGTH_EXTENDED_total = M_TIME_SERIESE_LENGTH_EXTENDED * window_batch;

    //Allocate some space for FFT/iFFT (for each channel and master channel)
#ifndef FFTW_LIB_AVAILABLE
    fft_in_legnth = M_TIME_SERIESE_LENGTH_EXTENDED * sizeof(kiss_fft_cpx);
    fft_in_temp = (kiss_fft_cpx *)malloc(fft_in_legnth);
    fft_out_temp = (kiss_fft_cpx *)malloc(fft_in_legnth);
    master_vector_fft = (kiss_fft_cpx *)malloc(sizeof(kiss_fft_cpx) * M_TIME_SERIESE_LENGTH_EXTENDED_total);
    memset(master_vector_fft, 0, sizeof(kiss_fft_cpx) * M_TIME_SERIESE_LENGTH_EXTENDED_total);
#else
    fft_in_legnth = sizeof(fftw_complex) * M_TIME_SERIESE_LENGTH_EXTENDED;
    fft_in_temp = (fftw_complex *)malloc(fft_in_legnth);
    fft_out_temp = (fftw_complex *)malloc(fft_in_legnth);
    master_vector_fft = (fftw_complex *)malloc(sizeof(fftw_complex) * M_TIME_SERIESE_LENGTH_EXTENDED_total);
    memset(master_vector_fft, 0, sizeof(fftw_complex) * M_TIME_SERIESE_LENGTH_EXTENDED_total);
#endif

    if (fft_in_temp == NULL || fft_out_temp == NULL || master_vector_fft == NULL)
    {
        printf("not enough memory, in %s:%d\n", __FILE__, __LINE__);
        exit(-1);
    }

    //Get the mater vector and its fft
    memset(fft_in_temp, 0, fft_in_legnth);
    memset(fft_out_temp, 0, fft_in_legnth);
    std::vector<float> master_v_per_batch;
    master_v_per_batch.resize(m_TIME_SERIESE_LENGTH);
    std::vector<unsigned long long> master_start, master_end;
    master_start.resize(2);
    master_end.resize(2);
    for (int bi = 0; bi < window_batch; bi++)
    {
        master_start[0] = 0 + bi * m_TIME_SERIESE_LENGTH;
        master_start[1] = MASTER_INDEX;
        master_end[0] = master_start[0] + m_TIME_SERIESE_LENGTH - 1;
        master_end[1] = MASTER_INDEX;
        //Get master chunk's data
        IFILE->ReadData(master_start, master_end, master_v_per_batch);
        //Get the FFT of master
        for (int i = 0; i < m_TIME_SERIESE_LENGTH; i++)
        {
#ifndef FFTW_LIB_AVAILABLE
            fft_in_temp[i].r = master_v_per_batch[i]; //IFILE->operator()(m_TIME_SERIESE_LENGTH *bi + i, MASTER_INDEX);
            fft_in_temp[i].i = 0;
#else
            fft_in_temp[i][0] = master_v_per_batch[i]; //IFILE->operator()(m_TIME_SERIESE_LENGTH *bi + i, MASTER_INDEX);
            fft_in_temp[i][1] = 0;
#endif
        }
#ifndef FFTW_LIB_AVAILABLE
        FFT_HELP_K(M_TIME_SERIESE_LENGTH_EXTENDED, fft_in_temp, fft_out_temp, 0)
#else
        FFT_HELP_W(M_TIME_SERIESE_LENGTH_EXTENDED, fft_in_temp, fft_out_temp, FFTW_FORWARD)
#endif
        for (int j = 0; j < M_TIME_SERIESE_LENGTH_EXTENDED; j++)
        {
#ifndef FFTW_LIB_AVAILABLE
            master_vector_fft[bi * M_TIME_SERIESE_LENGTH_EXTENDED + j].r = fft_out_temp[j].r;
            master_vector_fft[bi * M_TIME_SERIESE_LENGTH_EXTENDED + j].i = fft_out_temp[j].i;
#else
            master_vector_fft[bi * M_TIME_SERIESE_LENGTH_EXTENDED + j][0] = fft_out_temp[j][0];
            master_vector_fft[bi * M_TIME_SERIESE_LENGTH_EXTENDED + j][1] = fft_out_temp[j][1];
#endif
        }
    }

    //Set the strip size and output vector size before the run
    IFILE->SetApplyStripSize(strip_size);
    IFILE->SetOutputVector(x_GATHER_X_CORR_LENGTH_total, 0);

    //Allocate spaces for FFT_UDF for performance
    gatherXcorr_per_batch.resize(x_GATHER_X_CORR_LENGTH);
    gatherXcorr_final.resize(x_GATHER_X_CORR_LENGTH_total);

    //Run FFT
    IFILE->Apply(FFT_UDF, OFILE);
    IFILE->ReportTime();

    delete IFILE;
    delete OFILE;

    free(fft_in_temp);
    free(fft_out_temp);
    MPI_Finalize();
    return 0;
}

//It is power of 2 and greater than minimum_m
unsigned long long find_m(unsigned long long minimum_m)
{
    unsigned long long m = minimum_m;
    while ((minimum_m & (minimum_m - 1)) != 0)
    {
        minimum_m = minimum_m + 1;
    }
    return minimum_m;
}

void convert_str_vector(int n, char *str, int *vector)
{
    int i;
    char *pch;
    char temp[NAME_LENGTH];

    if (n == 1)
    {
        vector[0] = atoi(str);
    }
    else
    {
        strcpy(temp, str);
        pch = strtok(temp, ",");

        i = 0;
        while (pch != NULL)
        {
            //printf("%s \n", pch);
            vector[i] = atoi(pch);
            pch = strtok(NULL, ",");
            i++;
        }
    }
    return;
}

void printf_help(char *cmd)
{
    char *msg = (char *)"Usage: %s [OPTION]\n\
      	  -h help (--help)\n\
          -i input file\n\
          -o output file\n\
	      -g group name (path) for both input and output \n\
          -t dataset name for intput time series \n\
          -x dataset name for output correlation \n\
          -w window size (only used when window size is different from chunk_size[0]) \n\
          -m index of master channel (0 by default )\n\
          Example: mpirun -n 1 %s -i ./test-data/fft-test.h5 -o ./test-data/fft-test.arrayudf.h5  -g / -t /white -x /Xcorr\n";

    fprintf(stdout, msg, cmd, cmd);
}
