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

void* compute_channels(void* t_args) {
  FILE* f;
  
  ThreadArgs* args = (ThreadArgs*) t_args;
  FileData* fd = args->fd;
  ComputeOptions* op = args->op;
  
  int file_index_offset = args->offset;
  int file_count_local = fd->channel_file_size / op->num_threads;
  int read_sz = op->buffer_size;
  
  char* read_buffer = (char*) calloc(read_sz + 1, sizeof(char));

  for (int f_index = 0; f_index < file_count_local; ++f_index) {
    int output_index = 0;
    int f_index = file_index_offset + (f_index * file_count_local);
    char* read_path = fd->channel_files[f_index].path;
    float prev = -1;
    size_t bytes_read;

    f = fopen(read_path, "rb");
    if (f == NULL) {
      fprintf(stderr, ERROR_FILE_PATH);
      exit(EXIT_FAILURE);
    }
  
    while ((bytes_read = fread(read_buffer, sizeof(char), read_sz, f)) > 0) {
      for (int i = 0; i < read_sz; ++i) {
	if (read_buffer[i] == '\n') printf("\\n");
	else printf("%c", read_buffer[i]);
      }
      printf("\n");
    
      int parsed, ps_err;
      float res, alpha_res;
    
      str_clean(read_buffer);
      ps_err = parse_int(&parsed, read_buffer);

      if (ps_err) continue;

      if (output_index >= output_size) {
	++output_size;
	output_entries = (float*) realloc(output_entries, output_size);
	output_entries[output_size - 1] = 0;
      }

      alpha_res = compute_alpha(fd->channel_files[f_index].alpha, parsed, prev);
      res = compute_beta(fd->channel_files[f_index].beta, alpha_res);

      prev = alpha_res;
      output_entries[output_index++] += res;
      memset(read_buffer, 0, read_sz + 1);
    }

    fclose(f);
  }
  
  return NULL;
}


int main(int argc, char** argv) {

  output_entries = (float*) calloc(DEFAULT_OUTPUT_SIZE, sizeof(int));
  output_size = DEFAULT_OUTPUT_SIZE;
  
  if (argc != EXPECTED_ARGC) {
    fprintf(stderr, ERROR_ARGC);
    exit(EXIT_FAILURE);
  }

  int parse_status, bs, nt, lc, gcp;

  parse_status = 0;
  parse_status += parse_int(&bs, *(argv + 1));
  parse_status += parse_int(&nt, *(argv + 2));
  parse_status += parse_int(&lc, *(argv + 4));
  parse_status += parse_int(&gcp, *(argv + 5));

  if (parse_status > 0) {
    fprintf(stderr, ERROR_PARSE_INT_ARG);
    exit(EXIT_FAILURE);
  }
 
  char* mfp = *(argv + 3);
  char* ofp = *(argv + 6);
  
  ComputeOptions* op = (ComputeOptions*) malloc(sizeof(ComputeOptions));
  
  op->buffer_size = bs;
  op->num_threads = nt;
  op->lock_config = lc;
  op->global_checkpointing = gcp;
  op->metadata_file_path = mfp;
  op->output_file_path = ofp;
  
  char* output_content = (char*) calloc(1, sizeof(char));
  
  FileData* fd = parse_metadata(mfp);
  ThreadArgs* t_args = (ThreadArgs*) malloc(sizeof(ThreadArgs) * nt);
  pthread_t* t_ids = (pthread_t*) malloc(sizeof(pthread_t) * nt);

  /*
    Spawn Threads
  */

  for (int i = 0; i < nt; ++i) {
    t_args[i].fd = fd;
    t_args[i].op = op;
    t_args[i].offset = i;
    pthread_create(t_ids + i, NULL, compute_channels, t_args + i);
  }

  /*
    Join Threads
  */

  for (int i = 0; i < nt; ++i) {
    pthread_join(t_ids[i], NULL);
  }
  
  //compute_channels(fd, output_file_path, 10, buffer_size);

  printf("bsize: %d\nthreads: %d\nmeta: %s\nlock: %d\ncheck: %d\nout: %s\n\n",
	 bs, nt, mfp, lc, gcp, ofp);
  
  for (int i = 0; i < fd->channel_file_size; ++i) {
    printf("File: %s\n", fd->channel_files[i].path);
    printf("Alpha: %f\n", fd->channel_files[i].alpha);
    printf("Beta: %f\n\n", fd->channel_files[i].beta);
  } 

  printf("\nOUT \n\n");
  for (int i = 0; i < output_size; ++i) {
    printf("%.2f\n", output_entries[i]);
  }

  /*
    Format Output
  */

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
  free(op);

  exit(EXIT_SUCCESS);
}


/* =======================================================================================
   =======================================================================================
   ==================================================================================== */

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
