#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define RESET "\033[0m"
#define YELLOW "\033[48;5;226m  \033[0m"     // Y - Bright Yellow
#define ORANGE "\033[48;5;214m  \033[0m"     // O - Orange
#define RED "\033[48;5;196m  \033[0m"        // R - Bright Red
#define MAROON "\033[48;5;124m  \033[0m"     // L - Dark Red/Maroon
#define PINK "\033[48;5;205m  \033[0m"       // P - Hot Pink
#define PURPLE "\033[48;5;93m  \033[0m"      // U - Purple
#define SKYBLUE "\033[48;5;51m  \033[0m"     // C - Sky Blue
#define DARKBLUE "\033[48;5;21m  \033[0m"    // B - Dark Blue
#define LIGHTBLUE "\033[48;5;250m  \033[0m"  // A - Light Blue
#define TEAL "\033[48;5;79m  \033[0m"        // X - Teal/Aqua
#define DARKGREEN "\033[48;5;28m  \033[0m"   // D - Dark Green
#define LIGHTGREEN "\033[48;5;118m  \033[0m" // G - Light Green

const char* getColor(char c) {
    switch(c) {
        case 'Y': return YELLOW;
        case 'O': return ORANGE;
        case 'R': return RED;
        case 'L': return MAROON;
        case 'P': return PINK;
        case 'U': return PURPLE;
        case 'C': return SKYBLUE;
        case 'B': return DARKBLUE;
        case 'A': return LIGHTBLUE;
        case 'X': return TEAL;
        case 'D': return DARKGREEN;
        case 'G': return LIGHTGREEN;
        default: return "  ";
    }
}

void printColoredGrid(char grid[5][11]) {
    printf("\nColored Grid Visualization:\n");
    printf("==========================\n");
    
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 11; j++) {
            printf("%s", getColor(grid[i][j]));
        }
        printf("\n");
    }
    printf("\n");
}

void printLegend() {
    printf("Color Legend:\n");
    printf("=============\n");
    printf("Y - Yellow:     %s\n", YELLOW);
    printf("O - Orange:     %s\n", ORANGE);
    printf("R - Red:        %s\n", RED);
    printf("L - Maroon:     %s\n", MAROON);
    printf("P - Pink:       %s\n", PINK);
    printf("U - Purple:     %s\n", PURPLE);
    printf("C - Sky Blue:   %s\n", SKYBLUE);
    printf("B - Dark Blue:  %s\n", DARKBLUE);
    printf("A - Light Blue: %s\n", LIGHTBLUE);
    printf("X - Teal:       %s\n", TEAL);
    printf("D - Dark Green: %s\n", DARKGREEN);
    printf("G - Light Green:%s\n", LIGHTGREEN);
    printf("\n");
}

int createIndex(const char* solutions_path, const char* index_path) {
    FILE *solutions = fopen(solutions_path, "r");
    if (!solutions) {
        printf("Error: Cannot open %s\n", solutions_path);
        return 0;
    }
    
    FILE *index = fopen(index_path, "wb");
    if (!index) {
        printf("Error: Cannot create index file %s\n", index_path);
        fclose(solutions);
        return 0;
    }
    
    printf("Creating index file... This may take a moment.\n");
    
    char line[256];
    long position;
    int solution_count = 0;
    
    while (fgets(line, sizeof(line), solutions)) {
        if (strncmp(line, "Solution ", 9) == 0) {
            position = ftell(solutions);
            fwrite(&position, sizeof(long), 1, index);
            solution_count++;
            
            if (solution_count % 1000000 == 0) {
                printf("Indexed %d solutions...\n", solution_count);
            }
        }
    }
    
    fclose(solutions);
    fclose(index);
    
    printf("Index created successfully for %s! Found %d solutions.\n", solutions_path, solution_count);
    return 1;
}

int isIndexStale(const char* solutions_path, const char* index_path) {
    struct stat solutions_stat, index_stat;

    if (stat(solutions_path, &solutions_stat) != 0) {
        printf("Error: Source file %s not found.\n", solutions_path);
        return -1;
    }

    if (stat(index_path, &index_stat) != 0) {
        return 1;
    }

    if (solutions_stat.st_mtime > index_stat.st_mtime) {
        return 1;
    }

    return 0;
}

long getSolutionPosition(int solution_num, const char* index_path) {
    FILE *index = fopen(index_path, "rb");
    if (!index) return -1;
    
    if (fseek(index, (solution_num - 1) * sizeof(long), SEEK_SET) != 0) {
        fclose(index);
        return -1;
    }
    
    long position;
    if (fread(&position, sizeof(long), 1, index) != 1) {
        fclose(index);
        return -1;
    }
    
    fclose(index);
    return position;
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <s|m> <solution_number>\n", argv[0]);
        fprintf(stderr, "  s: Use 'serial/solutions.txt'\n");
        fprintf(stderr, "  m: Use 'mpi/solutions.txt'\n");
        return 1;
    }

    const char* mode_arg = argv[1];
    if (strcmp(mode_arg, "s") != 0 && strcmp(mode_arg, "m") != 0) {
        fprintf(stderr, "Error: Invalid mode '%s'. Please use 's' for serial or 'm' for mpi.\n", mode_arg);
        return 1;
    }

    int solution_num = atoi(argv[2]);
    if (solution_num < 1 || solution_num > 4331140) {
        fprintf(stderr, "Error: Solution number must be between 1 and 4331140\n");
        return 1;
    }

    const char* folder = (strcmp(mode_arg, "s") == 0) ? "serial" : "mpi";
    char solutions_path[256];
    char index_path[256];
    sprintf(solutions_path, "%s/solutions.txt", folder);
    sprintf(index_path, "%s/solutions.idx", folder);

    int stale_status = isIndexStale(solutions_path, index_path);
    if (stale_status == -1) {
        return 1;
    }
    
    if (stale_status == 1) {
        printf("Index file is missing or outdated. Regenerating for %s...\n", solutions_path);
        if (!createIndex(solutions_path, index_path)) {
            fprintf(stderr, "Failed to create index. Cannot proceed.\n");
            return 1;
        }
    }

    long position = getSolutionPosition(solution_num, index_path);
    if (position == -1) {
        fprintf(stderr, "Error: Solution %d not found in index for %s\n", solution_num, solutions_path);
        return 1;
    }
    
    FILE *file = fopen(solutions_path, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open %s\n", solutions_path);
        return 1;
    }
    
    if (fseek(file, position, SEEK_SET) != 0) {
        fprintf(stderr, "Error: Cannot seek to solution position\n");
        fclose(file);
        return 1;
    }
    
    char line[256];
    char grid[5][11] = {{0}};
    
    printf("\nFound Solution %d from %s:\n", solution_num, solutions_path);
    printf("==================================\n");
    
    for (int i = 0; i < 5; i++) {
        if (fgets(line, sizeof(line), file)) {
            int col = 0;
             for (size_t j = 0; j < strlen(line) && col < 11; j++) {
                 if (line[j] != ' ' && line[j] != '\n' && line[j] != '\r') {
                     grid[i][col] = line[j];
                     col++;
                 }
             }
            printf("%s", line);
        }
    }
    
    fclose(file);
    
    printColoredGrid(grid);
    printLegend();
    
    return 0;
}