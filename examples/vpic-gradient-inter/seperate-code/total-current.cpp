// overload_array.cpp
// overloading the c++ array subscript operator []
//http://neondataskills.org/HDF5/TimeSeries-Data-In-HDF5-Using-R/

#include <iostream>
#include <stdarg.h>
#include <vector>
#include <stdlib.h>
#include <math.h>       /* ceil  and floor*/
#include <cstring>

#include "array_udf.h"


using namespace std;

void create_xdmf_file(char *xml_output_file, char *dimensions,  char *origin_dxdydz_dimensions, char * origin,  char *dxdydz, char * attribute_name, char *hdf5_file_dataset);

//Define a compund data strcuture 
struct HydroPoint{
  float jx;
  float jy;
  float jz;
  //Some help functions.
  friend std::ostream& operator<<(std::ostream& os, HydroPoint &p)   //test 
  {  
    os << p.jx << '/' << p.jy << '/' << p.jz  << std::endl;  
    return os;  
  }
    friend bool operator == (HydroPoint &lp, HydroPoint& rp)
  {
    return  (lp.jx == rp.jx && lp.jy == rp.jy && lp.jz == rp.jz);
  }

  void set_value(int index, float value){
    switch(index){
      case 0:
        jx = value;
        break;
      case 1:
        jy =value;
        break;
      case 2:
        jz = value;
        break;
      default:
        printf("The object only has three variables !\n");
        break;
    }
  }
};


//Define operation for each vector (JX/JY/JZ)
inline float totalCurrent_UDF(const Stencil<HydroPoint> &hp) 
{
  HydroPoint hpt = hp(0,0,0);
  //printf(" jx,jy,jz = (%f, %f, %f) \n", hpt.jx,  hpt.jy,  hpt.jz);
  return hpt.jx + hpt.jy +hpt.jz;
}


int main(int argc, char *argv[])
{

  char i_file[1024]="/Users/dbin/work/vpic-hdf5/vpic/build/testdir/hydro_hdf5/T.50/hydro_ion_50.h5";
  char o_file[1024]="/Users/dbin/work/vpic-hdf5/vpic/build/testdir/hydro_hdf5/T.50/hydro_ion_50_totalCurrent.h5";
  
  char  group[1024]="/Timestep_50";
  char  dsetX[1024]="/Timestep_50/jx";
  char  dsetY[1024]="/Timestep_50/jy";
  char  dsetZ[1024]="/Timestep_50/jz";
  char  dsetM[1024]="/Timestep_50/totalCurrent";

  //Split array into chunks for parallel processing
  std::vector<int> chunk_size(3) ;
  std::vector<int> overlap_size(3);
  chunk_size[0]   = 32;   chunk_size[1]   = 1;   chunk_size[2]   = 32;  
  overlap_size[0] =  0;   overlap_size[1] = 0;   overlap_size[2] = 0;


   MPI_Init(&argc, &argv);


  
  //Orginal data sets
  Array<float> *JX = new Array<float>(AU_NVS,  AU_HDF5, i_file, group,  dsetX, chunk_size, overlap_size);
  Array<float> *JY = new Array<float>(AU_NVS,  AU_HDF5, i_file, group,  dsetY,  chunk_size, overlap_size);
  Array<float> *JZ = new Array<float>(AU_NVS,  AU_HDF5, i_file, group,  dsetZ, chunk_size, overlap_size);
 
  
  //Create a compound data strucutre to include JX/JY/JZ
  Array<HydroPoint, float> *JXYZ = new Array<HydroPoint, float>(AU_VIRTUAL);
  JXYZ->SetAttributes(JX, JY, JZ); 


  //Output array
  Array<float> *M = new Array<float>(AU_COMPUTED, AU_HDF5, o_file, group, dsetM, chunk_size, overlap_size);

  //Run the code 
  JXYZ->Apply(totalCurrent_UDF, M);  
  

  float       attr_data[2][3];
  hid_t attr_file_id = H5Fopen(i_file, H5F_ACC_RDONLY, H5P_DEFAULT);
  hid_t attr_id = H5Aopen(attr_file_id, "VPIC-ArrayUDF-GEO", H5P_DEFAULT);
  H5Aread(attr_id, H5T_NATIVE_FLOAT, &attr_data);
  H5Aclose(attr_id);

  std::vector<unsigned long long> output_array_size; output_array_size.resize(3);
  output_array_size = M->GetDimSize();
  char xml_output_file[1024], hdf5_file_dataset[1024];
  char dimensions[128], origin_dxdydz_dimensions[128], origin[128], dxdydz[128], attribute_name[128]; 

  sprintf(xml_output_file, "%s.xmf", o_file);
  sprintf(hdf5_file_dataset, "%s:%s", o_file, dsetM);
  sprintf(dimensions, "%lld %lld %lld", output_array_size[0],output_array_size[1], output_array_size[2]);
  sprintf(origin_dxdydz_dimensions, "%lld", 3);
  sprintf(origin,  "%f %f %f", attr_data[0][0], attr_data[0][1], attr_data[0][2]);
  sprintf(dxdydz,  "%f %f %f", attr_data[1][0], attr_data[1][1], attr_data[1][2]);
  sprintf(attribute_name, "%s",  dsetM);
  create_xdmf_file(xml_output_file, dimensions, origin_dxdydz_dimensions, origin,  dxdydz, attribute_name, hdf5_file_dataset);

  //Clear
  delete JX;
  delete JY;
  delete JZ;
  delete JXYZ;
  delete M;

  MPI_Finalize();

  return 0; 
}


const char *xml_body = "<?xml version=\"1.0\"?> \n \
<!DOCTYPE Xdmf SYSTEM \"Xdmf.dtd\" []> \n \
<Xdmf xmlns:xi=\"http://www.w3.org/2001/XInclude\" Version=\"2.0\"> \n \
\t<Domain> \n \
\t\t<Grid GridType=\"Uniform\" Name=\"MESH\"> \n \
\t\t\t<Topology Dimensions=\" %s \" TopologyType=\"3DCoRectMesh\" name=\"topo\"/> \n\
\t\t\t<Geometry Type=\"ORIGIN_DXDYDZ\" name=\"geo\"> \n\
\t\t\t\t<!-- Origin -->  \n\
\t\t\t\t<DataItem Dimensions=\" %s \" Format=\"XML\"> %s </DataItem> \n\
\t\t\t\t<!-- DxDyDz -->  \n\
\t\t\t\t<DataItem Dimensions=\" %s \" Format=\"XML\"> %s </DataItem> \n\
\t\t\t</Geometry> \n\
\t\t\t<Attribute AttributeType=\"Scalar\" Center=\"Node\" Name=\" %s \"> \n\
\t\t\t\t<DataItem  Dimensions=\" %s \" DataType=\"Float\" Precision=\"4\" Format=\"HDF\"> \n\
\t\t\t\t %s \n\
\t\t\t\t</DataItem>  \n\
\t\t\t</Attribute>  \n\
\t\t</Grid>  \n\
\t</Domain> \n\
</Xdmf>";


void create_xdmf_file(char *xml_output_file, char *dimensions,  char *origin_dxdydz_dimensions, char * origin,  char *dxdydz, char * attribute_name, char *hdf5_file_dataset){
  FILE * fp;
  fp = fopen(xml_output_file,"w");
  fprintf(fp, xml_body, dimensions, origin_dxdydz_dimensions, origin, origin_dxdydz_dimensions, dxdydz, attribute_name, dimensions, hdf5_file_dataset);
  fclose (fp); 
}






