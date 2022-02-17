#define _GNU_SOURCE         /* needed for some ompi internal headers*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <malloc.h>
#include <sys/time.h>

#include "low_level.h"

#include <execinfo.h>

/* ************************************************************************ */
/*  main                                                                    */
/* ************************************************************************ */

#define READY_TO_RECEIVE 1
#define READY_TO_SEND 2

#define SEND 3
#define RECEIVED 4

struct matching_info {
	int flag;
	int crosstalk_flag;
	uint64_t remote_data_addr;
	ucp_rkey_h remote_data_rkey;
	uint64_t remote_flag_addr;
	ucp_rkey_h remote_flag_rkey;
};

void b_send(struct matching_info *info, void *buf, size_t size, ucp_ep_h ep) {

	if (info->flag == READY_TO_RECEIVE) {
		info->crosstalk_flag = SEND;
		// start rdma data transfer
#ifdef STATISTIC_PRINTING
		printf("send pushes data\n");
#endif
		RDMA_Put_test(buf, size, info->remote_data_rkey, ep,
				info->remote_data_addr);

		info->flag=0;// the send is done at our side

		RDMA_Put_test(&info->crosstalk_flag, sizeof(int), info->remote_flag_rkey, ep,
				info->remote_flag_addr);

	} else {
		info->flag = READY_TO_SEND;// doesnt matter, if this "corrupts" the flag, only he receiver has to detect crosstalk
		info->crosstalk_flag = READY_TO_SEND;
		// give him the flag that we are ready: he will RDMA get the data
		RDMA_Put_test(&info->crosstalk_flag, sizeof(int), info->remote_flag_rkey, ep,
				info->remote_flag_addr);
	}

}

void e_send(struct matching_info *info, void *buf, size_t size, ucp_ep_h ep) {
	if (info->crosstalk_flag == READY_TO_SEND) {
#ifdef STATISTIC_PRINTING
		if (info->flag == READY_TO_RECEIVE) {

			printf("Crosstalk on send\n");
			// the receiver will get the data
		}
#endif
		spin_wait_for(&info->flag, DATA_RECEIVED);
	}
	// else: nothing to do, we have Puted the content
	// TODO use proper non-blocking
		info->crosstalk_flag=0;

}

void b_recv(struct matching_info* info, void *buf, size_t size, ucp_ep_h ep) {

	if (info->flag == READY_TO_SEND) {
		//info.crosstalk_flag = 0;
		// start rdma data transfer
#ifdef STATISTIC_PRINTING
		printf("recv fetches data\n");
#endif
		RDMA_Get_test(buf, size, info->remote_data_rkey, ep,
				info->remote_data_addr);
		info->flag = 0;// recv is done at our side
		info->crosstalk_flag=DATA_RECEIVED;
		RDMA_Put_test(&info->crosstalk_flag, sizeof(int), info->remote_flag_rkey, ep,
				info->remote_flag_addr);

	} else {
		//info->flag = READY_TO_RECEIVE;
		info->crosstalk_flag = READY_TO_RECEIVE;
		// give him the flag that we are ready: he will RDMA get the data
		RDMA_Put_test(&info->crosstalk_flag, sizeof(int), info->remote_flag_rkey, ep,
				info->remote_flag_addr);
	}

}

void e_recv(struct matching_info *info, void *buf, size_t size, ucp_ep_h ep) {
	if (info->crosstalk_flag == READY_TO_RECEIVE) {
		if (info->flag == READY_TO_SEND) {
#ifdef STATISTIC_PRINTING
			printf("Crosstalk on recv\n");
#endif
			//we will fetch the data
			b_recv(info, buf, size, ep);
		} else {
			// wait for content to arrive
			//spin_wait_for(&info->flag, SEND);
			// wait until either the other rank has finished transfer, or we have initiated the transfer ourselves
			while (info->flag != SEND || info->crosstalk_flag==DATA_RECEIVED) {
					//TODO sleep?
					ucp_worker_progress(mca_osc_ucx_component.ucp_worker);
					// need to double check, that the other thread havent crosstalked
					if(info->flag==READY_TO_SEND){
#ifdef STATISTIC_PRINTING
						printf("second stage Crosstalk on recv\n");
#endif
						b_recv(info, buf, size, ep);
					}
				}
		}
	}
	// else: nothing to do, we have gotten the content via rdma get
	// TODO use proper non-blocking

	info->crosstalk_flag=0;

}

//#define STATISTIC_PRINTING

#define BUFFER_SIZE 10000
#define NUM_ITERS 1000

#define N BUFFER_SIZE



void check_buffer_content(int* buf,int n){
	int not_correct=0;

	for (int i = 0; i < N; ++i) {
		if(buf[i] != 1 * i * n){
			not_correct++;
		}
	}

	if (not_correct!=0){
		printf("ERROR: %d: buffer has unexpected content\n",n);
		//exit(-1);
	}

}



#define tag_entry 42
#define tag_rkey_data 43
#define tag_rkey_flag 44

void use_self_implemented_comm() {
	int send_list[2] = { 1, 1 };
	int recv_list[2] = { 1, 1 };

	struct matching_info info;
	struct matching_info info_to_send;

	int rank, numtasks;
	// Welchen rang habe ich?
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	// wie viele Tasks gibt es?
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);

	MPI_Win win;
	MPI_Win_create(&rank, sizeof(int), 1, MPI_INFO_NULL, MPI_COMM_WORLD, &win);

	int dest = (rank + 1) % numtasks;

	ompi_osc_ucx_module_t *module = (ompi_osc_ucx_module_t*) win->w_osc_module;
	ucp_ep_h ep = OSC_UCX_GET_EP(module->comm, dest);

	int *buffer = malloc(N * sizeof(int));

	info_to_send.crosstalk_flag = 0;
	info_to_send.flag = 0;
	info_to_send.remote_data_addr = buffer;
	info_to_send.remote_flag_addr = &info;
	MPI_Send(&info_to_send, sizeof(struct matching_info), MPI_BYTE, dest,
			tag_entry, MPI_COMM_WORLD);
	MPI_Recv(&info, sizeof(struct matching_info), MPI_BYTE, dest, tag_entry,
			MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	ucp_mem_h mem_handle_data;
	ucp_mem_h mem_handle_flag;

	ucp_context_h context = mca_osc_ucx_component.ucp_context;

	// prepare buffer for RDMA access:
	ucp_mem_map_params_t mem_params;
	//ucp_mem_attr_t mem_attrs;
	ucs_status_t ucp_status;
	// init mem params
	memset(&mem_params, 0, sizeof(ucp_mem_map_params_t));

	mem_params.address = buffer;
	mem_params.length = N * sizeof(int);
	// we need to tell ucx what fields are valid
	mem_params.field_mask = UCP_MEM_MAP_PARAM_FIELD_ADDRESS
			| UCP_MEM_MAP_PARAM_FIELD_LENGTH;

	ucp_status = ucp_mem_map(context, &mem_params, &mem_handle_data);
	assert(ucp_status == UCS_OK && "Error in register mem for RDMA operation");

	void *rkey_buffer;
	size_t rkey_size;

	// pack a remote memory key
	ucp_status = ucp_rkey_pack(context, mem_handle_data, &rkey_buffer,
			&rkey_size);
	assert(ucp_status == UCS_OK && "Error in register mem for RDMA operation");

	MPI_Send(rkey_buffer, rkey_size, MPI_BYTE, dest, tag_rkey_data,
			MPI_COMM_WORLD);

	// free temp buf
	ucp_rkey_buffer_release(rkey_buffer);

	memset(&mem_params, 0, sizeof(ucp_mem_map_params_t));

	mem_params.address = &info;
	mem_params.length = sizeof(int);
	// we need to tell ucx what fields are valid
	mem_params.field_mask = UCP_MEM_MAP_PARAM_FIELD_ADDRESS
			| UCP_MEM_MAP_PARAM_FIELD_LENGTH;

	ucp_status = ucp_mem_map(context, &mem_params, &mem_handle_flag);
	assert(ucp_status == UCS_OK && "Error in register mem for RDMA operation");

	// pack a remote memory key
	ucp_status = ucp_rkey_pack(context, mem_handle_flag, &rkey_buffer,
			&rkey_size);
	assert(ucp_status == UCS_OK && "Error in register mem for RDMA operation");

	MPI_Send(rkey_buffer, rkey_size, MPI_BYTE, dest, tag_rkey_flag,
			MPI_COMM_WORLD);

	// free temp buf
	ucp_rkey_buffer_release(rkey_buffer);

	void *temp_buf;
	MPI_Status status;
	int count;
	MPI_Probe(dest, tag_rkey_data, MPI_COMM_WORLD, &status);
	MPI_Get_count(&status, MPI_BYTE, &count);
	temp_buf = calloc(count,1);
	MPI_Recv(temp_buf, count, MPI_BYTE, dest, tag_rkey_data, MPI_COMM_WORLD,
			MPI_STATUS_IGNORE);
	ucp_ep_rkey_unpack(ep, temp_buf, &info.remote_data_rkey);
	free(temp_buf);

	MPI_Probe(dest, tag_rkey_flag, MPI_COMM_WORLD, &status);
	MPI_Get_count(&status, MPI_BYTE, &count);
	temp_buf = calloc(count,1);
	MPI_Recv(temp_buf, count, MPI_BYTE, dest, tag_rkey_flag, MPI_COMM_WORLD,
			MPI_STATUS_IGNORE);
	ucp_ep_rkey_unpack(ep, temp_buf, &info.remote_flag_rkey);
	free(temp_buf);


	//printf("Rank %d: buffer: %p remote:%p\n",rank,buffer,info.remote_data_addr);
	//printf("Rank %d: flagbuffer: %p flagremote:%p\n",rank,&info,info.remote_flag_addr);

	//TODO should not be necessary
	MPI_Barrier(MPI_COMM_WORLD);

	if (rank == 0) {

		for (int n = 0; n < NUM_ITERS; ++n) {

			for (int i = 0; i < N; ++i) {
				buffer[i] = rank * i * n;
			}

			b_recv(&info, buffer, sizeof(int) * N, ep);
			e_recv(&info, buffer, sizeof(int) * N, ep);
#ifdef STATISTIC_PRINTING
			check_buffer_content(buffer,n);
#endif
		}

	} else {

		for (int n = 0; n < NUM_ITERS; ++n) {
			for (int i = 0; i < N; ++i) {
				buffer[i] = rank * i * n;
			}
			b_send(&info, buffer, sizeof(int) * N, ep);
			e_send(&info, buffer, sizeof(int) * N, ep);
		}
	}

	// free ressources
	ucp_mem_unmap(context, mem_handle_flag);
	ucp_mem_unmap(context, mem_handle_data);

	free(buffer);
	MPI_Win_free(&win);
}

void use_standard_comm() {

	int rank, numtasks;
// Welchen rang habe ich?
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
// wie viele Tasks gibt es?
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	int *buffer = malloc(N * sizeof(int));

	if (rank == 1) {

		for (int n = 0; n < NUM_ITERS; ++n) {
			for (int i = 0; i < N; ++i) {
				buffer[i] = rank * i * n;
			}
			MPI_Send(buffer, sizeof(int) * N, MPI_BYTE, 0, 42,
			MPI_COMM_WORLD);

		}
	} else {
		for (int n = 0; n < NUM_ITERS; ++n) {

			for (int i = 0; i < N; ++i) {
				buffer[i] = rank * i * n;
			}

			MPI_Recv(buffer, sizeof(int) * N, MPI_BYTE, 1, 42,
			MPI_COMM_WORLD, MPI_STATUS_IGNORE);
#ifdef STATISTIC_PRINTING
			check_buffer_content(buffer,n);
#endif
		}

		// after comm
		/*
		 for (int i = 0; i < N; ++i) {
		 printf("%i,", buffer[i]);
		 }
		 printf("\n");
		 */
	}
}

int main(int argc, char **argv) {

	struct timeval start_time; /* time when program started                      */
	struct timeval stop_time; /* time when calculation completed                */

//Initialisiere Alle Prozesse
	MPI_Init(&argc, &argv);
	gettimeofday(&start_time, NULL); /*  start timer         */
	use_self_implemented_comm();
	gettimeofday(&stop_time, NULL); /*  stop timer          */
	double time = (stop_time.tv_sec - start_time.tv_sec)
			+ (stop_time.tv_usec - start_time.tv_usec) * 1e-6;

	printf("Self Implemented:    %f s \n", time);
	gettimeofday(&start_time, NULL); /*  start timer         */
	use_standard_comm();
	gettimeofday(&stop_time, NULL); /*  stop timer          */
	time = (stop_time.tv_sec - start_time.tv_sec)
			+ (stop_time.tv_usec - start_time.tv_usec) * 1e-6;

	printf("Standard:    %f s \n", time);

	MPI_Finalize();
	return 0;
}

