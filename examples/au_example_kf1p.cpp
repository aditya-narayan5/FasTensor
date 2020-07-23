
/**
 *
 * Email questions to dbin@lbl.gov
 * Scientific Data Management Research Group
 * Lawrence Berkeley National Laboratory
 *
 */

#include <iostream>
#include <stdarg.h>
#include <vector>
#include <stdlib.h>
#include "au.h"

using namespace std;
using namespace AU;

int rows_chunk = 11648;
int cols_chunk = 60000;

//UDF One: duplicate the original data
inline Stencil<double> udf_kf1p(const Stencil<short> &iStencil)
{
    Stencil<double> oStencil;
    double oDouble = iStencil(0, 0) * 2.0;
    oStencil = oDouble;
    return oStencil;
}

int main(int argc, char *argv[])
{
    //Init the MPICH, etc.
    AU_Init(argc, argv);

    // set up the chunk size and the overlap size
    // 11648, 30000 for each dataset
    std::vector<int> chunk_size(2);
    chunk_size[0] = rows_chunk;
    chunk_size[1] = cols_chunk;
    std::vector<int> overlap_size = {0, 0};

    //Input data
    Array<short> *A = new Array<short>("EP_DIR:EP_TDMS:/Users/dbin/work/arrayudf-git-svn-test-on-bitbucket/examples/das/tdms-dir", chunk_size, overlap_size);
    A->EndpointControl(DIR_MERGE_INDEX, "1");
    A->EndpointControl(DIR_SUB_CMD_ARG, "BINARY_SET_SIZE:11648,30000");


    //Result data
    Array<double> *B = new Array<double>("EP_DIR:EP_TDMS:/Users/dbin/work/arrayudf-git-svn-test-on-bitbucket/examples/das/tdms-dir-dec");

    //Run
    A->Apply(udf_kf1p, B);

    //Clear
    delete A;
    delete B;

    AU_Finalize();

    return 0;
}
