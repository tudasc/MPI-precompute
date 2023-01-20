#ifndef MPIOPT_REQUEST_TYPE_H
#define MPIOPT_REQUEST_TYPE_H


#define RECV_REQUEST_TYPE 1
#define SEND_REQUEST_TYPE 2
#define SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION 3
#define RECV_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION 4
#define SEND_REQUEST_TYPE_USE_FALLBACK 5
#define RECV_REQUEST_TYPE_USE_FALLBACK 6

struct mpiopt_request {
    // this way it it can be used as a normal request ptr as well
    struct ompi_request_t original_request;
    int flag;
    int flag_buffer;
    uint64_t remote_data_addr;
    uint64_t remote_flag_addr;
    ucp_rkey_h remote_data_rkey;
    ucp_rkey_h remote_flag_rkey;
    void *buf;
    size_t size;
    // initialized locally
    void *ucx_request_data_transfer;
    void *ucx_request_flag_transfer;
    int operation_number;
    int type;
    ucp_mem_h mem_handle_data;
    ucp_mem_h mem_handle_flag;
    ucp_ep_h
            ep; // save used endpoint, so we dont have to look it up over and over
    // necessary for backup in case no other persistent op matches
    MPI_Request backup_request;
    int tag;
    int dest;
    MPI_Comm comm;
    // MPI_Request rdma_exchange_request;
    MPI_Request rdma_exchange_request_send;
    void *rdma_info_buf;
    // struct mpiopt_request* rdma_exchange_buffer;
#ifdef BUFFER_CONTENT_CHECKING
    void *checking_buf;
  MPI_Request chekcking_request;
#endif
};
typedef struct mpiopt_request MPIOPT_Request;

#endif //MPIOPT_REQUEST_TYPE_H
