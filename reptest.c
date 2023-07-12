#include <stdio.h>
#include <stdlib.h>

#define COMMAND "valgrind --fair-sched=yes --show-leak-kinds=all --leak-check=full --log-file=log.txt ./myChannels 3 2 meta/meta16.txt 2 0 output.txt"
#define LOG_FILE "error_log.txt"
#define NUM_RUNS 100000

int main() {
    int i;
    FILE* logFile;
    char buffer[256];
    int exitCode;

    logFile = fopen(LOG_FILE, "w");
    if (logFile == NULL) {
        printf("Failed to open log file.\n");
        return 1;
    }

    for (i = 0; i < NUM_RUNS; i++) {
        sprintf(buffer, "%s 2>&1", COMMAND);
        exitCode = system(buffer);

        if (exitCode != 0) {
            fprintf(logFile, "Error in run %d:\n", i + 1);
            fclose(logFile);
            return 1;
        }
    }

    fclose(logFile);
    return 0;
}