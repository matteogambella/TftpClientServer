#define BIN 0
#define TEXT 1

int get_file_size(char* filepath);
int get_file_chunk(char* buff, char* filepath, int offset, int size, int mode);
int append_file_chunk(char* data, char* filepath, int size, int mode);