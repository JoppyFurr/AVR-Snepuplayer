#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main (int argc, char **argv)
{
    FILE *source_vgm = NULL;
    uint8_t *buffer = NULL;
    uint8_t latch = 0;
    uint8_t data = 0;
    uint16_t data_low = 0;
    uint16_t data_high = 0;
    uint16_t data_volume = 0;

    /* Statistics */
    uint32_t tone0_bytes_written = 0;
    uint32_t tone1_bytes_written = 0;
    uint32_t tone2_bytes_written = 0;
    uint32_t noise_bytes_written = 0;
    uint32_t delays = 0;

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

    /* TODO: Preprocess to make things easier on the micro
     *  * Truncate tone registers to 8-bits
     */

    for (uint32_t i = 0x40; i < (12 * 1024); i++)
    {
        switch (buffer[i])
        {
        case 0x4f:
            i++; /* Gamegear stereo data - Ignore */
            break;
        case 0x50: /* PSG Data */
            data = buffer[++i];
            data_low  = data & 0x0f;
            data_high = data << 0x04;
            /* Preprocess volume here to make things easier on the micro */
            data_volume = 0x0f - data_low;

            if (data & 0x80) { /* Latch + data-low (4-bits) */

                latch = data & 0x70;

                switch (latch)
                {
                /* Tone0 */
                case 0x00:
                    printf ("Tone0 low    = %03x\n", data_low);
                    tone0_bytes_written++;
                    break;
                case 0x10:
                    printf ("Tone0 volume = %01x\n", data_volume);
                    tone0_bytes_written++;
                    break;

                /* Tone1 */
                case 0x20:
                    printf ("Tone1 low    = %03x\n", data_low);
                    tone1_bytes_written++;
                    break;
                case 0x30:
                    printf ("Tone1 volume = %01x\n", data_volume);
                    tone1_bytes_written++;
                    break;

                /* Tone2 */
                case 0x40:
                    printf ("Tone2 low    = %03x\n", data_low);
                    tone2_bytes_written++;
                    break;
                case 0x50:
                    printf ("Tone2 volume = %01x\n", data_volume);
                    tone2_bytes_written++;
                    break;

                /* Noise */
                case 0x60:
                    printf ("Noise        = %01x\n", data_low);
                    noise_bytes_written++;
                    break;
                case 0x70:
                    printf ("Noise volume = %01x\n", data_volume);
                    noise_bytes_written++;
                    break;
                }
            }
            else { /* Data-high */
                switch (latch)
                {
                /* Tone0 */
                case 0x00:
                    printf ("Tone0 high   = %03x\n", data_high);
                    tone0_bytes_written++;
                    break;
                case 0x10:
                    printf ("Tone0 volume = %01x\n", data_volume);
                    tone0_bytes_written++;
                    break;

                /* Tone1 */
                case 0x20:
                    printf ("Tone1 high   = %03x\n", data_high);
                    tone1_bytes_written++;
                    break;
                case 0x30:
                    printf ("Tone1 volume = %01x\n", data_volume);
                    tone1_bytes_written++;
                    break;

                /* Tone2 */
                case 0x40:
                    printf ("Tone2 high   = %03x\n", data_high);
                    tone2_bytes_written++;
                    break;
                case 0x50:
                    printf ("Tone2 volume = %01x\n", data_volume);
                    tone2_bytes_written++;
                    break;

                /* Noise */
                case 0x60:
                    printf ("Noise        = %01x\n", data_low);
                    noise_bytes_written++;
                    break;
                case 0x70:
                    printf ("Noise volume = %01x\n", data_volume);
                    noise_bytes_written++;
                    break;
                }
            }
            break;
#if 0
        case 0x61: /* Wait n 44.1 KHz samples */
            printf ("Delay: %d samples.\n", * (uint16_t *)(&buffer[i + 1]));
            i += 2;
            break;
#endif
        case 0x62: /* Wait 1/60 of a second */
            printf ("--- Frame delay ---\n");
            delays++;
            break;
#if 0
        case 0x63: /* Wait 1/50 of a second */
            printf ("Delay: 1/50 of a second.\n");
            break;
#endif
        case 0x66: /* End of sound data */
            printf ("End of sound data.\n");
            i = 12 * 1024;
            break;
        default:
            printf ("Unknow command %02x.\n", buffer[i]);
        }
    }

    printf ("Done.\n");
    printf ("%d 1/60 second delays.\n", delays);
    printf ("%d bytes written for tone0.\n", tone0_bytes_written);
    printf ("%d bytes written for tone1.\n", tone1_bytes_written);
    printf ("%d bytes written for tone2.\n", tone2_bytes_written);
    printf ("%d bytes written for noise.\n", noise_bytes_written);

    free (buffer);
    fclose (source_vgm);
}
