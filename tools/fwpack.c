#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define chunksize 4096
unsigned char tmpbuf[chunksize];

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        printf("usage: %s [input] [output]\n", argv[0]);
        return 0;
    }

    FILE* fin = fopen(argv[1], "rb");
    if (!fin)
    {
        printf("error: failed to open input file %s\n", argv[1]);
        return -1;
    }

    FILE* fout = fopen(argv[2], "wb");
    if (!fout)
    {
        printf("error: failed to create output file %s\n", argv[2]);
        fclose(fin);
        return -1;
    }

    int inputsize;
    fseek(fin, 0, SEEK_END);
    inputsize = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    unsigned int header[7*4] = {
        0x00000000, 0x00000070, 0x58444E49, 0x00000000, // INDX
        0x00000070, 0x00000004, 0x5F524556, 0x00000000, // VER_
        0x00000074, 0x00000000, 0x5F43564C, 0x00000002, // LVC_
        0x00000000, 0x00000000, 0x49464957, 0x00000000, // WIFI
        0x00000000, 0x00000000, 0x5F525245, 0x00000000, // ERR_
        0x00000000, 0x00000000, 0x5F494D55, 0x00000000, // UMI_
        0x00000000, 0x00000000, 0x5F474D49, 0x00000000, // IMG_
    };

    // fix LVC_ length
    header[2*4 + 1] = inputsize;

    fwrite(header, 7*16, 1, fout);

    unsigned int version = 0x190C0117; // fixme
    fwrite(&version, 4, 1, fout);

    for (int i = 0; i < inputsize; i += chunksize)
    {
        int thischunk = chunksize;
        if ((i + thischunk) > inputsize)
            thischunk = inputsize - i;

        fread(tmpbuf, thischunk, 1, fin);
        fwrite(tmpbuf, thischunk, 1, fout);
    }

    fclose(fout);
    fclose(fin);

    return 0;
}



