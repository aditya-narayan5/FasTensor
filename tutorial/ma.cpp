/**
 *
 * Author:  Bin Dong, dbin@lbl.gov
 * Scientific Data Management Research Group
 * Lawrence Berkeley National Laboratory
 *
 */

#include <iostream>
#include <stdarg.h>
#include <vector>
#include <stdlib.h>
#include "ft.h"

using namespace std;
using namespace FT;

//UDF One: duplicate the original data
inline Stencil<float> udf_ma(const Stencil<float> &iStencil)
{
    Stencil<float> oStencil;
    oStencil = (iStencil(0, -1) + iStencil(0, 0) + iStencil(0, 1)) / 3.0;
    return oStencil;
}

int main(int argc, char *argv[])
{
    //Init the MPICH, etc.
    AU_Init(argc, argv);

    // set up the chunk size and the overlap size
    std::vector<int> chunk_size = {4, 4};
    std::vector<int> overlap_size = {1, 1};

    //Input data
    Array<float> *A = new Array<float>("EP_HDF5:tutorial.h5:/data", chunk_size, overlap_size);

    //Result data
    Array<float> *B = new Array<float>("EP_HDF5:tutorial_ma.h5:/data");

    //Run
    A->Apply(udf_ma, B);

    //Clear
    delete A;
    delete B;

    AU_Finalize();

    return 0;
}
