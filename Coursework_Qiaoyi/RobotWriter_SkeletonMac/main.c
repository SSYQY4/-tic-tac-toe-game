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

int print_shape_count(char filename[])
{
    FILE *file = fopen(filename, "r");
    char header[64];
    char shape_name[64];
    int shape_count;
    int point_count;

    if (file == NULL) {
        printf("Could not open %s\n", filename);
        return 1;
    }

    if (read_name_and_count(file, header, 64, &shape_count) != 0 ||
        strcmp(header, "NumShapes") != 0) {
        fclose(file);
        printf("Invalid NumShapes header\n");
        return 1;
    }

    if (read_name_and_count(file, shape_name, 64, &point_count) != 0) {
        fclose(file);
        printf("Invalid first shape header\n");
        return 1;
    }

        struct StrokePoint *points = calloc(point_count, sizeof(struct StrokePoint));
    if (points == NULL) {
        fclose(file);
        printf("Could not allocate memory for the points\n");
        return 1;
    }

    for (int i = 0; i < point_count; i++) {
        int items_read = fscanf(file, "%f %f %d",
                                &points[i].x, &points[i].y, &points[i].pen);

        if (items_read != 3) {
            free(points);
            fclose(file);
            printf("Could not read point %d of %s\n", i + 1, shape_name);
            return 1;
        }

        if (points[i].pen != 0 && points[i].pen != 1) {
            free(points);
            fclose(file);
            printf("Invalid pen value at point %d of %s\n", i + 1, shape_name);
            return 1;
        }
    }

    fclose(file);

    printf("Number of shapes: %d\n", shape_count);
    printf("First shape: %s, points: %d\n", shape_name, point_count);

    for (int i = 0; i < point_count; i++) {
        printf("Point %d: X=%.2f Y=%.2f P=%d\n",
               i + 1, points[i].x, points[i].y, points[i].pen);
    }

    free(points);
    return 0;
}

int main()
{
    char buffer[100];

    if (print_shape_count("test.txt") != 0) 
    {
        return 1;
    }
    // If we cannot open the port then give up immediatly
    if ( CanRS232PortBeOpened() == -1 )
    {
        printf ("\nUnable to open the COM port (specified in serial.h) ");
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
    sprintf (buffer, "G0 X2.5 Y-2.5\n");
    SendCommands(buffer);
    sprintf (buffer, "S1000\n");
    SendCommands(buffer);
    sprintf (buffer, "G1 X7.5 Y-2.5\n");
    SendCommands(buffer);
    sprintf (buffer, "G1 X7.5 Y-7.5\n");
    SendCommands(buffer);
    sprintf (buffer, "G1 X2.5 Y-7.5\n");
    SendCommands(buffer);
    sprintf (buffer, "G1 X2.5 Y-2.5\n");
    SendCommands(buffer);
    sprintf (buffer, "S0\n");
    SendCommands(buffer);
    sprintf (buffer, "G0 X0 Y0\n");
    SendCommands(buffer);
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
