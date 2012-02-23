#include <string.h>
#include <assert.h>
#include "kissFFT/kiss_fftr.h"
#include "png.h"

const int SAMPLES_PER_BLOCK_FREQ = 2048;
const int SAMPLES_STEP = 512;
const float PI = 3.14159265f;

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

int render_freq(char *in_filename, char *out_filename, int samples)
{
    int i = 0, j;
    int wtf;
    float cumulative_average = 0;
    float block_average = 0;
    float result;
    int bytes_read;
    size_t block_size = sizeof(char) * 2 * 2 * SAMPLES_PER_BLOCK_FREQ;
    char *buffer = malloc(block_size);
    int width = samples / SAMPLES_STEP;
    int height = SAMPLES_PER_BLOCK_FREQ / 2 + 1;
    float *float_data = malloc(sizeof(float) * width * height);
    unsigned char *data = malloc(sizeof(unsigned char) * width * height);
    FILE *infile = fopen(in_filename, "rb");
    float *in = malloc(sizeof(float) * SAMPLES_PER_BLOCK_FREQ);
    kiss_fft_cpx *out = malloc(sizeof(kiss_fft_cpx) * (SAMPLES_PER_BLOCK_FREQ / 2 + 1));
    kiss_fftr_cfg fft = kiss_fftr_alloc(SAMPLES_PER_BLOCK_FREQ, 0, 0, 0);
    float brightness_scale;

    memset(data, 0, sizeof(unsigned char) * width * height);

    bytes_read = fread(buffer, sizeof(char), block_size, infile);
    while(bytes_read == block_size)
    {
        for (j = 0; j < SAMPLES_PER_BLOCK_FREQ; j ++)
        {
            // Convert 16bit int to signed float
            wtf = buffer[j * 2 * 2] + buffer[j * 2 * 2 + 1] * 256;
            in[j] = (float)wtf / (65536 * 0.5f);
            // Use a Hann window to reduce noise
            in[j] *= (1 - cosf(2 * PI * j / SAMPLES_PER_BLOCK_FREQ)) * 0.5f;
        }
        kiss_fftr(fft, in, out);

        block_average = 0;
        for (j = 0; j < height; j ++)
        {
            result = sqrtf(out[j].i * out[j].i + out[j].r * out[j].r);
            block_average += result;
            float_data[(height - j - 1) * width + i] = result;
        }

        block_average /= height;
        cumulative_average = (block_average + i * cumulative_average) / (i + 1);

        fseek(infile, sizeof(char) * -2 * 2 * (SAMPLES_PER_BLOCK_FREQ - SAMPLES_STEP), SEEK_CUR);
        bytes_read = fread(buffer, sizeof(char), block_size, infile);
        i ++;
    }

    brightness_scale = 20.f / cumulative_average;
    printf("\naver : %.3f\nscale: %.3f\n", cumulative_average, brightness_scale);
    for (i = 0; i < width * height; i ++)
        data[i] = (unsigned char)min(255, float_data[i] * brightness_scale);

    fclose(infile);

    save_png(out_filename, data, width, height);
    free(float_data);
    free(data);
    free(buffer);

    free(fft);
    free(in);
    free(out);

    return 0;
}

