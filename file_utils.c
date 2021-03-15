#include "file_utils.h"

#include <stdio.h>
#include <sys/stat.h>

int get_file_size(char* filepath) {
	struct stat info;
	if (stat(filepath, &info) == -1)
		return -1;
	return info.st_size;
}

int get_file_chunk(char* buff, char* filepath, int offset, int size, int mode) {
	FILE* stream = NULL;
	if (filepath == NULL || buff == NULL) return -1;
	if (mode == BIN)	
		stream = fopen(filepath, "rb");
	else if (mode == TEXT)
		stream = fopen(filepath, "r");
	if (stream == NULL) return -1;
	fseek(stream, offset, SEEK_SET);
	fread(buff, size, 1, stream);
	fclose(stream);
	return 0;
}

int append_file_chunk(char* data, char* filepath, int size, int mode) {
	FILE* stream = NULL;
	if (data == NULL || filepath == NULL) return -1;
	if (mode == BIN)
		stream = fopen(filepath, "ab");
	else if (mode == TEXT)
		stream = fopen(filepath, "a");
	if (stream == NULL) return -1;
	fwrite(data, size, 1, stream);
	fclose(stream);
	return 0;
}