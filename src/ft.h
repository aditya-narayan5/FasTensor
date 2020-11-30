

/**
 *
 * Email questions to dbin@lbl.gov
 * Scientific Data Management Research Group
 * Lawrence Berkeley National Laboratory
 *
 */

#ifndef ARRAY_UDF_H
#define ARRAY_UDF_H

#include <assert.h>
#include "au_endpoint.h"
#include "au_stencil.h"
#include "au_type.h"
#include "au_array.h"
#include "au_mpi.h"

#ifdef HAS_DASH_ENDPOINT
#include <libdash.h>
#endif
extern int au_mpi_size_global;
extern int au_mpi_rank_global;
extern int au_size;
extern int au_rank;

extern MPI_COMM_TYPE au_mpi_comm_global;
void FT_Init(int argc, char *argv[], MPI_COMM_TYPE au_mpi_comm = MPI_COMM_WORLD_DEFAULT);

void FT_Finalize();

//for some legacy code
#define AU_Init(argc, argv) FT_Init(argc, argv)
#define AU_Finalize() FT_Finalize()

//const auto &AU_Init = FT_Init;

//const auto &AU_Finalize = FT_Finalize;

#endif
