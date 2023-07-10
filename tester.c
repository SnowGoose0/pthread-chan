#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_COMMAND_LENGTH 100
#define MAX_FILENAME_LENGTH 100
#define MAX_LINE_LENGTH 1000

int main() {
    int n = 100000; // Number of times to execute myChannel
    int counter = 0; // Counter for differences
    char command[MAX_COMMAND_LENGTH];
    char filename[MAX_FILENAME_LENGTH];
    char line1[MAX_LINE_LENGTH];
    char line2[MAX_LINE_LENGTH];

    // Execute myChannel n times
    for (int i = 0; i < n; i++) {
        sprintf(command, "./myChannel 3 2 meta.txt 2 0 out.txt > output.txt"); // Command to execute myChannel and redirect output
        system(command);

        // Compare output with sample.txt
        sprintf(filename, "output.txt");
        FILE *outputFile = fopen(filename, "r");
        FILE *sampleFile = fopen("sample.txt", "r");

        if (outputFile == NULL || sampleFile == NULL) {
            printf("Error opening files.\n");
            return 1;
        }

        // Compare line by line
        int lineNum = 1;
        while (fgets(line1, sizeof(line1), outputFile) != NULL && fgets(line2, sizeof(line2), sampleFile) != NULL) {
	  if (lineNum < 23) {
	    lineNum++;
	    continue;
	  }
            if (strcmp(line1, line2) != 0) {
                counter++;
                printf("Difference found in line %d.\n", lineNum);
            }
            lineNum++;
        }

        fclose(outputFile);
        fclose(sampleFile);
    }

    printf("Total differences: %d\n", counter);

    return 0;
}
