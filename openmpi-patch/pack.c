#include "pack.h"

LINKAGE_TYPE int opt_pack(MPIOPT_Request* request) {
    void* current_pos = request->packed_buf;
    for(int k = 0; k < request->count; ++k) {
        for(int i = 0; i < request->num_cont_blocks; ++i) {
            memcpy(current_pos,
                request->buf + request->dtype_displacements[i] + k * request->dtype_extent,
                request->dtype_lengths[i]);
            
            current_pos += request->dtype_lengths[i];
          }
    }

    return 0;
}

LINKAGE_TYPE int opt_unpack(MPIOPT_Request* request) {
    void* current_pos = request->packed_buf;
    for(int k = 0; k < request->count; ++k) {
        for(int i = 0; i < request->num_cont_blocks; ++i) {
            memcpy(request->buf + request->dtype_displacements[i] + k * request->dtype_extent,
                current_pos,
                request->dtype_lengths[i]);
            
            current_pos += request->dtype_lengths[i];
          }
    }
    
    return 0;
}