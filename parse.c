#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"

int parse_int(int* dest, char* src) {
  char* res;
  *dest = (int) strtol(src, &res, 10);

  if (res == src || *res != '\0') {
    return 1;
  }
  
  return 0;
}

int parse_int_digit(int val) {
  int d_count = 1;
  if (val == 0) return d_count;
  
  while ((val /= 10) != 0) ++d_count;

  return d_count;
}

void parse_assign_default(FileData* fd, int* parse_state, int* index) {
  fd->channel_files[*index].beta = 1;
      
  if (*parse_state == STATE_ALPHA) {
    fd->channel_files[*index].alpha = 1;
  }
        
  (*index)++;
  *parse_state = STATE_FPATH;
}

FileData* parse_metadata(char* metadata_file_path) {
  FileData* fd = (FileData*) malloc(sizeof(FileData));
  FILE* f = fopen(metadata_file_path, "r");

  if (f == NULL) {
    free(fd);
    fprintf(stderr, "Error: invalid metadata file path\n");
    return NULL;
  }

  fseek(f, 0, SEEK_END);
  long fs = ftell(f);
  rewind(f);

  int parse_state = 0;
  int cur_file = 0;
  char* tok;
  char* meta_buffer = (char*) calloc(fs + 1, sizeof(char));
  char* meta_tmp = meta_buffer;
  fread(meta_buffer, sizeof(char), fs, f);
  fclose(f);

  tok = strtok(meta_buffer, TOK_DELIMITER);
  fd->channel_file_size = atoi(tok);
  fd->channel_files = (File*) malloc(sizeof(File) * fd->channel_file_size);
  tok = strtok(NULL, TOK_DELIMITER);

  if (tok == NULL) {
    free(fd->channel_files);
    free(fd);
    free(meta_buffer);
    fprintf(stderr, "Error: invalid contents in metadata file\n");

    return NULL;
  }
  
  while (tok != NULL && cur_file < fd->channel_file_size) {

    /* if one of or both alpha/beta values are not present */
    if (strstr(tok, ".txt") != NULL && parse_state != STATE_FPATH) {
      parse_assign_default(fd, &parse_state, &cur_file);
    }
    
    if (parse_state == STATE_FPATH) {
      char* p = (char*) calloc(strlen(tok) + 1, sizeof(char));
      strcpy(p, tok);
      fd->channel_files[cur_file].path = p;
      
    } else {
      float val = atof(tok);
      
      if (parse_state == STATE_ALPHA) {
	fd->channel_files[cur_file].alpha = val;
      }
      
      if (parse_state == STATE_BETA) {
	fd->channel_files[cur_file].beta = val;
	++cur_file;
      }
    }
    
    parse_state = (parse_state + 1) % STATE_COUNT;
    tok = strtok(NULL, TOK_DELIMITER);
  }

  if (parse_state != STATE_FPATH) {
    parse_assign_default(fd, &parse_state, &cur_file);
  }

  free(meta_tmp);
  return fd;
}
