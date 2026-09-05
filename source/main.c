#include <stdio.h>
#include <string.h>

#include <3ds.h>

int main(int argc, char* argv[])
{
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    consoleInit(GFX_BOTTOM, NULL);

    printf("\x1b[1;1HBVG Navigator");
    printf("\x1b[3;1HBerlin public transport");
    printf("\x1b[5;1H");
    printf("Starting...");

    printf("\x1b[8;1H");
    printf("A = Search");
    printf("\x1b[9;1H");
    printf("B = Exit");

    while (aptMainLoop())
    {
        hidScanInput();

        u32 kDown = hidKeysDown();

        if (kDown & KEY_A)
        {
            printf("\x1b[12;1H");
            printf("Search selected!");
        }

        if (kDown & KEY_B)
        {
            break;
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();

    return 0;
}

