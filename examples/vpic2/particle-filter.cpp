// overload_array.cpp
// overloading the c++ array subscript operator []
//http://neondataskills.org/HDF5/TimeSeries-Data-In-HDF5-Using-R/

#include <iostream>
#include <stdarg.h>
#include <vector>
#include <stdlib.h>
#include <math.h> /* ceil  and floor*/
#include <cstring>
#include <optional> //Need c++17

#include "array_udf.h"

using namespace std;

struct Particle
{
    float x;
    float y;
    float z;
    friend std::ostream &operator<<(std::ostream &os, Particle &p) //test
    {
        os << p.x << '/' << p.y << '/' << p.z;
        return os;
    }
    friend bool operator==(Particle &lp, Particle &rp)
    {
        return (lp.x == rp.x && lp.y == rp.y && lp.z == rp.z);
    }
    void set_value(int index, float value)
    {
        switch (index)
        {
        case 0:
            x = value;
            break;
        case 1:
            y = value;
            break;
        case 2:
            z = value;
            break;
        default:
            printf("The object only has three variables !\n");
            exit(-1);
            break;
        }
    }

    float get_value(int index)
    {
        switch (index)
        {
        case 0:
            return x;
            break;
        case 1:
            return y;
            break;
        case 2:
            return z;
            break;
        default:
            printf("The object only has three variables !\n");
            exit(-1);
        }
        return 0;
    }
};

//Find the global Z
inline std::optional<Particle> Filter_UDF(const Stencil<Particle> &p)
{
    if (p(0).x > 200.0 && p(0).y > 0.0 && p(0).z > 0.0)
    {
        return p(0);
    }
    else
    {
        return {};
    }
}

int main(int argc, char *argv[])
{
    int copt, has_set_output_flag = 0;
    int c_size = 3277, o_size = 0;

    char p_file[1024] = "./test-file/electron_50.h5";
    char group[1024] = "/Timestep_50";

    char o_file[1024] = "./test-file/electron_50_filtered.h5";

    int mpi_rank;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

    Array<Particle, float> *P = new Array<Particle, float>(AU_VIRTUAL);

    P->PushBackAttribute(AU_NVS, AU_HDF5, p_file, group, "x", 0);
    P->PushBackAttribute(AU_NVS, AU_HDF5, p_file, group, "y", 0);
    P->PushBackAttribute(AU_NVS, AU_HDF5, p_file, group, "z", 0);

    Array<Particle, float> *PF = new Array<Particle, float>(AU_VIRTUAL);

    PF->PushBackAttribute(AU_COMPUTED, AU_HDF5, o_file, group, "x", 0);
    PF->PushBackAttribute(AU_COMPUTED, AU_HDF5, o_file, group, "y", 0);
    PF->PushBackAttribute(AU_COMPUTED, AU_HDF5, o_file, group, "z", 0);

    //P->SetApplyOptionalOutput();
    P->Apply(Filter_UDF, PF);
    P->ReportTime();

    delete P;
    delete PF;

    MPI_Finalize();
    return 0;
}
