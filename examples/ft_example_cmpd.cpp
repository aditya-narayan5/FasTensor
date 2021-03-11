
/*
****************************

FasTensor (FT) Copyright (c) 2021, The Regents of the University of
California, through Lawrence Berkeley National Laboratory (subject to
receipt of any required approvals from the U.S. Dept. of Energy). 
All rights reserved.

If you have questions about your rights to use or distribute this software,
please contact Berkeley Lab's Intellectual Property Office at
IPO@lbl.gov.

NOTICE.  This Software was developed under funding from the U.S. Department
of Energy and the U.S. Government consequently retains certain rights.  As
such, the U.S. Government has been granted for itself and others acting on
its behalf a paid-up, nonexclusive, irrevocable, worldwide license in the
Software to reproduce, distribute copies to the public, prepare derivative 
works, and perform publicly and display publicly, and to permit others to do so.


****************************


*** License Agreement ***

FasTensor (FT) Copyright (c) 2021, The Regents of the University of
California, through Lawrence Berkeley National Laboratory (subject to
receipt of any required approvals from the U.S. Dept. of Energy). 
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

(1) Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

(2) Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

(3) Neither the name of the University of California, Lawrence Berkeley
National Laboratory, U.S. Dept. of Energy nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.


THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

You are under no obligation whatsoever to provide any bug fixes, patches,
or upgrades to the features, functionality or performance of the source
code ("Enhancements") to anyone; however, if you choose to make your
Enhancements available either publicly, or directly to Lawrence Berkeley
National Laboratory, without imposing a separate written license agreement
for such Enhancements, then you hereby grant the following license: a
non-exclusive, royalty-free perpetual license to install, use, modify,
prepare derivative works, incorporate into other computer software,
distribute, and sublicense such enhancements or derivative works thereof,
in binary and source code form.
*/

/**
 *
 * Author: Bin Dong dbin@lbl.gov
 * Web: https://crd.lbl.gov/bin-dong
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

struct InputStruct
{
    AU_UDT_INIT(InputStruct)
    double d1;
    float f1;
    int i1;
    double d2;
    double d3
};

struct OutputStruct
{
    AU_UDT_INIT(OutputStruct)
    int i;
    float f;
    double d_ave;
};

//UDF One: duplicate the original data
inline Stencil<OutputStruct> udf_vds(const Stencil<InputStruct> &iStencil)
{
    OutputStruct ovds;
    Stencil<OutputStruct> oStencil;

    ovds.i = iStencil(0, 0).i1;
    ovds.f = iStencil(0, 0).f1;
    ovds.d_ave = (iStencil(0, 0).d1 + iStencil(0, 0).d2 + iStencil(0, 0).d3) / 3.0;

    oStencil = ovds;
    return oStencil;
}

int main(int argc, char *argv[])
{
    //Init the MPICH, etc.
    AU_Init(argc, argv);

    // set up the chunk size and the overlap size
    std::vector<int> chunk_size = {4};
    std::vector<int> overlap_size = {0};

    //Input data
    Array<InputStruct> *A = new Array<InputStruct>(chunk_size, overlap_size, "EP_HDF5:./test-data/cmpd.h5:/DS1");

    std::cout << A->GetValue(0, 0) << "\n";

    //Result data
    Array<OutputStruct> *B = new Array<OutputStruct>();
    B->PushBackAttribute<int>("EP_HDF5:./test-data/cmpd-output.h5:/i");
    B->PushBackAttribute<float>("EP_HDF5:./test-data/cmpd-output.h5:/f");
    B->PushBackAttribute<double>("EP_HDF5:./test-data/cmpd-output.h5:/ave");

    B->PrintEndpointInfo();

    //Run
    A->Apply(udf_vds, B);

    //Clear
    delete A;
    delete B;

    AU_Finalize();

    return 0;
}
