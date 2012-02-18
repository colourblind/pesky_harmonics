#include <string.h>
#include <assert.h>
#include "kissFFT/kiss_fftr.h"
#include "png.h"

const int SAMPLES_PER_BLOCK_TIME = 1;
const int SAMPLES_PER_BLOCK_FREQ = 1024;
const float POWER_SCALE = 5.f;

int save_png(char *filename, unsigned char *data, int width, int height)
{
    png_structp png_ptr;
    png_infop info_ptr;
    int i;
    FILE *file = fopen(filename, "wb");

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
       return 1;

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
       png_destroy_write_struct(&png_ptr,
         (png_infopp)NULL);
       return 2;
    }

    png_init_io(png_ptr, file);

    if (setjmp(png_jmpbuf(png_ptr)))
    {
       png_destroy_write_struct(&png_ptr, &info_ptr);
       fclose(file);
       return 3;
    }

    png_set_IHDR(png_ptr, info_ptr, width, height,
                 8, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    if (setjmp(png_jmpbuf(png_ptr)))
        return 4;

    for (i = 0; i < height; i ++)
        png_write_row(png_ptr, &(data[i * width]));

    if (setjmp(png_jmpbuf(png_ptr)))
        return 5;

    png_write_end(png_ptr, NULL);

    fclose(file);

    return 0;
}

/*
 * Only works for 16bit 2 channel PCM data!
 */
int render_time(char *in_filename, char *out_filename, int samples)
{
    int skip = 70000;
    int take = 20000;
    int i = 0, j;
    int wtf;
    unsigned int bar;
    float minv = 0, maxv = 0;
    int bytes_read;
    size_t block_size = sizeof(char) * 2 * 2 * SAMPLES_PER_BLOCK_TIME;
    char *buffer = malloc(block_size);
    int width = min(samples / SAMPLES_PER_BLOCK_TIME, take);
    int height = max(SAMPLES_PER_BLOCK_TIME / 2 + 1, 256);
    unsigned char *data = malloc(sizeof(unsigned char) * width * height);
    FILE *infile = fopen(in_filename, "rb");

    memset(data, 0, sizeof(unsigned char) * width * height);

    bytes_read = fread(buffer, sizeof(char), block_size, infile);
    while(bytes_read == block_size && i < (skip + take))
    {
        for (j = 0; j < SAMPLES_PER_BLOCK_TIME; j ++)
        {
            if (i > skip && j == 0)
            {
                wtf = buffer[j * 2 * 2] + buffer[j * 2 * 2 + 1] * 256;
                bar = (unsigned char)((wtf / 256) + 128);
                data[bar * width + (i - skip)] = 255;
            }
        }

        bytes_read = fread(buffer, sizeof(char), block_size, infile);
        i ++;
    }

    fclose(infile);

    save_png(out_filename, data, width, height);
    free(data);
    free(buffer);

    //free(fft);
    //free(in);
    //free(out);

    return 0;
}

int render_freq(char *in_filename, char *out_filename, int samples)
{
    int i = 0, j;
    int wtf;
    float r1, r2;
    float minv = 0, maxv = 0;
    int bytes_read;
    size_t block_size = sizeof(char) * 2 * 2 * SAMPLES_PER_BLOCK_FREQ;
    char *buffer = malloc(block_size);
    int width = samples / SAMPLES_PER_BLOCK_FREQ;
    int height = SAMPLES_PER_BLOCK_FREQ / 2 + 1;
    unsigned char *data = malloc(sizeof(unsigned char) * width * height);
    FILE *infile = fopen(in_filename, "rb");
    float *in = malloc(sizeof(float) * SAMPLES_PER_BLOCK_FREQ);
    kiss_fft_cpx *out = malloc(sizeof(kiss_fft_cpx) * (SAMPLES_PER_BLOCK_FREQ / 2 + 1));
    kiss_fftr_cfg fft = kiss_fftr_alloc(SAMPLES_PER_BLOCK_FREQ, 0, 0, 0);

    memset(data, 0, sizeof(unsigned char) * width * height);

    bytes_read = fread(buffer, sizeof(char), block_size, infile);
    while(bytes_read == block_size)
    {
        for (j = 0; j < SAMPLES_PER_BLOCK_FREQ; j ++)
        {
            // Convert 16bit int to signed float
            wtf = buffer[j * 2 * 2] + buffer[j * 2 * 2 + 1] * 256;
            in[j] = (float)wtf / (65536 * 0.5f);
            // assert(in[j] >= -1 && in[j] <= 1);
        }
        kiss_fftr(fft, in, out);
        for (j = 0; j < height; j ++)
        {
            r1 = sqrtf(out[j].i * out[j].i + out[j].r * out[j].r);
            minv = min(minv, r1);
            maxv = max(maxv, r1);
            data[(height - j - 1) * width + i] = (unsigned char)(min(255, r1 * POWER_SCALE));
        }

        bytes_read = fread(buffer, sizeof(char), block_size, infile);
        i ++;
    }

    printf("\n%f2, %f2\n", minv, maxv);

    fclose(infile);

    save_png(out_filename, data, width, height);
    free(data);
    free(buffer);

    free(fft);
    free(in);
    free(out);

    return 0;
}

