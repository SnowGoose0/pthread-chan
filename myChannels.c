#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdatomic.h>
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
Lock* C_LOCK_ENTRIES; // L_CONFIG == 2: array of locks per entry 
int C_LOCK_ENTRIES_SIZE; // L_CONFIG == 2: size of C_LOCK_ENTRIES

pthread_barrier_t G_CHECKPOINT; // G_CP == 1: checkpoint
atomic_int G_FLAG; // G_CP == 1: number of threads that are finished 
atomic_int A_FLAG; // G_CP == 1: number of threads waiting at the barrier

void* compute_channels(void* t_args) {  
  ThreadArgs* args = (ThreadArgs*) t_args;
  FileData* fd = args->fd;
  ComputeOptions* op = args->op;

  const int thread_count = op->num_threads;
  int file_index_offset = args->offset;
  int file_count_local = fd->channel_file_size / thread_count;
  int read_sz = op->buffer_size;
  int file_finished_count = 0;

  OpenFiles* open_files_buffer = (OpenFiles*) calloc(file_count_local, sizeof(OpenFiles));
  char* read_buffer = (char*) calloc(read_sz + 1, sizeof(char));
  int* output_index_buffer = (int*) calloc(file_count_local, sizeof(int));
  float* prev_buffer = (float*) calloc(file_count_local, sizeof(float));

  /* Set initial prev to -1 */
  for (int i = 0; i < file_count_local; i++) *(prev_buffer + i) = -1;

  /* Open all files */
  for (int j = 0; j < file_count_local; ++j) {
    int f_index = file_index_offset + (j * thread_count);
    char* read_path = fd->channel_files[f_index].path;
    
    open_files_buffer[j].file_desc = fopen(read_path, "rb");
    open_files_buffer[j].fi = f_index;
  }

  printf("file_count_local: %d\n", file_count_local);

  int _byte = -1;

  for (int i = 0; G_FLAG <= thread_count; i = (i + 1) % file_count_local) {
    FILE* f = open_files_buffer[i].file_desc;

    if (op->global_checkpointing == 1 && i == 0) {
      //printf("Thread #%d is WAITING\n", file_index_offset + 1);
      atomic_fetch_add(&A_FLAG, 1);
      pthread_barrier_wait(&G_CHECKPOINT);
    }

    if (G_FLAG == thread_count) { // all threads are done - exit now
      break;
    }

    if (op->global_checkpointing) __sync_val_compare_and_swap(&A_FLAG, thread_count, 0);
    
    if (f == NULL) continue; /* Any finished file will be set to NULL */

    size_t bytes_read = fread(read_buffer, sizeof(char), read_sz, f);
    int output_index = output_index_buffer[i];
    int f_index = open_files_buffer[i].fi;

  //         for (int i = 0; i < \==-098765432
    // ead_sz; ++i) {
	// if (read_buffer[i] == '\n') printf("\\n");
	// else printf("%c", read_buffer[i]);
  //     }
  //     printf("\n"); 

    if (bytes_read == 0) {
      ++file_finished_count;
      
      if (file_finished_count == file_count_local) { // finished all files
	if (G_FLAG == thread_count - 1 && op->global_checkpointing == 1) // last working thread
	  while (A_FLAG != thread_count - 1); // wait till all other threads at barrier
	
	atomic_fetch_add(&G_FLAG, 1);
      };
      
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

    if (i == 0) _byte++;
    
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
    printf("Thread#%d - File # %d - Byte: k + %d\n", file_index_offset + 1, i, _byte);

    //float cur_entry_val = OUTPUT_ENTRIES[output_index];
    
    if (output_index >= OUTPUT_SIZE) {
      ++OUTPUT_SIZE;
      ++C_LOCK_ENTRIES_SIZE;

      OUTPUT_ENTRIES = (float*) realloc(OUTPUT_ENTRIES, OUTPUT_SIZE * sizeof(float));
      OUTPUT_ENTRIES[output_index] = 0;

      if (op->lock_config == 2) {
	C_LOCK_ENTRIES = (Lock*) realloc(C_LOCK_ENTRIES, C_LOCK_ENTRIES_SIZE * sizeof(Lock));
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

  /*  if (op->global_checkpointing == 1) {
    atomic_fetch_add(&G_FLAG, 1);
    pthread_barrier_wait(&G_CHECKPOINT);
    
    while (G_FLAG != op->num_threads) {
      printf("Thread #%d is WAITING\n", file_index_offset + 1);      
      pthread_barrier_wait(&G_CHECKPOINT);
    }
    }*/

  printf("Thread #%d is OUT\n", file_index_offset + 1);

  mem_free(open_files_buffer);
  mem_free(read_buffer);
  mem_free(output_index_buffer);
  mem_free(prev_buffer);

  return NULL;
}

int main(int argc, char** argv) {
  int e_status;
  
  if (argc != EXPECTED_ARGC) {
    fprintf(stderr, ERROR_ARGC);
    exit(EXIT_FAILURE);
  }

  OUTPUT_SIZE = 1;
  OUTPUT_ENTRIES = (float*) calloc(OUTPUT_SIZE, sizeof(float));
  C_LOCK_ENTRIES = NULL;
  C_LOCK_ENTRIES_SIZE = 1; 
  C_LOCK = 0;

  //G_FLAG = 0;
  atomic_store(&G_FLAG, 0);

  int parse_status, bs, nt, lc, gcp;

  parse_status = 0;
  parse_status += parse_int(&bs, *(argv + 1));
  parse_status += parse_int(&nt, *(argv + 2));
  parse_status += parse_int(&lc, *(argv + 4));
  parse_status += parse_int(&gcp, *(argv + 5));

  if (parse_status > 0) {
    fprintf(stderr, ERROR_PARSE_INT_ARG);
    e_status = EXIT_FAILURE;
    goto EXIT;
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
  
  char* output_content = (char*) calloc(1, sizeof(char));

  /* Parse MetaData + Validate Parameters */
  FileData* fd = parse_metadata(mfp);
  ThreadArgs* t_args = (ThreadArgs*) malloc(sizeof(ThreadArgs) * nt);
  pthread_t* t_ids = (pthread_t*) malloc(sizeof(pthread_t) * nt);

  int error_count;
  if ((error_count = check_option_validity(op, fd)) > 0) {
    fprintf(stderr, "Total number of errors: %d\n", error_count);
    e_status = EXIT_FAILURE;
    goto EXIT;
  }

  if (gcp == 1) pthread_barrier_init(&G_CHECKPOINT, NULL, nt);
 
  /* Spawn Threads */
  for (int i = 0; i < nt; ++i) {
    t_args[i].fd = fd;
    t_args[i].op = op;
    t_args[i].offset = i;
    pthread_create(t_ids + i, NULL, compute_channels, t_args + i);
  }

  /* Join Threads */
  for (int i = 0; i < nt; ++i) pthread_join(t_ids[i], NULL);

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
  FILE* out = fopen("out.txt", "w+");
  fputs(output_content, out); /* Dump */
  fclose(out);
  printf("output_content:\n%s\n", output_content);

  e_status = EXIT_SUCCESS;
  
 EXIT:
  mem_free_metadata(fd);
  mem_free(output_content);
  mem_free(t_args);
  mem_free(t_ids);

  if (op->global_checkpointing == 1 && e_status != EXIT_FAILURE) pthread_barrier_destroy(&G_CHECKPOINT);
  
  mem_free(op);
  mem_free(C_LOCK_ENTRIES);
  mem_free(OUTPUT_ENTRIES);
  
  exit(e_status);
}


/* =======================================================================================
   =======================================================================================
   ==================================================================================== */

int check_option_validity(const ComputeOptions* op, const FileData* fd) {
  int violation_count = 0;
  
  if (op->buffer_size <= 0) {
    fprintf(stderr, "Error: invalid buffer size\n");
    violation_count++;
  }

  if (op->num_threads <= 0) {
    fprintf(stderr, "Error: invalid number of threads\n");
    violation_count++;
  }

  if (op->num_threads != 0 &&
      fd->channel_file_size != 0 &&
       fd->channel_file_size % op->num_threads != 0
      ) {
    fprintf(stderr, "Error: invalid ratio of files and threads\n");
    violation_count++;
  }

  if (op->global_checkpointing < 0 || op->global_checkpointing > 1) {
    fprintf(stderr, "Error: invalid checkpointing option\n");
    violation_count++;
  }

  if (op->lock_config < 1 || op->lock_config > 3) {
    fprintf(stderr, "Error: invalid lock configuration option\n");
    violation_count++;
  }

  return violation_count;
}

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

void mem_free_metadata(FileData* fd) {
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
