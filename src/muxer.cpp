#include <stdio.h>
#include "muxer_core.h"

static void usage(const char* program_name)
{
    printf("usage: %s video_file audio_file output_file\n", program_name);
}

int main(int argc, char** argv)
{

    if (argc < 4) {
        usage(argv[0]);
        return 1;
    }

    int result = 0;
    do {
        result = init_muxer(argv[1], argv[2], argv[3]);
        if (result < 0) {
            break;
        }

        result = muxing();
        if (result < 0) {
            break;
        }
    } while (0);

    destory_muxer();
    return 0;
}