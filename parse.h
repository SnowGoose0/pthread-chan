#ifndef PARSE
#define PARSE

#include "myChannels.h"

int parse_int(int* dest, char* src);
int parse_int_digit(int val);
void parse_assign_default(FileData* fd, int* parse_state, int* index);
FileData* parse_metadata(char* metadata_file_path);

#endif
