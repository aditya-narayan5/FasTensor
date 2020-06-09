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
 * Email questions to dbin@lbl.gov
 * Scientific Data Management Research Group
 * Lawrence Berkeley National Laboratory
 *
 */

#ifndef END_POINT_H
#define END_POINT_H

#include "au_utility.h"
#include "au_type.h"
#include <string>
#include <iostream>
#include <vector>
#include <math.h>

using namespace std;

/**
 * @brief Define the class for the Endpoint used by ArrayUDF 
 * to store the data. It contains basic infomation for the endpoint
 * as well as the operations supported by the endpoint
 * 
 */
class Endpoint
{
protected:
    AuEndpointType endpoint_type;
    std::string endpoint_info;
    std::vector<unsigned long long> endpoint_dim_size;
    int endpoint_ranks;
    AuEndpointDataType data_element_type;
    std::string data_endpoint_orig;

    bool set_endpoint_dim_size_flag = false;
    bool open_flag = false;
    bool create_flag = false;
    unsigned read_write_flag;

public:
    Endpoint(){};
    virtual ~Endpoint(){};

    /**
     * @brief Get the Dimensions of the data
     * 
     * @return vector for the size of data  endpoint_dim_size.size() is the rank
     */
    std::vector<unsigned long long> GetDimensions();

    /**
     * @brief Set the Dimensions 
     * 
     * @return < 0 error, works otherwise
     */
    void SetDimensions(std::vector<unsigned long long> endpoint_dim_size_p);

    /**
     * @brief set the type of data element
     * 
     * @param data_element_type_p 
     */
    void SetDataElementType(AuEndpointDataType data_element_type_p);

    /**
     * @brief Get the Type of Data Element 
     * 
     * @return AuEndpointDataType 
     */
    AuEndpointDataType GetDataElementType();

    /**
     * @brief Get the size of the type for the element
     * 
     * @return int 
     */
    int GetDataElementTypeSize();

    /**
     * @brief Set the Endpoint Type object
     * 
     * @param endpoint_type_p 
     */
    void SetEndpointType(AuEndpointType endpoint_type_p);

    /**
     * @brief Get the Endpoint Type object
     * 
     * @return AuEndpointType 
     */
    AuEndpointType GetEndpointType();

    bool GetOpenFlag();

    void SetOpenFlag(bool open_flag_p);

    bool GetCreateFlag();

    void SetCreateFlag(bool open_flag_p);

    void SetRwFlag(unsigned read_write_flag_p);

    unsigned GetRwFlag();

    /**
     * @brief convert my data in (void *) type to Union type
     * 
     * @param vp : pointer to data (after read)
     * @return std::vector<AuEndpointDataTypeUnion> : return value
     */
    std::vector<AuEndpointDataTypeUnion> Void2Union(void *vp, size_t n_elements);

    /**
     * @brief convert data from union to void type 
     * 
     * @param data_vector_in_union_type : vector of data in union type
     * @return void* : pointer to data (for write)
     */
    void *Union2Void(std::vector<AuEndpointDataTypeUnion> &data_vector_in_union_type);

    /**
     * @brief set the endpoint_info string 
     * 
     * @param endpoint_info 
     */
    void SetEndpointInfo(std::string endpoint_info_p);

    /**
     * @brief Get the endpoint_info string 
     * 
     * @return std::string 
     */
    std::string GetEndpointInfo();

    /**
     * @brief parse endpoint_info to my own info
     *        
     * @return int: 0 works,  < 0 error,
     */
    virtual int ParseEndpointInfo() = 0;

    /**
     * @brief extracts metadata, possbile endpoint_ranks/endpoint_dim_size/other ep_type dependents ones
     * 
     * @return int < 0 error, >= 0 works 
     */
    virtual int ExtractMeta() = 0;
    /**
     * @brief print information about the endpoint
     * 
     * @return < 0 error, >= 0 works 
     */
    virtual int PrintInfo() = 0;

    /**
     * @brief create the endpoint
     * 
     * @return  < 0 error, >= 0 works 
     */
    virtual int Create() = 0;

    /**
     * @brief open the endpoint
     * 
     * @return < 0 error, >= 0 works 
     */
    virtual int Open() = 0;

    /**
     * @brief read the data from end-point
     * 
     * @param start, coordinates of the cell to start (including)
     * @param end , coordinates of the cell to end (including)
     * @param data, store the result data 
     * @return int < 0 error, >= 0 works
     */
    virtual int Read(std::vector<unsigned long long> start, std::vector<unsigned long long> end, void *data) = 0;

    /**
     * @brief write the data to the end-point
     * 
     * @param start, coordinates of the cell to start (including)
     * @param end , coordinates of the cell to end (including)
     * @param data, store the result data 
     * @return int < 0 error, >= 0 works
     */
    virtual int Write(std::vector<unsigned long long> start, std::vector<unsigned long long> end, void *data) = 0;

    /**
     * @brief close the end-point
     * 
     * @return int int < 0 error, >= 0 works
     */
    virtual int Close() = 0;

    virtual void Map2MyType() = 0;

    virtual void EnableCollectiveIO();

    virtual void DisableCollectiveIO();

    /**
    * @brief Get the Dir File Vector object
    * 
    * @return std::vector<std::string> 
    */
    virtual std::vector<std::string> GetDirFileVector();

    /**
     * @brief Set the Dir File Vector object
     * 
     * @param file_list 
     */
    virtual void SetDirFileVector(std::vector<std::string> &file_list);

    /**
     * @brief Get the Dir Chunk Size object
     * 
     * @return std::vector<int> 
     */
    virtual std::vector<int> GetDirChunkSize();

    /**
     * @brief Set the Dir Chunk Size object
     * 
     * @param dir_chunk_size_p 
     */
    virtual void SetDirChunkSize(std::vector<int> &dir_chunk_size_p);

    /**
     * @brief call a special operator on endpoint
     *        such as, enable collective I/O for HDF5
     *                 dump file from MEMORY to HDF5
     * @param opt_code, specially defined code 
     */
    virtual int SpecialOperator(int opt_code, std::string parameter);
};

#endif
