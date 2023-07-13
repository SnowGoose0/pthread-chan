#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MAX_COMMAND_LENGTH 100
#define MAX_FILENAME_LENGTH 100
#define MAX_LINE_LENGTH 1000

#define META_LENGTH 20
#define INPUT_LENGTH 10

const char* multiThreadChannel = "./myChannels %d %d %s %d %d output.txt > /dev/null";
const char* singleThreadChannel = "./mySingleChannels %d %d %s %d %d sample.txt > /dev/null";

const char* memCheck = "valgrind --log-file=log.txt --fair-sched=yes ./myChannels %d %d %s %d %d output.txt > /dev/null";

char meta_files[21][20] = {
  "meta/meta1.txt",
  "meta/meta2.txt",
  "meta/meta3.txt",
  "meta/meta4.txt",
  "meta/meta5.txt",
  "meta/meta6.txt",
  "meta/meta7.txt",
  "meta/meta8.txt",
  "meta/meta9.txt",
  "meta/meta10.txt",
  "meta/meta11.txt",
  "meta/meta12.txt",
  "meta/meta13.txt",
  "meta/meta14.txt",
  "meta/meta15.txt",
  "meta/meta16.txt",
  "meta/meta17.txt",
  "meta/meta18.txt",
  "meta/meta19.txt",
  "meta/meta20.txt",
  "meta/smeta3.txt"
};

void mem_clear(char* arr, int length) {
  for (int i = 0; i < length; i++) {
    *(arr + i) = 0;
  }
}

int isStringInFile(const char* filename, const char* searchString) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Failed to open the file.\n");
        return 0;
    }

    char line[256]; // Assuming a maximum line length of 255 characters
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, searchString) != NULL) {
            fclose(file);
            return 1; // String found in the file
        }
    }

    fclose(file);
    return 0; // String not found in the file
}

int compareFiles(const char* file1, const char* file2) {
    char command[256];
    snprintf(command, sizeof(command), "grep -Fxvf %s %s", file1, file2);

    FILE* commandOutput = popen(command, "r");
    if (commandOutput == NULL) {
        perror("Failed to execute grep command");
        exit(1);
    }

    char buffer[256];
    size_t bytesRead;
    int diffCount = 0;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), commandOutput)) > 0) {
        diffCount += (int)bytesRead;
    }

    pclose(commandOutput);

    return diffCount;
}

	      // Compare line by line
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BLUE_BOLD "\x1b[1;34m"

int main(int argc, char** argv) {
    int n = 100000; // Number of times to execute myChannel
    int test_counter = 0;
    int counter = 0; // Counter for differences
    int mem_counter = 0;
    char c_command[MAX_COMMAND_LENGTH];
    char command[MAX_COMMAND_LENGTH];
    char filename[MAX_FILENAME_LENGTH];
    char line1[MAX_LINE_LENGTH];
    char line2[MAX_LINE_LENGTH];

    int memcheck = 0;
    int edge = 1;

    if (argc > 1) {
      char op;
      int i = 1;

      while (i != argc) {
	op = argv[i][0];
	
	if (op == 'm') {
	  memcheck = 1;
	} else if (op == 'e') {
	  edge = 1;	  
	}
	
	++i;
      }
    }

    for (int i = 0; i < META_LENGTH + 1; i++) {
      char* meta_path = meta_files[i];

      for (int bytes = 1; bytes <= 5; bytes++) {
	for (int threads = 1; threads <= 2; threads++) {
	  for (int lock = 1; lock <= 3; lock++) {
	    for (int check = 0; check <= 1; check++) {
	      test_counter++;
	      sprintf(c_command, multiThreadChannel,
		      bytes, threads, meta_path, lock, check);
	      sprintf(command, singleThreadChannel,
		      bytes, threads, meta_path, lock, check);

	      printf("%s TRYING: %s!%s\n", ANSI_COLOR_BLUE_BOLD, c_command, ANSI_COLOR_RESET);
	      int c_command_status = system(c_command);
	      int command_status = system(command);
	      int status = compareFiles("output.txt", "sample.txt");
	      
	      if (!status && !WEXITSTATUS(c_command_status) && !WEXITSTATUS(command_status)) {
		printf("%s %sOK!%s\n", c_command,  ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
	      } else {
		counter++;
		printf("%s %sERROR!%s\n", c_command,  ANSI_COLOR_RED, ANSI_COLOR_RESET);
		system("cat output.txt > error.txt");
		system("cat sample.txt > error_s.txt");
	      }

	      if (memcheck) {
		sprintf(c_command, memCheck,
			bytes, threads, meta_path, lock, check);
		system(c_command);

		if (!isStringInFile("log.txt", "All heap blocks were freed -- no leaks are possible")) {
		  printf("%s LEAK DETECTED %s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
		  mem_counter++;
		} else {
		  printf("%s MEM OK! %s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
		}

		fflush(stdout);

		mem_clear(c_command, MAX_COMMAND_LENGTH);
		mem_clear(command, MAX_COMMAND_LENGTH);
	      }
	    }
	  }
	}
      }
    }

    

    printf("Total Failed Cases: %d/%d\n", counter, test_counter);
    printf("Total Mem Leaks: %d\n", mem_counter);
    
    return 0;
}
















