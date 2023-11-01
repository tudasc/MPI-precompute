
void init_communication(int rank, int size);

void begin_halo_receive(void);

void begin_halo_send(void);

void end_halo_exchange(void);

void free_communication(void);
