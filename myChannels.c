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
static int OUTPUT_SIZE;
static float* OUTPUT_ENTRIES;

Lock C_LOCK;
Lock* C_LOCK_ENTRIES; /* Only used if lock_config == 2 */
int C_LOCK_ENTRIES_SIZE;

typedef struct{
  FILE* file_desc;
  int fi;
} OpenFiles;

void* compute_channels(void* t_args) {  
  ThreadArgs* args = (ThreadArgs*) t_args;
  FileData* fd = args->fd;
  ComputeOptions* op = args->op;
  
  int file_index_offset = args->offset;
  int file_count_local = fd->channel_file_size / op->num_threads;
  int read_sz = op->buffer_size;
  int file_finished_count = 0;

  OpenFiles* open_files_buffer = (OpenFiles*) calloc(file_count_local, sizeof(OpenFiles));
  char* read_buffer = (char*) calloc(read_sz + 1, sizeof(char));
  int* output_index_buffer = (int*) calloc(file_count_local, sizeof(int));
  float* prev_buffer = (float*) calloc(file_count_local, sizeof(float));

  //memset(prev_buffer, 0, file_count_local);

  for (int i = 0; i < file_count_local; i++) prev_buffer[i] = -1;

  /* Open all files */
  for (int j = 0; j < file_count_local; ++j) {
    int f_index = file_index_offset + (j * op->num_threads);
    char* read_path = fd->channel_files[f_index].path;
    
    open_files_buffer[j].file_desc = fopen(read_path, "rb");
    open_files_buffer[j].fi = f_index;
  }

  // for (int fi = 0; fi < file_count_local; ++fi) { /* Loop through files 
  /* int output_index = 0; */
    /* int f_index = file_index_offset + (fi * op->num_threads); */
    
    /* char* read_path = fd->channel_files[f_index].path; */
    
    /* float prev = -1; */
    /* size_t bytes_read; */

    /* f = fopen(read_path, "rb"); */
    /* if (f == NULL) { */
    /*   fprintf(stderr, ERROR_FILE_PATH); */
    /*   exit(EXIT_FAILURE); */
    /* } */

  printf("file_count_local: %d\n", file_count_local);

  for (int i = 0; file_finished_count < file_count_local; i = (i + 1) % file_count_local) {
    FILE* f = open_files_buffer[i].file_desc;
    
    if (f == NULL) continue; /* Any finished file will be set to NULL */

    size_t bytes_read = fread(read_buffer, sizeof(char), read_sz, f);
    int output_index = output_index_buffer[i];
    int f_index = open_files_buffer[i].fi;

  //         for (int i = 0; i < read_sz; ++i) {
	// if (read_buffer[i] == '\n') printf("\\n");
	// else printf("%c", read_buffer[i]);
  //     }
  //     printf("\n"); 

    if (bytes_read == 0) {
      ++file_finished_count;
      
      fclose(f);
      open_files_buffer[i].file_desc = NULL;
      
      continue;
    }
    
    int parsed, ps_err;
    float res, alpha_res;
    
    str_clean(read_buffer);
    ps_err = parse_int(&parsed, read_buffer);

    if (ps_err) continue;
      
    alpha_res = compute_alpha(fd->channel_files[f_index].alpha, parsed, prev_buffer[i]);
    prev_buffer[i] = alpha_res;
    res = compute_beta(fd->channel_files[f_index].beta, alpha_res);

    /* Entry Section */
    if (op->lock_config == 1) {
      while(__sync_lock_test_and_set(&C_LOCK, 1));
    }

    else if (op->lock_config == 2) {
      while(__sync_lock_test_and_set(&C_LOCK_ENTRIES[output_index], 1));
    }

    else if (op->lock_config == 3) {
      while(__sync_val_compare_and_swap(&C_LOCK, 0, 1));
    }

    /* Critical Section */
    printf("Thread#%d - Iteration %d - Res %f\n", file_index_offset + 1, output_index, res);

    if (output_index >= OUTPUT_SIZE) {
      ++OUTPUT_SIZE;
      ++C_LOCK_ENTRIES_SIZE;

      OUTPUT_ENTRIES = (float*) realloc(OUTPUT_ENTRIES, OUTPUT_SIZE);
      OUTPUT_ENTRIES[output_index] = 0;

      if (op->lock_config == 2) {
	C_LOCK_ENTRIES = (Lock*) realloc(C_LOCK_ENTRIES, C_LOCK_ENTRIES_SIZE);
	C_LOCK_ENTRIES[output_index] = 0;
      }
    }
      
    float cur_entry_val = OUTPUT_ENTRIES[output_index];
    float new_entry_val = cur_entry_val + res;
    OUTPUT_ENTRIES[output_index] = new_entry_val;

    /* Exit Section */
    Lock* lock_ptr = &C_LOCK;

    if (op->lock_config == 2) lock_ptr = &C_LOCK_ENTRIES[output_index];
      
    __sync_lock_release(lock_ptr);

    /* Remainder Section */
    memset(read_buffer, 0, read_sz + 1);
    ++output_index_buffer[i];
  }
  /*
    fclose(f);
    } */

  free(read_buffer);
  
  return NULL;
}

int main(int argc, char** argv) {
  //output_entries = (float*) calloc(DEFAULT_OUTPUT_SIZE, sizeof(int));
  OUTPUT_SIZE = 1;
  OUTPUT_ENTRIES = (float*) calloc(OUTPUT_SIZE, sizeof(float));
  C_LOCK_ENTRIES_SIZE = 1; 
  C_LOCK = 0;
  
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

  if (lc == 2) C_LOCK_ENTRIES = (Lock*) calloc(1, sizeof(Lock));
  
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
  
  for (int out_i = 0; out_i < OUTPUT_SIZE; ++out_i) {
    int out_entry = (int) ceil(OUTPUT_ENTRIES[out_i]);
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
  mem_free(output_content);
  mem_free(t_args);
  mem_free(t_ids);

  if (op->lock_config == 2) free(C_LOCK_ENTRIES); 
  
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
    mem_free(fd->channel_files[f].path);
  }
  
  mem_free(fd->channel_files);
  mem_free(fd);
}

void mem_free(void* ptr) {
  if (ptr != NULL) {
    free(ptr);
    ptr = NULL;
  }
}
