#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main (int argc, char **argv)
{
    FILE *source_vgm = NULL;
    uint8_t *buffer = NULL;
    uint32_t bytes_written = 0;

    if (argc != 2)
    {
        fprintf (stderr, "Error: No VGM file specified.\n");
        return EXIT_FAILURE;
    }

    source_vgm = fopen (argv[1], "r");
    if (source_vgm == NULL)
    {
        fprintf (stderr, "Error: Unable to open %s.\n", argv[1]);
        return EXIT_FAILURE;
    }

    /* TODO: Make this dynamic */
    buffer = malloc (12 * 1024);
    if (buffer == NULL)
    {
        fprintf (stderr, "Error: Unable to allocate 12K of memory.\n");
        fclose (source_vgm);
        return EXIT_FAILURE;
    }

    fread (buffer, sizeof (uint8_t), 12 * 1024, source_vgm);
    if (memcmp (buffer, "Vgm ", 4))
    {
        if (buffer[0] == 0x1f && buffer[1] == 0x8b)
        {
            fprintf (stderr, "Error: File %s appears to be compressed.\n", argv[1]);
        }
        else
        {
            fprintf (stderr, "Error: File %s is not a VGM.\n", argv[1]);
        }

        free (buffer);
        fclose (source_vgm);
        return EXIT_FAILURE;
    }

    printf ("Version: %x.\n",       * (uint32_t *)(&buffer[0x08]));
    printf ("Clock rate: %d Hz.\n", * (uint32_t *)(&buffer[0x0c]));
    printf ("Rate: %d Hz.\n",       * (uint32_t *)(&buffer[0x24]));

    /* TODO: For now, we assume the version is less than 1.50 and that
     *       data starts at 0x40 */

    for (uint32_t i = 0x40; i < (12 * 1024); i++)
    {
        switch (buffer[i])
        {
        case 0x50: /* PSG Data */
            printf ("Write %02x.\n", buffer[++i]);
            bytes_written++;
            break;
        case 0x61: /* Wait n 44.1 KHz samples */
            printf ("Delay: %d samples.\n", * (uint16_t *)(&buffer[i + 1]));
            i += 2;
            break;
        case 0x62: /* Wait 1/60 of a second */
            printf ("Delay: 1/60 of a second.\n");
            break;
        case 0x63: /* Wait 1/50 of a second */
            printf ("Delay: 1/50 of a second.\n");
            break;
        case 0x66: /* End of sound data */
            printf ("End of sound data.\n");
            i = 12 * 1024;
            break;
        default:
            printf ("Unknow command %02x.\n", buffer[i]);
        }
    }

    printf ("Done.\n");
    printf ("%d bytes written to PSG.\n", bytes_written);

    free (buffer);
    fclose (source_vgm);
}
