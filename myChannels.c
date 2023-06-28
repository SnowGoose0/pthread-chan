#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

#include "myChannels.h"
#include "parse.h"

static float* output_entries;
static int output_size;

int main(int argc, char** argv) {

  output_entries = (float*) calloc(DEFAULT_OUTPUT_SIZE, sizeof(int));
  output_size = DEFAULT_OUTPUT_SIZE;
  
  if (argc != EXPECTED_ARGC) {
    fprintf(stderr, ERROR_ARGC);
    exit(EXIT_FAILURE);
  }

  int buffer_size;
  int num_threads;
  int lock_config;
  int global_checkpointing;

  int parse_status = 0;

  parse_status += parse_int(&buffer_size, *(argv + 1));
  parse_status += parse_int(&num_threads, *(argv + 2));
  parse_status += parse_int(&lock_config, *(argv + 4));
  parse_status += parse_int(&global_checkpointing, *(argv + 5));

  if (parse_status > 0) {
    fprintf(stderr, ERROR_PARSE_INT_ARG);
    exit(EXIT_FAILURE);
  }
 
  char* metadata_file_path = *(argv + 3);
  char* output_file_path = *(argv + 6);
  char* output_content = (char*) calloc(1, sizeof(char));
  
  FileData* fd = parse_metadata(metadata_file_path);

  compute_channels(fd, output_file_path, 10, buffer_size);

  printf("bsize: %d\nthreads: %d\nmeta: %s\nlock: %d\ncheck: %d\nout: %s\n\n",
	 buffer_size, num_threads, metadata_file_path, lock_config, global_checkpointing, output_file_path);
  
  for (int i = 0; i < fd->channel_file_size; ++i) {
    printf("File: %s\n", fd->channel_files[i].path);
    printf("Alpha: %f\n", fd->channel_files[i].alpha);
    printf("Beta: %f\n\n", fd->channel_files[i].beta);
  } 

  printf("\nOUT \n\n");
  for (int i = 0; i < output_size; ++i) {
    printf("%.2f\n", output_entries[i]);
  }

  int output_offset = 0;
  
  for (int out_i = 0; out_i < output_size; ++out_i) {
    int out_entry = (int) ceil(output_entries[out_i]);
    int out_entry_len = parse_int_digit(out_entry);

    output_content = (char*) realloc((char*) output_content,
				     sizeof(char) * (output_offset + out_entry_len + 2));
    
    sprintf(output_content + output_offset, "%d\n", out_entry);
    output_offset += out_entry_len + 1;
  }

  output_content[output_offset] = 0;
  printf("output_content:\n%s\n", output_content);

  free_metadata(fd);
  free(output_entries);

  exit(EXIT_SUCCESS);
}


/* =======================================================================================
   =======================================================================================
   ================
   ==================================================================== */

void compute_channels(FileData* fd, const char* output_file_path, int file_index_offset, int read_sz) {
  FILE* f;
  char* read_buffer = (char*) calloc(read_sz + 1, sizeof(char));

  for (int f_index = 0; f_index < fd->channel_file_size; ++f_index) {
    
    char* read_path = fd->channel_files[f_index].path;
    size_t bytes_read;

    f = fopen(read_path, "rb");
    if (f == NULL) {
      fprintf(stderr, ERROR_FILE_PATH);
      exit(EXIT_FAILURE);
    }

    int output_index = 0;
    float prev = -1;
  
    while ((bytes_read = fread(read_buffer, sizeof(char), read_sz, f)) > 0) {
    
      for (int i = 0; i < read_sz; ++i) {
	if (read_buffer[i] == '\n') printf("\\n");
	else printf("%c", read_buffer[i]);
      }
      printf("\n");
    
      int parsed;
      float res;
    
      str_clean(read_buffer);
      int ps_err = parse_int(&parsed, read_buffer);

      if (ps_err) continue;

      if (output_index >= output_size) {
	++output_size;
	output_entries = (float*) realloc(output_entries, output_size);
	output_entries[output_size - 1] = 0;
      }

      float alpha_res = compute_alpha(fd->channel_files[f_index].alpha, parsed, prev);
      res = compute_beta(fd->channel_files[f_index].beta, alpha_res);

      prev = alpha_res;
      output_entries[output_index++] += res;
      memset(read_buffer, 0, read_sz + 1);
    }

    fclose(f);

  }
}

float compute_beta(float beta, float sample) {
  return sample * beta;
}

float compute_alpha(float alpha, float sample, float prev_sample) {
  if (prev_sample == -1) {
    return sample;
  }
  
  float inv_alpha = 1 - alpha;
  return compute_beta(sample, alpha) + compute_beta(prev_sample, inv_alpha);
}

int str_clean(char* src) {
  char* tmp = src;
  int cur = 0;

  while (*tmp != 0) {
    if (strchr(WHITESPACE_CHARS, *tmp) != NULL) {
      *tmp = 0;
    } else {
      int tmp_val = *tmp;
      *tmp = 0;
      *(src + cur) = tmp_val;
      cur++;
    }    
    ++tmp;
  }

  return cur;
}

void free_metadata(FileData* fd) {
  int f = 0;
  for (; f < fd->channel_file_size; ++f) {
    free(fd->channel_files[f].path);
    fd->channel_files[f].path = NULL;
  }
  
  free(fd->channel_files);
  free(fd);
  fd = NULL;
}
