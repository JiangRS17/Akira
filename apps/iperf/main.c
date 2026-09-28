#include <flexos/isolation.h>
#if CONFIG_LIBFLEXOS_VMEPT
#include <flexos/impl/main_annotation.h>
#endif

#define RECVBUFFERSIZE 128

int run_server(int rcv_buffer_size);

int main(int argc, char *argv[])
{
	flexos_gate(libiperf, run_server, RECVBUFFERSIZE);
	return 0;
}
