#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s task|service\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "task") == 0)
    {
        sleep(3);
        printf("Task finished normally\n");
        return 0;
    }

    if (strcmp(argv[1], "service") == 0)
    {
        for (;;)
        {
            pause();
        }
    }

    fprintf(stderr, "Unknown worker mode: %s\n", argv[1]);
    return 1;
}