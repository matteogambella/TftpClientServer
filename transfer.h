struct transfer {
	int id;
	struct sockaddr* addr;
	char* filepath;
	int filemode;
	pthread_cond_t acked;
	pthread_mutex_t mutex;
	struct transfer* next;
};

typedef struct transfer* transfer_list_t;

void init_transfer_list(transfer_list_t* list);
struct transfer* create_transfer(int id, struct sockaddr* addr, char* filepath, int mode);
int add(transfer_list_t* list, struct transfer* new_transfer);
void remove_transfer(transfer_list_t* list, struct transfer* target);
void deallocate(struct transfer* transfer);
void print_transfer_list(transfer_list_t list);
struct transfer* get_transfer_byaddr(transfer_list_t list, struct sockaddr_in* addr);