#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

const uint8_t  vgm_magic[4] = { 'V', 'g', 'm', ' ' };
const uint8_t gzip_magic[3] = { 0x1f, 0x8b, 0x08 };

#define SOURCE_SIZE_MAX 524288
#define OUTPUT_SIZE_MAX 7680

uint8_t *read_vgz (char *filename)
{
    gzFile source_vgz = NULL;
    uint8_t file_magic[4] = { 0 };
    uint8_t scratch[128] = { 0 };
    uint8_t *buffer = NULL;
    uint32_t filesize = 0;

    source_vgz = gzopen (filename, "rb");
    if (source_vgz == NULL)
    {
        fprintf (stderr, "Error: Unable to open vgz %s.\n", filename);
        return NULL;
    }

    gzread (source_vgz, file_magic, 4);
    gzrewind (source_vgz);

    /* Check the magic bytes are valid */
    if (memcmp (file_magic, vgm_magic, 4) != 0)
    {
        fprintf (stderr, "Error: File is not a valid VGM.\n");
        gzclose (source_vgz);
        return NULL;
    }

    /* Get the uncompressed filesize by reading the contents */
    while (gzread (source_vgz, scratch, 128) == 128);
    filesize = gztell (source_vgz);
    gzrewind (source_vgz);

    if (filesize > SOURCE_SIZE_MAX)
    {
        fprintf (stderr, "Error: Source file (uncompressed) larger than 512 KiB.\n");
        gzclose (source_vgz);
        return NULL;
    }

    /* Allocate a buffer */
    buffer = malloc (filesize);
    if (buffer == NULL)
    {
        fprintf (stderr, "Error: Unable to allocate %d bytes of memory.\n", filesize);
        gzclose (source_vgz);
        return NULL;
    }

    /* Read the file */
    if (gzread (source_vgz, buffer, filesize) != filesize)
    {
        fprintf (stderr, "Error: Unable to read %d bytes from file.\n", filesize);
        gzclose (source_vgz);
        free (buffer);
        return NULL;
    }

    gzclose (source_vgz);

    return buffer;
}

uint8_t *read_vgm (char *filename)
{
    FILE *source_vgm = NULL;
    uint8_t file_magic[4] = { 0 };
    uint8_t *buffer = NULL;
    uint32_t filesize = 0;

    source_vgm = fopen (filename, "rb");
    if (source_vgm == NULL)
    {
        fprintf (stderr, "Error: Unable to open %s.\n", filename);
        return NULL;
    }

    fread (file_magic, sizeof (uint8_t), 4, source_vgm);
    rewind (source_vgm);

    /* First, check if we should be using the vgz path instead */
    if (memcmp (file_magic, gzip_magic, 3) == 0)
    {
        fclose (source_vgm);

        return read_vgz (filename);
    }

    if (memcmp (file_magic, vgm_magic, 4) != 0)
    {
        fclose (source_vgm);
        fprintf (stderr, "Error: File is not a valid VGM.\n");
        return NULL;
    }

    /* Get the filesize */
    fseek (source_vgm, 0, SEEK_END);
    filesize = ftell (source_vgm);

    rewind (source_vgm);

    if (filesize > SOURCE_SIZE_MAX)
    {
        fprintf (stderr, "Error: Source file larger than 512 KiB.\n");
        fclose (source_vgm);
        return NULL;
    }

    /* Allocate a buffer */
    buffer = malloc (filesize);
    if (buffer == NULL)
    {
        fprintf (stderr, "Error: Unable to allocate %d bytes of memory.\n", filesize);
        fclose (source_vgm);
        return NULL;
    }

    /* Read the file */
    if (fread (buffer, sizeof (uint8_t), filesize, source_vgm) != filesize)
    {
        fprintf (stderr, "Error: Unable to read %d bytes from file.\n", filesize);
        fclose (source_vgm);
        free (buffer);
        return NULL;
    }

    fclose (source_vgm);

    return buffer;
}

/* A struct to represent our pseudo-psg registers */
/* For now, just tones. Noise should be added later */
typedef struct psg_regs_s
{
    uint8_t tone_0;
    uint8_t tone_1;
    uint8_t tone_2;
    uint8_t noise;
    uint8_t volume_0;
    uint8_t volume_1;
    uint8_t volume_2;
    uint8_t volume_3;
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
uint32_t samples_delay = 0;

uint8_t  output[OUTPUT_SIZE_MAX + 6] = { 0 };
uint32_t output_size = 0;

/* TODO: For PAL music, perhaps define delay as multiples of 1/50, or have
 *       a shorter delay like 1/300 that can cleanly describy both PAL and
 *       NTSC timings. */
int write_frame (void)
{
    static psg_regs previous_state;
    uint8_t frame[32] = { 0 };
    uint8_t frame_size = 1;
    uint16_t frame_delay = samples_delay / 735;

    samples_delay -= frame_delay * 735;

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
     *  dd   ->   00: No delay
     *            01: 1/60s delay after this data frame
     *            10: 2/60s delay after this data frame
     *            11: 3/60s delay after this data frame
     *
     *  Bytes follow in the order they appear in the above list.
     *
     *  While we lose two bits of accuracy for the tone registers,
     *  the output is only ~30% the size of the uncompressed VGM file.
     */

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

    /* Noise */
    if (current_state.noise != previous_state.noise)
    {
        frame[0] |= NOISE_BIT;
        frame[frame_size++] = current_state.noise;
    }

    /* Volume 0/1 */
    if ((current_state.volume_0 != previous_state.volume_0) ||
        (current_state.volume_1 != previous_state.volume_1))
    {
        frame[0] |= VOLUME_0_1_BIT;
        frame[frame_size++] = current_state.volume_0 | (current_state.volume_1 << 4);
    }

    /* Volume 2/N */
    if ((current_state.volume_2 != previous_state.volume_2) ||
        (current_state.volume_3 != previous_state.volume_3))
    {
        frame[0] |= VOLUME_2_N_BIT;
        frame[frame_size++] = current_state.volume_2 | (current_state.volume_3 << 4);
    }

    if (frame_delay > 3)
    {
        frame[0] |= 3 << 6;
        frame_delay -= 3;
    }
    else
    {
        frame[0] |= frame_delay << 6;
        frame_delay = 0;
    }

    while (frame_delay != 0)
    {
        if (frame_delay > 3)
        {
            frame[frame_size++] |= 3 << 6;
            frame_delay -= 3;
        }
        else
        {
            frame[frame_size++] |= frame_delay << 6;
            frame_delay = 0;
        }
    }

    for (int i = 0; i < frame_size; i++)
    {
        output [output_size++] = frame[i];
    }

    memcpy (&previous_state, &current_state, sizeof (psg_regs));

    return frame_size;
}

int main (int argc, char **argv)
{
    /* File I/O */
    char *filename = argv[1];
    uint8_t *buffer = NULL;
    uint32_t vgm_offset = 0;

    /* PSG */
    uint8_t latch = 0;
    uint8_t data = 0;
    uint16_t data_low = 0;
    uint16_t data_high = 0;
    uint16_t data_volume = 0;

    if (argc != 2)
    {
        fprintf (stderr, "Error: No VGM file specified.\n");
        return EXIT_FAILURE;
    }

    buffer = read_vgm (filename);

    if (buffer == NULL)
    {
        /* read_vgm should already have output an error message */
        return EXIT_FAILURE;
    }

    fprintf (stderr, "Version: %x.\n",       * (uint32_t *)(&buffer[0x08]));
    fprintf (stderr, "Clock rate: %d Hz.\n", * (uint32_t *)(&buffer[0x0c]));
    fprintf (stderr, "Rate: %d Hz.\n",       * (uint32_t *)(&buffer[0x24]));
    fprintf (stderr, "VGM offset: %02x.\n",  * (uint32_t *)(&buffer[0x34]));

    /* TODO: For now, we're assuming a little-endian host */
    if (* (uint32_t *)(&buffer[0x34]) != 0)
    {
        vgm_offset = 0x34 + * (uint32_t *)(&buffer[0x34]);
    }
    else
    {
        vgm_offset = 0x40;
    }

    /* TODO: Support for repeating */

    for (uint32_t i = 0x40; (i < SOURCE_SIZE_MAX) && (output_size < OUTPUT_SIZE_MAX); i++)
    {
        switch (buffer[i])
        {
        case 0x4f:
            i++; /* Gamegear stereo data - Ignore */
            break;

        case 0x50: /* PSG Data */
            if (samples_delay >= 735)
            {
                write_frame ();
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
                case 0x00:
                    current_state.tone_0 &= 0xfc;
                    current_state.tone_0 |= (data_low >> 2);
                    break;

                case 0x10:
                    current_state.volume_0 = data_volume;
                    break;

                /* Tone1 */
                case 0x20:
                    current_state.tone_1 &= 0xfc;
                    current_state.tone_1 |= (data_low >> 2);
                    break;

                case 0x30:
                    current_state.volume_1 = data_volume;
                    break;

                /* Tone2 */
                case 0x40:
                    current_state.tone_2 &= 0xfc;
                    current_state.tone_2 |= (data_low >> 2);
                    break;

                case 0x50:
                    current_state.volume_2 = data_volume;
                    break;

                /* Noise */
                case 0x60:
                    current_state.noise = data_low;
                    break;

                case 0x70:
                    current_state.volume_3 = data_volume;
                    break;
                }
            }
            else { /* Data-high */
                switch (latch)
                {
                /* Tone0 */
                case 0x00:
                    current_state.tone_0 &= 0x03;
                    current_state.tone_0 |= (data_high >> 2);
                    break;

                case 0x10:
                    current_state.volume_0 = data_volume;
                    break;

                /* Tone1 */
                case 0x20:
                    current_state.tone_1 &= 0x03;
                    current_state.tone_1 |= (data_high >> 2);
                    break;

                case 0x30:
                    current_state.volume_1 = data_volume;
                    break;

                /* Tone2 */
                case 0x40:

                    current_state.tone_2 &= 0x03;
                    current_state.tone_2 |= (data_high >> 2);
                    break;

                case 0x50:
                    current_state.volume_2 = data_volume;
                    break;

                /* Noise */
                case 0x60:
                    current_state.noise = data_low;
                    break;

                case 0x70:
                    current_state.volume_3 = data_volume;
                    break;
                }
            }
            break;

        case 0x61: /* Wait n 44.1 KHz samples */
            samples_delay += * (uint16_t *)(&buffer[i+1]);
            i += 2;
            break;

        case 0x62: /* Wait 1/60 of a second */
            samples_delay += 735;
            break;

        case 0x63: /* Wait 1/50 of a second */
            samples_delay += 882;
            break;

        case 0x66: /* End of sound data */
            write_frame ();
            i = SOURCE_SIZE_MAX;
            break;

        /* 0x7n: Wait n+1 samples */
        case 0x70: case 0x71: case 0x72: case 0x73:
        case 0x74: case 0x75: case 0x76: case 0x77:
        case 0x78: case 0x79: case 0x7a: case 0x7b:
        case 0x7c: case 0x7d: case 0x7e: case 0x7f:
            samples_delay += 1 + (buffer[i] & 0x0f);
            break;

        default:
            fprintf (stderr, "Unknow command %02x.\n", buffer[i]);
            break;
        }
    }

    printf (".db");
    for (int i = 0; i < output_size; i++)
    {
        printf (" 0x%02x%s", output[i], (i & 7) == 7 ? "\n.db" : ",");
    }
    printf (" 0x00\n");

    fprintf (stderr, "Done. %d bytes output.\n", output_size);

    free (buffer);
}
