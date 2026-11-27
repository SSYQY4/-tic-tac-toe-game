#include <stdio.h>
#include <stdlib.h>

#define MAC

#ifdef MAC
#include <unistd.h>
#else
#include <Windows.h>
#include "rs232.h"
#include <conio.h>
#endif

#include "serial.h"

#define bdrate 115200               /* 115200 baud */

void SendCommands (char *buffer );

struct StrokePoint
{
    float x; // X coordinate of the stroke point
    float y; // Y coordinate of the stroke point
    int pen; // Pen state: 0 for pen up, 1 for pen down
};

struct Shape
{
    char name[64];
    int point_count;
    struct StrokePoint *points;
};

struct ShapeLibrary
{
    int shape_count;
    struct Shape *shapes;
};

int read_name_and_count(FILE *file, char name[], int capacity, int *count)
{
    char line[128];
    int position = 0;
    int name_length = 0;
    int digits = 0;
    int value = 0;

    if (fgets(line, 128, file) == NULL) {
        return 1;
    }

    while (line[position] == ' ' || line[position] == '\t') {
        position++;
    }

    while (line[position] != ' ' && line[position] != '\t' &&
           line[position] != '\n' && line[position] != '\r' &&
           line[position] != '\0') {
        if (name_length == capacity - 1) {
            return 1;
        }
        name[name_length] = line[position];
        name_length++;
        position++;
    }
    name[name_length] = '\0';

    if (name_length == 0 ||
        (line[position] != ' ' && line[position] != '\t')) {
        return 1;
    }

    while (line[position] == ' ' || line[position] == '\t') {
        position++;
    }

    while (line[position] >= '0' && line[position] <= '9') {
        if (digits == 9) {
            return 1;
        }
        value = value * 10 + (line[position] - '0');
        digits++;
        position++;
    }

    while (line[position] == ' ' || line[position] == '\t' ||
           line[position] == '\r') {
        position++;
    }

    if (value <= 0 ||
        (line[position] != '\n' && (line[position] != '\0' || !feof(file)))) {
        return 1;
    }

    *count = value;
    return 0;
}

void free_shapes(struct ShapeLibrary *library)
{
    for (int i = 0; i < library->shape_count; i++) {
        free(library->shapes[i].points);
    }
    free(library->shapes);
    library->shapes = NULL;
    library->shape_count = 0;
}

int read_shape(FILE *file, struct Shape *shape)
{
    if (read_name_and_count(file, shape->name, 64, &shape->point_count) != 0) {
        return 1;
    }

    shape->points = calloc(shape->point_count, sizeof(struct StrokePoint));
    if (shape->points == NULL) {
        return 1;
    }

    for (int i = 0; i < shape->point_count; i++) {
        struct StrokePoint *point = &shape->points[i];

        if (fscanf(file, "%f %f %d", &point->x, &point->y, &point->pen) != 3) {
            return 1;
        }
        if (point->pen != 0 && point->pen != 1) {
            return 1;
        }

        int next = fgetc(file);
        while (next == ' ' || next == '\t' || next == '\r') {
            next = fgetc(file);
        }
        if (next != '\n' && !feof(file)) {
            return 1;
        }
    }

    return 0;
}

int load_shapes(char filename[], struct ShapeLibrary *library)
{
    char header[64];
    int declared_count;

    library->shape_count = 0;
    library->shapes = NULL;

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Could not open %s\n", filename);
        return 1;
    }

    if (read_name_and_count(file, header, 64, &declared_count) != 0 ||
        strcmp(header, "NumShapes") != 0) {
        fclose(file);
        printf("Invalid NumShapes header\n");
        return 1;
    }

    library->shapes = calloc(declared_count, sizeof(struct Shape));
    if (library->shapes == NULL) {
        fclose(file);
        printf("Could not allocate the shape library\n");
        return 1;
    }

    for (int i = 0; i < declared_count; i++) {
        struct Shape *shape = &library->shapes[i];
        shape->points = NULL;
        library->shape_count++;

        if (read_shape(file, shape) != 0) {
            fclose(file);
            free_shapes(library);
            printf("Could not load shape %d\n", i + 1);
            return 1;
        }
    }

    fclose(file);
    return 0;
}
// Function to draw a shape using the G-code
void draw_shape(struct Shape *shape, float origin_x, float origin_y, float scale)
{
    char buffer[100];
    int pen_down = 0;

    sprintf(buffer, "G0 X%.3f Y%.3f\n", origin_x, origin_y);
    SendCommands(buffer);

    for (int i = 0; i < shape->point_count; i++) {
        struct StrokePoint *point = &shape->points[i];
        float x = origin_x + point->x * scale;
        float y = origin_y + point->y * scale;

        if (point->pen != pen_down) {
            sprintf(buffer, "S%d\n", point->pen * 1000);
            SendCommands(buffer);
            pen_down = point->pen;
        }

        sprintf(buffer, "G%d X%.3f Y%.3f\n", point->pen, x, y);
        SendCommands(buffer);
    }

    if (pen_down == 1) {
        sprintf(buffer, "S0\n");
        SendCommands(buffer);
    }
}

// Function to difine the size of shapes and where to start drawing
void draw_shape_in_cell(struct Shape *shape, float board_size, int column, int row)
{
    float cell_size = board_size / 3;
    float margin = 2;
    float scale = (cell_size - 2 * margin) / 16;

    float origin_x = (column - 1) * cell_size + margin;
    float origin_y = -board_size + (row - 1) * cell_size + margin;

    draw_shape(shape, origin_x, origin_y, scale);
}

int main()
{
    char buffer[100];

        struct ShapeLibrary library;
    if (load_shapes("test.txt", &library) != 0) {
        return 1;
    }

    printf("Number of shapes: %d\n", library.shape_count);
    int total_points = 0;
    for (int i = 0; i < library.shape_count; i++) {
        printf("Shape %d: %s, points: %d\n", i + 1,
               library.shapes[i].name, library.shapes[i].point_count);
        total_points += library.shapes[i].point_count;
    }
    printf("Total stroke points: %d\n", total_points);

        char shape_name[] = "STAR";
    struct Shape *selected_shape = NULL;

    for (int i = 0; i < library.shape_count && selected_shape == NULL; i++) {
        if (strcmp(library.shapes[i].name, shape_name) == 0) {
            selected_shape = &library.shapes[i];
        }
    }

    if (selected_shape == NULL) {
        printf("%s was not found in test.txt\n", shape_name);
        free_shapes(&library);
        return 1;
    }

    // If we cannot open the port then give up immediatly
    if ( CanRS232PortBeOpened() == -1 )
    {
        printf ("\nUnable to open the COM port (specified in serial.h) ");
        free_shapes(&library);
        exit (0);
    }

    // Time to wake up the robot
    printf ("\nAbout to wake up the robot\n");

    // We do this by sending a new-line
    sprintf (buffer, "\n");
    // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    #ifdef MAC
    sleep(0.1);
    #else
    Sleep(100);
    #endif


    // This is a special case - we wait  until we see a dollar ($)
    WaitForDollar();

    printf ("\nThe robot is now ready to draw\n");

    //These commands get the robot into 'ready to draw mode' and need to be sent before any writing commands
    sprintf (buffer, "G1 X0 Y0 F1000\n");
    SendCommands(buffer);
    sprintf (buffer, "M3\n");
    SendCommands(buffer);
    sprintf (buffer, "S0\n");
    SendCommands(buffer);

    // These are sample commands to draw out some information 
    // This is the section that you will replace with your own code
    draw_shape_in_cell(selected_shape, 60, 2, 2);
        
    sprintf(buffer, "G0 X0 Y0\n");
    SendCommands(buffer);
    free_shapes(&library);
    // End of sample g-code
    
    // Before we exit the program we need to close the COM port
    CloseRS232Port();
    printf("Com port now closed\n");

    return (0);
}

// Send the data to the robot - note in 'PC' mode you need to hit space twice
// as the dummy 'WaitForReply' has a getch() within the function.
void SendCommands (char *buffer )
{
    // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    WaitForReply();
    #ifdef MAC
    sleep(0.1);
    #else
    Sleep(100); // Can omit this when using the writing robot but has minimal effect
    #endif
}
