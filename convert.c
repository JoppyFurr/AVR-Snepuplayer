#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A struct to represent our pseudo-psg registers */
/* For now, just tones. Noise should be added later */
typedef struct psg_regs_s
{
    uint8_t tone_0;
    uint8_t tone_1;
    uint8_t tone_2;
    uint8_t volume_0;
    uint8_t volume_1;
    uint8_t volume_2;
} psg_regs;

#define TONE_0_BIT     0x01
#define TONE_1_BIT     0x02
#define TONE_2_BIT     0x04
#define NOISE_BIT      0x08
#define VOLUME_0_1_BIT 0x10
#define VOLUME_2_N_BIT 0x20

/* State tracking */
/* OwO - notices your globals */
psg_regs current_state = { 0 };
uint8_t write_delay_count = 0;

/* TODO: Support delays > 3/60s. */
int write_frame (void)
{
    static psg_regs previous_state;
    uint8_t frame[8] = { 0 };
    uint8_t frame_size = 1;

#if 0
    /* Dump state */
    printf ("{ Tone0: %02x:%01x, Tone1: %02x:%01x, Tone2: %02x:%01x, Delay: %02x }\n",
            current_state.tone_0, current_state.volume_0,
            current_state.tone_1, current_state.volume_1,
            current_state.tone_2, current_state.volume_2,
            write_delay_count);
#endif

    /* Header format description:
     *
     *  Bitfields: ddvv nttt
     *
     *  nttt -> 0001: Tone0 byte follows
     *          0010: Tone1 byte follows
     *          0100: Tone2 byte follows
     *          1000: Noise byte follows
     *
     *  vv   ->   01: Tone0 and Tone1 volume change follows
     *       ->   10: Tone2 and Noise volume change follows
     *
     *  dd   ->   00: End of data
     *            01: 1/60s delay after this data frame
     *            10: 2/60s delay after this data frame
     *            11: 3/60s delay after this data frame
     *
     *  Bytes follow in the order they appear in the above list.
     *
     *  While we lose two bits of accuracy for the tone registers,
     *  the output is only ~30% the size of the uncompressed VGM file.
     */

    frame[0] |= write_delay_count << 6;

    /* Tone0 */
    if (current_state.tone_0 != previous_state.tone_0)
    {
        frame[0] |= TONE_0_BIT;
        frame[frame_size++] = current_state.tone_0;
    }

    /* Tone1 */
    if (current_state.tone_1 != previous_state.tone_1)
    {
        frame[0] |= TONE_1_BIT;
        frame[frame_size++] = current_state.tone_1;
    }

    /* Tone2 */
    if (current_state.tone_2 != previous_state.tone_2)
    {
        frame[0] |= TONE_2_BIT;
        frame[frame_size++] = current_state.tone_2;
    }

    /* TODO: Noise */

    /* Volume 0/1 */
    if ((current_state.volume_0 != previous_state.volume_0) ||
        (current_state.volume_1 != previous_state.volume_1))
    {
        frame[0] |= VOLUME_0_1_BIT;
        frame[frame_size++] = current_state.volume_0 | (current_state.volume_1 << 4);
    }

    /* Volume 2/N */
    /* TODO: Noise */
    if (current_state.volume_2 != previous_state.volume_2)
    {
        frame[0] |= VOLUME_2_N_BIT;
        frame[frame_size++] = current_state.volume_2;
    }

    for (int i = 0; i < frame_size; i++)
    {
        printf ("%02x ", frame[i]);
    }
    printf ("\n");

    memcpy (&previous_state, &current_state, sizeof (psg_regs));

    return frame_size;
}

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
    uint32_t statistic_tone0_bytes_written = 0;
    uint32_t statistic_tone1_bytes_written = 0;
    uint32_t statistic_tone2_bytes_written = 0;
    uint32_t statistic_noise_bytes_written = 0;
    uint32_t statistic_frame_count = 0;
    uint32_t statistic_delays = 0;
    uint32_t statistic_total_output_size = 0;

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
            if (write_delay_count)
            {
                statistic_total_output_size += write_frame ();
                write_delay_count = 0;
                statistic_frame_count++;
            }
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
                case 0x00: statistic_tone0_bytes_written++;

                    /* printf ("Tone0 low    = %03x\n", data_low); */
                    current_state.tone_0 &= 0xfc;
                    current_state.tone_0 |= (data_low >> 2);
                    break;

                case 0x10: statistic_tone0_bytes_written++;

                    /* printf ("Tone0 volume = %01x\n", data_volume); */
                    current_state.volume_0 = data_volume;
                    break;

                /* Tone1 */
                case 0x20: statistic_tone1_bytes_written++;

                    /* printf ("Tone1 low    = %03x\n", data_low); */
                    current_state.tone_1 &= 0xfc;
                    current_state.tone_1 |= (data_low >> 2);
                    break;

                case 0x30: statistic_tone1_bytes_written++;

                    /* printf ("Tone1 volume = %01x\n", data_volume); */
                    current_state.volume_1 = data_volume;
                    break;

                /* Tone2 */
                case 0x40: statistic_tone2_bytes_written++;

                    /* printf ("Tone2 low    = %03x\n", data_low); */
                    current_state.tone_2 &= 0xfc;
                    current_state.tone_2 |= (data_low >> 2);
                    break;

                case 0x50: statistic_tone2_bytes_written++;

                    /* printf ("Tone2 volume = %01x\n", data_volume); */
                    current_state.volume_2 = data_volume;
                    break;

                /* Noise */
                case 0x60: statistic_noise_bytes_written++;

                    /* printf ("Noise        = %01x\n", data_low); */
                    break;

                case 0x70: statistic_noise_bytes_written++;

                    /* printf ("Noise volume = %01x\n", data_volume); */
                    break;
                }
            }
            else { /* Data-high */
                switch (latch)
                {
                /* Tone0 */
                case 0x00: statistic_tone0_bytes_written++;

                    /* printf ("Tone0 high   = %03x\n", data_high); */
                    current_state.tone_0 &= 0x03;
                    current_state.tone_0 |= (data_high >> 2);
                    break;

                case 0x10: statistic_tone0_bytes_written++;

                    /* printf ("Tone0 volume = %01x\n", data_volume); */
                    current_state.volume_0 = data_volume;
                    break;

                /* Tone1 */
                case 0x20: statistic_tone1_bytes_written++;

                    /* printf ("Tone1 high   = %03x\n", data_high); */
                    current_state.tone_1 &= 0x03;
                    current_state.tone_1 |= (data_high >> 2);
                    break;

                case 0x30: statistic_tone1_bytes_written++;

                    /* printf ("Tone1 volume = %01x\n", data_volume); */
                    current_state.volume_1 = data_volume;
                    break;

                /* Tone2 */
                case 0x40: statistic_tone2_bytes_written++;

                    /* printf ("Tone2 high   = %03x\n", data_high); */
                    current_state.tone_2 &= 0x03;
                    current_state.tone_2 |= (data_high >> 2);
                    break;

                case 0x50: statistic_tone2_bytes_written++;

                    /* printf ("Tone2 volume = %01x\n", data_volume); */
                    current_state.volume_2 = data_volume;
                    break;

                /* Noise */
                case 0x60: statistic_noise_bytes_written++;

                    /* printf ("Noise        = %01x\n", data_low); */
                    break;

                case 0x70: statistic_noise_bytes_written++;

                    /* printf ("Noise volume = %01x\n", data_volume); */
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
            /* printf ("--- Frame delay ---\n"); */
            write_delay_count++;
            statistic_delays++;
            break;
#if 0
        case 0x63: /* Wait 1/50 of a second */
            printf ("Delay: 1/50 of a second.\n");
            break;
#endif
        case 0x66: /* End of sound data */
            statistic_total_output_size += write_frame ();
            write_delay_count = 0;
            statistic_total_output_size += write_frame ();
            printf ("End of sound data.\n");
            i = 12 * 1024;
            break;
        default:
            printf ("Unknow command %02x.\n", buffer[i]);
        }
    }

    printf ("Done.\n");
    printf ("%d 1/60 second delays.\n", statistic_delays);
    printf ("%d bytes written for tone0.\n", statistic_tone0_bytes_written);
    printf ("%d bytes written for tone1.\n", statistic_tone1_bytes_written);
    printf ("%d bytes written for tone2.\n", statistic_tone2_bytes_written);
    printf ("%d bytes written for noise.\n", statistic_noise_bytes_written);
    printf ("%d bytes output.\n", statistic_total_output_size);

    free (buffer);
    fclose (source_vgm);
}
