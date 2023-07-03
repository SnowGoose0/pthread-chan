#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

#include "myChannels.h"
#include "lock.h"
#include "parse.h"

//static float* output_entries;
static int output_size;
static float* output_entries;

Lock c_lock;
Lock* c_lock_entries; /* Only used if lock_config == 2 */
int c_lock_entries_size;

void* compute_channels(void* t_args) {
  FILE* f;
  
  ThreadArgs* args = (ThreadArgs*) t_args;
  FileData* fd = args->fd;
  ComputeOptions* op = args->op;
  
  int file_index_offset = args->offset;
  int file_count_local = fd->channel_file_size / op->num_threads;
  int read_sz = op->buffer_size;
  
  char* read_buffer = (char*) calloc(read_sz + 1, sizeof(char));

  for (int fi = 0; fi < file_count_local; ++fi) { /* Loop through files assigned */
    int output_index = 0;
    int f_index = file_index_offset + (fi * op->num_threads);
    
    char* read_path = fd->channel_files[f_index].path;
    
    float prev = -1;
    size_t bytes_read;

    f = fopen(read_path, "rb");
    if (f == NULL) {
      fprintf(stderr, ERROR_FILE_PATH);
      exit(EXIT_FAILURE);
    }

    while ((bytes_read = fread(read_buffer, sizeof(char), read_sz, f)) > 0) {
      /*for (int i = 0; i < read_sz; ++i) {
	if (read_buffer[i] == '\n') printf("\\n");
	else printf("%c", read_buffer[i]);
      }
      printf("\n"); */
    
      int parsed, ps_err;
      float res, alpha_res;
    
      str_clean(read_buffer);
      ps_err = parse_int(&parsed, read_buffer);

      if (ps_err) continue;
      
      alpha_res = compute_alpha(fd->channel_files[f_index].alpha, parsed, prev);
      prev = alpha_res;
      res = compute_beta(fd->channel_files[f_index].beta, alpha_res);

      /* Entry Section */
      if (op->lock_config == 1) {
	while(__sync_lock_test_and_set(&c_lock, 1));
      }

      else if (op->lock_config == 2) {
	while(__sync_lock_test_and_set(&c_lock_entries[output_index], 1));
      }

      else if (op->lock_config == 3) {
	while(__sync_val_compare_and_swap(&c_lock, 0, 1));
      }

      /* Critical Section */
      printf("Thread#%d - Iteration %d\n", file_index_offset + 1, output_index);

      if (output_index >= output_size) {
	++output_size;
	++c_lock_entries_size;

	output_entries = (float*) realloc(output_entries, output_size);
	output_entries[output_index] = 0;

	if (op->lock_config == 2) {
	  c_lock_entries = (Lock*) realloc(c_lock_entries, c_lock_entries_size);
	  c_lock_entries[output_index] = 0;
	}
      }
      
      float cur_entry_val = output_entries[output_index];
      float new_entry_val = cur_entry_val + res;
      output_entries[output_index] = new_entry_val;

      /* Exit Section */
      Lock* lock_ptr = &c_lock;

      if (op->lock_config == 2) lock_ptr = &c_lock_entries[output_index];
      
      __sync_lock_release(lock_ptr);

      /* Remainder Section */
      memset(read_buffer, 0, read_sz + 1);
      ++output_index;
    }

    fclose(f);
  }

  free(read_buffer);
  
  return NULL;
}

int main(int argc, char** argv) {
  //output_entries = (float*) calloc(DEFAULT_OUTPUT_SIZE, sizeof(int));
  output_size = 1;
  output_entries = (float*) calloc(output_size, sizeof(float));
  c_lock_entries_size = 1; 
  c_lock = 0;
  
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

  if (lc == 2) c_lock_entries = (Lock*) calloc(1, sizeof(Lock));
  
  ComputeOptions* op = (ComputeOptions*) malloc(sizeof(ComputeOptions));
  
  op->buffer_size = bs;
  op->num_threads = nt;
  op->lock_config = lc;
  op->global_checkpointing = gcp;
  op->metadata_file_path = mfp;
  op->output_file_path = ofp;

  fclose(fopen(ofp, "w"));
  
  char* output_content = (char*) calloc(1, sizeof(char));
  
  FileData* fd = parse_metadata(mfp);
  ThreadArgs* t_args = (ThreadArgs*) malloc(sizeof(ThreadArgs) * nt);
  pthread_t* t_ids = (pthread_t*) malloc(sizeof(pthread_t) * nt);


  if (nt > fd->channel_file_size) {
    fprintf(stderr, "Too many threads\n");
    exit(EXIT_FAILURE);
  }

  if (fd->channel_file_size % nt != 0) {
    fprintf(stderr, "Invalid number of threads");
    exit(EXIT_FAILURE);
  }
 
  /* Spawn Threads */
  for (int i = 0; i < nt; ++i) {
    t_args[i].fd = fd;
    t_args[i].op = op;
    t_args[i].offset = i;
    pthread_create(t_ids + i, NULL, compute_channels, t_args + i);
  }

  /* Join Threads */
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

  /* Format Output */
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
  fputs(output_content, fopen("out.txt", "w+")); /* Dump */
  printf("output_content:\n%s\n", output_content);

  /* Memory Management */
  free_metadata(fd);
  free(output_content);
  free(t_args);
  free(t_ids);

  if (op->lock_config == 2) free(c_lock_entries); 
  
  free(op);
  // free(output_entries);

  exit(EXIT_SUCCESS);
}


/* =======================================================================================
   =======================================================================================
   ==================================================================================== */

char* ftos(char* path) {
  FILE* f = fopen(path, "rb");
  fseek(f, 0, SEEK_END);
  long fsz = ftell(f);
  rewind(f);

  char* f_buffer = (char*) calloc(fsz + 1, sizeof(char));
  fread(f_buffer, sizeof(char), fsz, f);

  fclose(f);
  
  return f_buffer;
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
