/**
 *ArrayUDF Copyright (c) 2017, The Regents of the University of California, through Lawrence Berkeley National Laboratory (subject to receipt of any required approvals from the U.S. Dept. of Energy).  All rights reserved.
 *
 *If you have questions about your rights to use or distribute this software, please contact Berkeley Lab's Innovation & Partnerships Office at  IPO@lbl.gov.
 *
 * NOTICE. This Software was developed under funding from the U.S. Department of Energy and the U.S. Government consequently retains certain rights. As such, the U.S. Government has been granted for itself and others acting on its behalf a paid-up, nonexclusive, irrevocable, worldwide license in the Software to reproduce, distribute copies to the public, prepare derivative works, and perform publicly and display publicly, and to permit other to do so. 
 *
 */

/**
 *
 * Email questions to {dbin, kwu, sbyna}@lbl.gov
 * Scientific Data Management Research Group
 * Lawrence Berkeley National Laboratory
 *
 */

//see au.h for its definations
extern int au_mpi_size_global;
extern int au_mpi_rank_global;

#ifndef ARRAY_UDF_ARRAY_H
#define ARRAY_UDF_ARRAY_H

#include <assert.h>
#include "mpi.h"
#include "au_endpoint.h"
#include "au_stencil.h"
#include "au_utility.h"
#include "au_type.h"
#include "au_endpoint_factory.h"
#include "au_mpi.h"

namespace AU
{
template <class T, class AttributeType = T>
class Array
{
private:
  /**
   * @brief array_data_endpoint_info has the format
   *        AuDataEndpointType : [other information]
   *        [other information] -- AU_HDF5 --> FileName:GroupName:DatasetName
   *                            -- AU_NETCDF --> Todo
   *                            -- AU_AUDIOS --> Todo
   *                            -- AU_BINARY --> Todo
   *                            -- AU_VIRTUAL --> NULL
   *                            -- AU_IARRAY  --> NULL
   *                            -- AU_CXXVECTOR --> NULL
   */
  std::string array_data_endpoint_info;
  Endpoint *endpoint;

  //Below are info about endpooint
  int data_dims;                                     //The number of dimensioins of data in endpoint
  std::vector<unsigned long long> data_size;         //The size of each dimension (global, extracted from data of endpoint
  std::vector<int> data_chunk_size;                  //size of each chunk (user provide)
  std::vector<int> data_overlap_size;                //size of overlapping  (user provide)
  std::vector<unsigned long long> data_chunked_size; //The number of chunks per dimenstion
  int data_auto_chunk_dim_index;
  unsigned long long data_total_chunks; //The total number of chunks (global)

  std::vector<unsigned long long> current_chunk_start_offset; //Start offset on disk
  std::vector<unsigned long long> current_chunk_end_offset;   //End offset on disk
  std::vector<unsigned long long> current_chunk_size;         //Size of the chunk, euqal to end_offset - start_offset
  unsigned long long current_chunk_cells;                     //The number of cells in current chunk

  std::vector<unsigned long long> current_result_chunk_start_offset; //Start offset on disk
  std::vector<unsigned long long> current_result_chunk_end_offset;   //End offset on disk
  unsigned long long current_result_chunk_cells;                     //The number of cells in current chunk

  std::vector<unsigned long long> current_chunk_ol_start_offset; //Start offset on disk with overlapping
  std::vector<unsigned long long> current_chunk_ol_end_offset;   //End offset on disk with overlapping
  std::vector<unsigned long long> current_chunk_ol_size;         //Size of the chunk, euqal to end_offset - start_offset
  unsigned long long current_chunk_ol_cells;                     //The number of cells in current chunk

  int current_chunk_id; //Id of the current chunk (in memory) to apply UDF

  std::vector<unsigned long long> skip_size;        //Size to ship after each operation
  std::vector<unsigned long long> skiped_dims_size; //Size of the data after
  std::vector<unsigned long long> skiped_chunks;    //# of chunks after skip
  std::vector<int> skiped_chunk_size;               //Size of each chunk after skip

  std::vector<T> current_chunk_data; //Pointer to data of current chunk

  std::vector<long long> ol_origin_offset; //Size of the chunk, euqal to end_offset - start_offset

  std::vector<unsigned long long> view_start_offset;
  std::vector<unsigned long long> view_size;
  std::vector<unsigned long long> view_orginal_data_dims_size;


  std::vector<T> mirror_values;

  //Flag variable
  bool skip_flag = false;
  bool view_flag = false;
  bool cpp_vec_flag = false;
  bool virtual_array_flag = false;
  bool save_result_flag = true;
  bool reverse_apply_direction_flag = false;
  bool mirror_value_flag  = false;
  bool apply_replace_flag = false;
  //help variable
  AU_WTIME_TYPE t_start, time_read = 0, time_udf = 0, time_write = 0, time_create = 0, time_sync = 0, time_nonvolatile = 0;

public:
  /**
   * @brief Construct a new Array object for Write
   * The data can be cached or dumped later
   */
  Array(){

  };

  //Below are three constructors for file based constructor

  /**
   * @brief Construct a new Array object for either Input or Output
    *       For Input, data_endpoint is opened before Apply
    *       For Ouput, data_endpoint is created during Apply
    * @param data_endpoint file information, ("AuDataEndpointType + [file name]")
    *        data_endpoint  get chunk infro from Apply
    *          
    */
  Array(std::string data_endpoint)
  {
    array_data_endpoint_info = data_endpoint;
    endpoint = EndpointFactory::NewEndpoint(data_endpoint);
  }

  /**
   * @brief Construct a new Array object for read, as Input of Apply 
   * 
   * @param data_endpoint file information ("AuDataEndpointType + file name") 
   * @param auto_chunk_dim_index  fileinfo is chunked on auto_chunk_dim_index
   */
  Array(std::string data_endpoint, int auto_chunk_dim_index)
  {
    array_data_endpoint_info = data_endpoint;
    data_auto_chunk_dim_index = auto_chunk_dim_index;
    endpoint = EndpointFactory::NewEndpoint(data_endpoint);
  }

  /**
   * @brief Construct a new Array object for read, as Input of Apply
   * 
   * @param data_endpoint contains file info, ("AuDataEndpointType + file name")
   * @param cs , chunk size
   * @param os , ghost size
   */
  Array(std::string data_endpoint, std::vector<int> cs, std::vector<int> os)
  {
    array_data_endpoint_info = data_endpoint;
    data_chunk_size = cs;
    data_overlap_size = os;
    endpoint = EndpointFactory::NewEndpoint(data_endpoint);
  }

  /**
   * @brief Construct a new Array object from in-memory vector
   *        The data are assumed to be 1D too here
   * @param data_vector_endpoint the data to intialize 
   */
  Array(std::vector<T> &data_vector_endpoint)
  {
  }

  /**
   * @brief Construct a new Array object
   * 
   * @param data_vector_endpoint 
   * @param cs 
   * @param os 
   */
  Array(std::vector<T> &data_vector_endpoint, std::vector<int> cs, std::vector<int> os)
  {
  }

  void CalculateChunkSize(std::vector<unsigned long long> &data_size, std::vector<unsigned long long> &chunk_size)
  {
  }

  

  void InitializeApplyInput()
  {
    //Map T to AuEndpointDataType
    AuEndpointDataType data_element_type = InferDataType<T>();
    endpoint->SetDataElementType(data_element_type);

    //Read the metadata (rank, dimension size) from endpoint
    endpoint->ExtractMeta();
    data_size = endpoint->GetDimensions();
    data_dims = data_size.size();

    current_chunk_start_offset.resize(data_dims);
    current_chunk_end_offset.resize(data_dims);
    current_chunk_size.resize(data_dims);

    current_result_chunk_start_offset.resize(data_dims);
    current_result_chunk_end_offset.resize(data_dims);

    current_chunk_ol_start_offset.resize(data_dims);
    current_chunk_ol_end_offset.resize(data_dims);
    current_chunk_ol_size.resize(data_dims);

    data_chunked_size.resize(data_dims);
    ol_origin_offset.resize(data_dims);

    data_total_chunks = 1;

    for (int i = 0; i < data_dims; i++)
    {
      if (data_size[i] % data_chunk_size[i] == 0)
      {
        data_chunked_size[i] = data_size[i] / data_chunk_size[i];
      }
      else
      {
        data_chunked_size[i] = data_size[i] / data_chunk_size[i] + 1;
      }
      data_total_chunks = data_total_chunks * data_chunked_size[i];
    }

    //#ifdef DEBUG
    if (au_mpi_rank_global == 0)
    {
      endpoint->PrintInfo();
      PrintVector("   data size", data_size);
      PrintVector("  chunk size", data_chunk_size);
      PrintVector("overlap size", data_overlap_size);
      PrintScalar("Total chunks", data_total_chunks);
    }

    current_chunk_id = au_mpi_rank_global; //Each process deal with one chunk one time, starting from its rank
  }

  /**
   * @brief print endpoint info of this array
   * 
   */
  void PrintEndpointInfo()
  {
    endpoint->PrintInfo();
  }

  template <class UDFOutputType, class BType = UDFOutputType, class BAttributeType = BType>
  void Apply(Stencil<UDFOutputType> (*UDF)(const Stencil<T> &), Array<BType, BAttributeType> *B = nullptr)
  {

    //Set up the input data for LoadNextChunk
    InitializeApplyInput();

    std::vector<UDFOutputType> current_result_chunk_data;
    unsigned long long current_result_chunk_data_size = 1;

    t_start = AU_WTIME;
    int load_ret = LoadNextChunk(current_result_chunk_data_size);
    current_result_chunk_data.resize(current_result_chunk_data_size);
    time_read = time_read + (AU_WTIME - t_start);

    unsigned long long result_vector_index = 0;

    while (load_ret == 1)
    {
      unsigned long long cell_target_g_location_rm;
      result_vector_index = 0;

      //unsigned long long lrm;
      t_start = AU_WTIME;

#if defined(_OPENMP)
      size_t *prefix;
#endif

#if defined(_OPENMP)
#pragma omp parallel
#endif
      {
        std::vector<unsigned long long> cell_coordinate(data_dims, 0), cell_coordinate_ol(data_dims, 0), global_cell_coordinate(data_dims, 0);
        unsigned long long offset_ol;
        Stencil<T> cell_target(0, &current_chunk_data[0], cell_coordinate_ol, current_chunk_ol_size);
        Stencil<UDFOutputType> cell_return_stencil;
        UDFOutputType cell_return_value;
        unsigned long long cell_target_g_location_rm;
        int is_mirror_value = 0;
        std::vector<unsigned long long> skip_chunk_coordinate(data_dims, 0), skip_chunk_coordinate_start(data_dims, 0);
        int skip_flag_on_cell = 0;

#if defined(_OPENMP)
        int ithread = omp_get_thread_num();
        int nthreads = omp_get_num_threads();
#pragma omp single
        {
          prefix = new size_t[nthreads + 1];
          prefix[0] = 0;
        }
        std::vector<UDFOutputType> vec_private;
#endif

#if defined(_OPENMP)
#pragma omp for schedule(static) nowait
#endif
        for (unsigned long long i = 0; i < current_chunk_cells; i++)
        {
          ROW_MAJOR_ORDER_REVERSE_MACRO(i, current_chunk_size, current_chunk_size.size(), cell_coordinate)

          if (skip_flag == 1)
          {
            skip_flag_on_cell = 0;
            for (int i = 0; i < data_dims; i++)
            {
              //The coordinate of the skip chunk this coordinate belong to
              skip_chunk_coordinate[i] = std::floor(cell_coordinate[i] / skip_size[i]);
              skip_chunk_coordinate_start[i] = skip_chunk_coordinate[i] * skip_size[i]; //first cell's coodinate of this skip chunk
              if (skip_chunk_coordinate_start[i] != cell_coordinate[i])
              { //we only run on the first  element of this skip chunk
                skip_flag_on_cell = 1;
                break;
              }
            }

            if (skip_flag_on_cell == 1)
              continue; //          for (unsigned long long i = 0; i < current_chunk_cells; i++)
            assert(i < current_chunk_cells);
          }

          //Get the coodinate with overlapping
          //Also, get the global coodinate of the cell in original array
          for (int ii = 0; ii < data_dims; ii++)
          {
            if (cell_coordinate[ii] + ol_origin_offset[ii] < current_chunk_ol_size[ii])
            {
              cell_coordinate_ol[ii] = cell_coordinate[ii] + ol_origin_offset[ii];
            }
            else
            {
              cell_coordinate_ol[ii] = current_chunk_ol_size[ii] - 1;
            }
            //get the global coordinate
            global_cell_coordinate[ii] = current_chunk_start_offset[ii] + cell_coordinate[ii];
          }

          //Update the offset with overlapping
          ROW_MAJOR_ORDER_MACRO(current_chunk_ol_size, current_chunk_ol_size.size(), cell_coordinate_ol, offset_ol);
          cell_target.SetLocation(offset_ol, cell_coordinate_ol, cell_coordinate, current_chunk_size, ol_origin_offset, current_chunk_ol_size);
          //Set the global coodinate of the cell as the ID of the cell
          //Disable it for performance.
          //RowMajorOrder(data_dims_size, global_cell_coordinate)
          //ROW_MAJOR_ORDER_MACRO(data_dims_size, data_dims_size.size(), global_cell_coordinate, cell_target_g_location_rm)
          //cell_target.set_my_g_location_rm(cell_target_g_location_rm);
           is_mirror_value = 0;
          if (mirror_value_flag)
          {
            for (int iii = 0; iii < mirror_values.size(); iii++)
            {
              if (current_chunk_data[offset_ol] == mirror_values[iii])
              {
                is_mirror_value = 1;
                break;
              }
            }
          }

          //Just for the test of performnace
          //if(cell_coordinate[0] >= 16383 || cell_coordinate[1] >= 16383)

          if (is_mirror_value == 0)
          {

            cell_return_stencil = UDF(cell_target); // Called by C++
            cell_return_value = cell_return_stencil.get_value();
          }
          else
          {
            //This is a mirrow value, copy it into result directly
            //Using memcpy to avail error in template of T
            if (sizeof(cell_return_value) == sizeof(T))
            {
              memset(&cell_return_value, 0, sizeof(T));
              std::memcpy(&cell_return_value, &current_chunk_data[offset_ol], sizeof(T));
            }
            //cell_return_value = current_chunk_data[offset_ol];
          }

          if (save_result_flag)
          {
            if (skip_flag)
            {
#if defined(_OPENMP)
              vec_private.push_back(cell_return_value);
#else
              current_result_chunk_data[result_vector_index] = cell_return_value;
              result_vector_index = result_vector_index + 1;
#endif
              //When skip_flag is set, there is no need to have apply_replace_flag
              //Todo: in future
              //if(apply_replace_flag == 1){
              //  current_chunk_data[i] = cell_return_value; //Replace the orginal data
              //}
            }
            else
            {
              current_result_chunk_data[i] = cell_return_value; //cell_return =  cell_return.
              if (apply_replace_flag == 1)
              {
                std::memcpy(&current_chunk_data[offset_ol], &cell_return_value, sizeof(T));
              }
            }
          }
        } //end for loop, finish the processing on a single chunk in row-major direction
#if defined(_OPENMP)
        prefix[ithread + 1] = vec_private.size();
#pragma omp barrier
#pragma omp single
        {
          for (int i = 1; i < (nthreads + 1); i++)
          {
            prefix[i] += prefix[i - 1];
          }
          if (current_result_chunk_data.size() != prefix[nthreads])
          {
            std::cout << "Wrong output size ! prefix[nthreads] =" << prefix[nthreads] << ", current.size() = " << current_result_chunk_data.size() << " \n ";
          }
        } //end of omp for
        std::copy(vec_private.begin(), vec_private.end(), current_result_chunk_data.begin() + prefix[ithread]);
        clear_vector(vec_private);
#endif
      } //end of omp parallel

#if defined(_OPENMP)
      delete[] prefix;
#endif
      time_udf = AU_WTIME - t_start + time_udf;

      PrintVector("current_result_chunk_data: ", current_result_chunk_data);
      //////////////////////////////////////
      //end of processing on a single chunk
      /////////////////////////////////////

      t_start = AU_WTIME;
      FinalizeApplyOutput();
      time_write = time_write + AU_WTIME - t_start;

      t_start = AU_WTIME;
      load_ret = LoadNextChunk(current_result_chunk_data_size);
      current_result_chunk_data.resize(current_result_chunk_data_size);
      time_read = time_read + AU_WTIME- t_start;

    } // end of while:: no more chunks to process

    return;
  }

  void FinalizeApplyOutput()
  {
  }

  /**
   * @brief Load the next chunk
   * 
   * @param result_vector_size 
   * @return 
   *     1, data read into   local_chunk_data
   *     0, end of file (no data left to handle)
   *    -1: error happen
   */
  int LoadNextChunk(unsigned long long &result_vector_size)
  {
    result_vector_size = 0;
    if (current_chunk_id >= data_total_chunks || current_chunk_id < 0)
    {
      return 0;
    }

    current_chunk_cells = 1;
    current_result_chunk_cells = 1;
    current_chunk_ol_cells = 1;

    std::vector<unsigned long long> chunk_coordinate = RowMajorOrderReverse(current_chunk_id, data_chunked_size);
    std::vector<unsigned long long> skiped_chunk_coordinate;
    if (skip_flag)
      skiped_chunk_coordinate = RowMajorOrderReverse(current_chunk_id, skiped_chunks);

    //calculate the start and end of a chunk
    for (int i = 0; i < data_dims; i++)
    {
      if (data_chunk_size[i] * chunk_coordinate[i] < data_size[i])
      {
        current_chunk_start_offset[i] = data_chunk_size[i] * chunk_coordinate[i];
      }
      else
      {
        current_chunk_start_offset[i] = data_size[i];
      }

      if (current_chunk_start_offset[i] + data_chunk_size[i] - 1 < data_size[i])
      {
        current_chunk_end_offset[i] = current_chunk_start_offset[i] + data_chunk_size[i] - 1;
      }
      else
      {
        current_chunk_end_offset[i] = data_size[i] - 1;
      }

      assert((current_chunk_end_offset[i] - current_chunk_start_offset[i] + 1 >= 0));
      current_chunk_size[i] = current_chunk_end_offset[i] - current_chunk_start_offset[i] + 1;
      current_chunk_cells = current_chunk_cells * current_chunk_size[i];

      //Deal with the result chunks size
      if (!skip_flag)
      {
        current_result_chunk_start_offset[i] = current_chunk_start_offset[i];
        current_result_chunk_end_offset[i] = current_chunk_end_offset[i];
        current_result_chunk_cells = current_chunk_cells;
      }
      else
      {
        if (skiped_chunk_coordinate[i] * skiped_chunk_size[i] < skiped_dims_size[i])
        {
          current_result_chunk_start_offset[i] = skiped_chunk_coordinate[i] * skiped_chunk_size[i];
        }
        else
        {
          current_result_chunk_start_offset[i] = skiped_dims_size[i];
        }

        if (current_result_chunk_start_offset[i] + skiped_chunk_size[i] - 1 < skiped_dims_size[i])
        {
          current_result_chunk_end_offset[i] = current_result_chunk_start_offset[i] + skiped_chunk_size[i] - 1;
        }
        else
        {
          current_result_chunk_end_offset[i] = skiped_dims_size[i] - 1;
        }
        assert((current_result_chunk_end_offset[i] - current_result_chunk_start_offset[i] + 1 >= 0));
        current_result_chunk_cells = current_result_chunk_cells * (current_result_chunk_end_offset[i] - current_result_chunk_start_offset[i] + 1);
      }

      //Deal with overlapping
      //Starting coordinate for the data chunk with overlapping
      if (current_chunk_start_offset[i] <= data_overlap_size[i])
      {
        current_chunk_ol_start_offset[i] = 0;
      }
      else
      {
        current_chunk_ol_start_offset[i] = current_chunk_start_offset[i] - data_overlap_size[i];
      }
      //Original coordinate offset for each, used to get gloabl coordinate in Apply
      ol_origin_offset[i] = current_chunk_start_offset[i] - current_chunk_ol_start_offset[i];

      //Ending oordinate for the data chunk with overlapping
      if (current_chunk_end_offset[i] + data_overlap_size[i] < data_size[i])
      {
        current_chunk_ol_end_offset[i] = current_chunk_end_offset[i] + data_overlap_size[i];
      }
      else
      {
        current_chunk_ol_end_offset[i] = data_size[i] - 1;
      }
      assert((current_chunk_ol_end_offset[i] - current_chunk_ol_start_offset[i] + 1 >= 0));
      current_chunk_ol_size[i] = current_chunk_ol_end_offset[i] - current_chunk_ol_start_offset[i] + 1;
      current_chunk_ol_cells = current_chunk_ol_cells * current_chunk_ol_size[i];
    }

    //Next chunk id
    if (!reverse_apply_direction_flag)
    {
      current_chunk_id = current_chunk_id + au_mpi_size_global;
    }
    else
    {
      current_chunk_id = current_chunk_id - au_mpi_size_global;
    }
    current_chunk_data.resize(current_chunk_ol_cells);
    if (save_result_flag == 1)
    {
      if (!skip_flag)
      {
        result_vector_size = current_chunk_cells;
      }
      else
      {
        result_vector_size = current_result_chunk_cells;
      }
    }

    if (view_flag == 1)
    {
      for (int i = 0; i < data_dims; i++)
      {
        current_chunk_ol_start_offset[i] = current_chunk_ol_start_offset[i] + view_start_offset[i];
        current_chunk_ol_end_offset[i] = current_chunk_ol_end_offset[i] + view_start_offset[i];
      }
    }

    //Return  1, data read into   local_chunk_data
    //Return  0, end of file (no data left to handle)
    //Return -1: error happen
    //Read data between local_chunk_start_offset and local_chunk_end_offset
    //current_chunk_data.resize(current_chunk_cells);
    //return data_on_disk->ReadData(current_chunk_start_offset, current_chunk_end_offset, current_chunk_data);
    if (!virtual_array_flag)
    {
      endpoint->Read(current_chunk_ol_start_offset, current_chunk_ol_end_offset, &current_chunk_data[0]);
      return 1;
    }
    else
    {
      /*
      if (current_chunk_ol_start_offset_cache == current_chunk_ol_start_offset && current_chunk_ol_end_offset_cache == current_chunk_ol_end_offset)
      {

        current_chunk_data = current_chunk_data_cache;
        if (mpi_rank == 0)
          printf("Read cached data (test)!!!\n");
      }
      else
      {
        int n = attributes.size();
        Data<AttributeType> *ah;
        unsigned long long hym_count = 1;
        std::vector<AttributeType> current_chunk_data_temp;
        current_chunk_data_temp.resize(current_chunk_ol_cells);
        for (int i = 0; i < n; i++)
        {
          ah = attributes[i]->GetDataHandle();
          if (mpi_rank == 0)
            std::cout << "Read " << i << "th attribute: " << attributes[i]->GetDatasetName() << " \n";

          //ah->ReadDataStripingMem(current_chunk_ol_start_offset, current_chunk_ol_end_offset, &current_chunk_data[0], i, n, hym_count);
          ah->ReadData(current_chunk_ol_start_offset, current_chunk_ol_end_offset, current_chunk_data_temp);
          // printf("Load attribute %s,  i=%d (n=%d): value =  %f, %f\n", ah->GetDatasetName().c_str(), i, n, current_chunk_data_temp[0], current_chunk_data_temp[1]);
#if __cplusplus > 201402L
          InsertIntoVirtualVector<AttributeType, T>(current_chunk_data_temp, current_chunk_data, i);
#endif
          //std::cout << current_chunk_data[0] << std::endl;
          //std::cout << current_chunk_data[1] << std::endl;
        }
        current_chunk_data_temp.resize(0);
        current_chunk_data_cache = current_chunk_data;
        current_chunk_ol_start_offset_cache = current_chunk_ol_start_offset;
        current_chunk_ol_end_offset_cache = current_chunk_ol_end_offset;
      }
     */
      return 1;
    }
  }

}; //end of class Array

} // namespace AU
#endif
