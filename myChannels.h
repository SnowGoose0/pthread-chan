#ifndef CHANNELS
#define CHANNELS

#define EXPECTED_ARGC                       0x7
#define TOK_DELIMITER                       "\n"
#define WHITESPACE_CHARS                    " \t\n\r\v\f"

#define STATE_FPATH                         0x00
#define STATE_ALPHA                         0x01
#define STATE_BETA                          0x02

#define STATE_COUNT                         0x03

#define DEFAULT_OUTPUT_SIZE                 0x01

typedef struct {
  char* path;
  float alpha;
  float beta;
} File;

typedef struct {
  int channel_file_size;
  File* channel_files;
} FileData;

typedef struct {
  int buffer_size;
  int num_threads;
  int lock_config;
  int global_checkpointing;

  char* metadata_file_path;
  char* output_file_path;
} ComputeOptions;

typedef struct {
  FileData* fd;
  ComputeOptions* op;
  int offset;
} ThreadArgs;

float compute_beta(float beta, float sample);
float compute_alpha(float alpha, float sample, float prev_sample);
int str_clean(char* src);
void free_metadata(FileData* fd);

void* compute_channels(void* t_args);

#define ERROR_ARGC                          "Error: Invalid argument count\n"
#define ERROR_PARSE_INT_ARG                 "Error: Invalid argument integer values\n"
#define ERROR_FILE_PATH                     "Error: Invalid file path\n"

#endif
