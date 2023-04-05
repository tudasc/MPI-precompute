#include "pack.h"

LINKAGE_TYPE int opt_pack(MPIOPT_Request* request) {
    void* current_pos = request->packed_buf;
    for(int i = 0; i < request->num_cont_blocks; ++i) {
        memcpy(current_pos,
            request->buf + request->dtype_displacements[i],
            request->dtype_lengths[i]);
        
        current_pos += request->dtype_lengths[i];
    }
    
    return 0;
}

LINKAGE_TYPE int opt_unpack(MPIOPT_Request* request) {
    void* current_pos = request->packed_buf;
    for(int i = 0; i < request->num_cont_blocks; ++i) {
        memcpy(request->buf + request->dtype_displacements[i],
            current_pos,
            request->dtype_lengths[i]);
        
        current_pos += request->dtype_lengths[i];
    }
    
    return 0;
}

LINKAGE_TYPE int opt_pack_threshold(MPIOPT_Request* request) {
    void* current_pos = request->packed_buf;
    for(int i = 0; i < request->num_cont_blocks; ++i) {
        if(request->dtype_lengths[i] <= request->threshold){
            memcpy(current_pos,
                request->buf + request->dtype_displacements[i],
                request->dtype_lengths[i]);
            
            current_pos += request->dtype_lengths[i];
        }
    }

    return 0;
}

LINKAGE_TYPE int opt_unpack_threshold(MPIOPT_Request* request) {
    void* current_pos = request->packed_buf;
    for(int i = 0; i < request->num_cont_blocks; ++i) {
        if(request->dtype_lengths[i] <= request->threshold){
            memcpy(request->buf + request->dtype_displacements[i],
                current_pos,
                request->dtype_lengths[i]);
            
            current_pos += request->dtype_lengths[i];
        }
    }
    
    return 0;
}