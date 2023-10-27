
void init_communication(int rank, int size);

void begin_halo_receive();

void begin_halo_send();

void end_halo_exchange();

void free_communication();
