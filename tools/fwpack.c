#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define chunksize 4096
unsigned char tmpbuf[chunksize];

int main(int argc, char** argv)
{
    int argsgood = 1;
    int hasver = 0;
    unsigned int version = 0x190C0117;

    if (argc < 3) argsgood = 0;
    else
    {
        for (int i = 1; i < argc-1; i++)
        {
            if (strlen(argv[i]) < 6)
            {
                argsgood = 0;
                break;
            }

            if (argv[i][4] != '=')
            {
                argsgood = 0;
                break;
            }

            if (!strncmp(argv[i], "VER_", 4))
            {
                hasver = 1;
                sscanf(argv[i], "VER_=%x", &version);
            }
        }
    }

    if (!argsgood)
    {
        printf("usage: %s [VER_=version] AAAA=inputA [BBBB=inputB ...] output\n", argv[0]);
        return 0;
    }

    char* outfile = argv[argc-1];

    int numentries = argc - 2;
    if (!hasver) numentries++;
    numentries++;
    printf("creating %d entries\n", numentries);

    unsigned int* header = (unsigned int*)malloc(numentries * 16);
    header[0] = 0;
    header[1] = numentries * 16;
    header[2] = 0x58444E49; // INDX
    header[3] = 0;

    header[4] = header[1];
    header[5] = 4;
    header[6] = 0x5F524556; // VER_
    header[7] = 0;

    FILE* fout = fopen(outfile, "wb");
    if (!fout)
    {
        printf("error: failed to create output file %s\n", outfile);
        return -1;
    }

    fseek(fout, header[1], SEEK_SET);
    printf("writing VER_ at %08X: version=%08X\n", (unsigned int)ftell(fout), version);
    fwrite(&version, 4, 1, fout);

    unsigned int* curhdr = &header[8];
    for (int i = 1; i < argc-1; i++)
    {
        if (!strncmp(argv[i], "VER_", 4))
            continue;

        char fname[512];
        strncpy(fname, &argv[i][5], 511); fname[511] = '\0';
        FILE* fin = fopen(fname, "rb");
        if (!fin)
        {
            printf("error: failed to open input file %s\n", fname);
            fclose(fout);
            remove(outfile);
            return -1;
        }

        int inputsize;
        fseek(fin, 0, SEEK_END);
        inputsize = ftell(fin);
        fseek(fin, 0, SEEK_SET);

        unsigned int tag = argv[i][0] | (argv[i][1] << 8) | (argv[i][2] << 16) | (argv[i][3] << 24);

        curhdr[0] = curhdr[-4] + curhdr[-3];
        curhdr[1] = inputsize;
        curhdr[2] = tag;
        curhdr[3] = 0; // TODO: allow setting blob version

        printf("writing %c%c%c%c at %08X: %d bytes\n",
               argv[i][0], argv[i][1], argv[i][2], argv[i][3],
               (unsigned int)ftell(fout), inputsize);

        for (int j = 0; j < inputsize; j += chunksize)
        {
            int thischunk = chunksize;
            if ((j + thischunk) > inputsize)
                thischunk = inputsize - j;

            fread(tmpbuf, thischunk, 1, fin);
            fwrite(tmpbuf, thischunk, 1, fout);
        }

        fclose(fin);
        curhdr += 4;
    }

    fseek(fout, 0, SEEK_SET);
    fwrite(header, numentries*16, 1, fout);

    fclose(fout);
    free(header);

    return 0;
}



