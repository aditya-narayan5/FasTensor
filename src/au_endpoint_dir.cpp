/**
 *
 * Email questions to {dbin}@lbl.gov
 * Scientific Data Management Research Group
 * Lawrence Berkeley National Laboratory
 *
 */

#include "au_endpoint_dir.h"
#include "au_array_view_access.h"

int EndpointDIR::ExtractMeta()
{
    dir_file_list = GetDirFileList(dir_str);

    if (dir_file_list.size() < 0)
        AU_EXIT("No file under directory");

    //std::vector<unsigned long long> endpoint_dim_size;
    //int endpoint_ranks;
    std::vector<unsigned long long> temp_endpoint_dim_size;
    for (int i = 0; i < dir_file_list.size(); i++)
    {
        sub_endpoint->SetEndpointInfo(dir_str + "/" + dir_file_list[i] + ":" + append_sub_endpoint_info);
        sub_endpoint->ExtractMeta();
        temp_endpoint_dim_size = sub_endpoint->GetDimensions();

        if (i == 0)
        {
            endpoint_dim_size = temp_endpoint_dim_size;
        }
        else
        {
            if (endpoint_dim_size != temp_endpoint_dim_size)
            {
                AU_EXIT("Data under directory must have same size");
            }
        }
    }

    //Set chunk size to be the size of a end_point
    dir_chunk_size.resize(endpoint_dim_size.size());
    dir_overlap_size.resize(endpoint_dim_size.size());
    endpoint_ranks = endpoint_dim_size.size();
    for (int i = 0; i < endpoint_ranks; i++)
    {
        dir_chunk_size[i] = endpoint_dim_size[i];
        dir_overlap_size[i] = 0;
        if (i == dir_data_merge_index)
        {
            endpoint_dim_size[i] = endpoint_dim_size[i] * dir_file_list.size();
        }
        else
        {
            endpoint_dim_size[i] = endpoint_dim_size[i];
        }
    }

    return 0;
}

int EndpointDIR::Create()
{
    //sub_endpoint->SetEndpointInfo(dir_str + "/" + dir_file_list[0] + ":" + append_sub_endpoint_info);
    //sub_endpoint->SetDataElementType(data_element_type);
    //return sub_endpoint->Create();
    return 0;
}

int EndpointDIR::Open()
{
    return 0;
}

/**
     * @brief read the data from end-point
     * 
     * @param start, coordinates of the cell to start (including)
     * @param end , coordinates of the cell to end (including)
     * @param data, store the result data 
     * @return int < 0 error, >= 0 works
     */
int EndpointDIR::Read(std::vector<unsigned long long> start, std::vector<unsigned long long> end, void *data)
{

    int data_rank = start.size();

    //get the size of data to read
    std::vector<unsigned long long> count(data_rank);
    COUNT_RANGES(start, end, count);

    int sub_endpoint_index = 0, sub_endpoint_index_end = 0;
    sub_endpoint_index = start[dir_data_merge_index] / dir_chunk_size[dir_data_merge_index];
    sub_endpoint_index_end = (end[dir_data_merge_index] + 1) / dir_chunk_size[dir_data_merge_index];

    std::vector<unsigned long long> start_sub_endpoint(start.begin(), start.end()), end_sub_endpoint(end.begin(), end.end());
    start_sub_endpoint[dir_data_merge_index] = 0;
    end_sub_endpoint[dir_data_merge_index] = dir_chunk_size[dir_data_merge_index] - 1;

    PrintScalar("sub_endpoint_index", sub_endpoint_index);
    PrintScalar("sub_endpoint_index_end", sub_endpoint_index_end);
    PrintVector("start_sub_endpoint", start_sub_endpoint);
    PrintVector("end_sub_endpoint", end_sub_endpoint);

    //std::cout << "DIR read sub (before) " << sub_endpoint->GetEndpointInfo() << ", append_sub_endpoint_info =" << append_sub_endpoint_info << ", sub_endpoint_index = " << sub_endpoint_index << "\n";

    size_t total_element;
    COUNT_CELLS(start_sub_endpoint, end_sub_endpoint, total_element);
    size_t sub_endpoint_element_size = sub_endpoint->GetDataElementTypeSize();

    std::cout << "sub_endpoint_element_size = " << sub_endpoint_element_size << ", total_element =" << total_element << "\n";
    void *data_temp = malloc(total_element * sub_endpoint_element_size);

    std::vector<unsigned long long> view_start(data_rank), view_end(data_rank);
    for (int i = sub_endpoint_index; i <= sub_endpoint_index_end; i++)
    {
        //sub_endpoint->SetDataElementType(data_element_type);
        sub_endpoint->SetEndpointInfo(dir_str + "/" + dir_file_list[i] + ":" + append_sub_endpoint_info);
        //sub_endpoint->Close();
        sub_endpoint->Open();
        sub_endpoint->Read(start_sub_endpoint, end_sub_endpoint, data_temp);
        sub_endpoint->Close();
        for (int j = 0; j < data_rank; j++)
        {
            view_start[j] = dir_chunk_size[j] * i;
            view_end[j] = view_start[j] + end_sub_endpoint[j] - start_sub_endpoint[j] + 1;
        }
        PrintVector("view_start: ", view_start);
        PrintVector("view_end: ", view_end);
        ArrayViewAccess(data_temp, data, count, view_start, view_end, ARRAY_VIEW_WRITE, sub_endpoint_element_size);
    }
    //Insert data_temp into data
    //inline int ArrayViewAccess(void *view_buffer, void *array_buffer, std::vector<unsigned long long> &array_size, std::vector<unsigned long long> &start, std::vector<unsigned long long> &end, int read_write_code, int element_size);
    //std::cout << "DIR read sub (after) " << sub_endpoint->GetEndpointInfo() << ", append_sub_endpoint_info =" << append_sub_endpoint_info << ", sub_endpoint_index = " << sub_endpoint_index << "\n";

    free(data_temp);
    return 0;
}

/**
     * @brief write the data to the end-point
     * 
     * @param start, coordinates of the cell to start (including)
     * @param end , coordinates of the cell to end (including)
     * @param data, store the result data 
     * @return int < 0 error, >= 0 works
     */
int EndpointDIR::Write(std::vector<unsigned long long> start, std::vector<unsigned long long> end, void *data)
{
    int sub_endpoint_index = 0;
    sub_endpoint_index = start[0] / dir_chunk_size[0];
    start[0] = 0;
    end[0] = dir_chunk_size[0] - 1;
    for (int i = 0; i < endpoint_ranks; i++)
        endpoint_dim_size[i] = dir_chunk_size[i];

    sub_endpoint->SetDataElementType(data_element_type);
    sub_endpoint->SetDimensions(endpoint_dim_size);
    sub_endpoint->SetEndpointInfo(dir_str + "/" + dir_file_list[sub_endpoint_index] + ":" + append_sub_endpoint_info);
    sub_endpoint->Create();
    sub_endpoint->Write(start, end, data);
    return sub_endpoint->Close();
}

/**
     * @brief close the end-point
     * 
     * @return int int < 0 error, >= 0 works
     */
int EndpointDIR::Close()
{
    return 0;
};

void EndpointDIR::EnableCollectiveIO()
{
}

void EndpointDIR::DisableCollectiveIO()
{
}

int EndpointDIR::PrintInfo()
{
    std::cout << data_endpoint_orig << std::endl;
    return 0;
}

void EndpointDIR::Map2MyType()
{
}

int EndpointDIR::ParseEndpointInfo()
{
    ExtractEndpointTypeInfo(endpoint_info, sub_endpoint_type, sub_endpoint_info);

    std::stringstream ss(sub_endpoint_info);
    if (!std::getline(ss, dir_str, ':'))
    {
        AU_EXIT("Wrong sub_endpoint_info");
    }

    if (sub_endpoint_type == EP_HDF5)
    {
        if (!std::getline(ss, append_sub_endpoint_info, ':'))
        {
            AU_EXIT("Wrong sub_endpoint_info");
        }
    }

    return 0;
}

/**
     * @brief Get the Chunk Size object
     * 
     * @return std::vector<int> 
     */
std::vector<int> EndpointDIR::GetDirChunkSize()
{
    return dir_chunk_size;
}

void EndpointDIR::SetDirChunkSize(std::vector<int> &dir_chunk_size_p)
{
    dir_chunk_size = dir_chunk_size_p;
}

std::vector<std::string> EndpointDIR::GetDirFileVector()
{
    return dir_file_list;
}

void EndpointDIR::SetDirFileVector(std::vector<std::string> &file_list)
{
    dir_file_list = file_list;
}

int EndpointDIR::SpecialOperator(int opt_code, std::string parameter)
{
    int sub_cmd;
    std::string sub_cmd_arg, temp_str;
    std::stringstream ss(parameter);

    switch (opt_code)
    {
    case DIR_MERGE_INDEX:
        SetMergeIndex(std::stoi(parameter));
        break;
    case DIR_SUB_CMD_ARG:
        if (!std::getline(ss, temp_str, ':'))
        {
            AU_EXIT("Invalued sub_endpoint_info");
        }
        if (sub_endpoint != nullptr)
            sub_cmd = sub_endpoint->MapOpStr2Int(temp_str);

        if (!std::getline(ss, sub_cmd_arg, ':'))
        {
            AU_EXIT("Invalued sub_endpoint_info");
        }
        if (sub_endpoint != nullptr)
            sub_endpoint->SpecialOperator(sub_cmd, sub_cmd_arg);
        break;
    default:
        break;
    }
    return 0;
}

/**
     * @brief Set the Merge Index
     * 
     * @param index_p 
     */
void EndpointDIR::SetMergeIndex(int index_p)
{
    dir_data_merge_index = index_p;
}

/**
     * @brief Get the Merge Index object
     * 
     * @return int 
     */
int EndpointDIR::GetMergeIndex()
{
    return dir_data_merge_index;
}