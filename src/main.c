#include <stdio.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

int render_freq(char *in_filename, char *out_filename, int samples, int sampleRate);

int samples = 0;
int frames = 0;
FILE *outfile;

int decode_packet(AVCodecContext *codecContext, AVPacket *packet, AVFrame *frame)
{
    int got_frame;
    int consumed = 0;
    int data_size;

    while (packet->size > 0)
    {
        avcodec_get_frame_defaults(frame);
        consumed = avcodec_decode_audio4(codecContext, frame, &got_frame, packet);
        if (consumed <= 0)
            return -1;

        data_size = av_samples_get_buffer_size(NULL, codecContext->channels,  frame->nb_samples, codecContext->sample_fmt, 1);
        fwrite(frame->data[0], 1, data_size, outfile);

        if (got_frame)
        {
            frames ++;
            samples += frame->nb_samples;
            packet->size -= consumed;
            packet->data += consumed;
        }
    }

    return 0;
}

int dump_pcm(char *filename)
{
    int i, sampleRate;
    AVFormatContext *formatContext = avformat_alloc_context();
    AVCodecContext *codecContext;
    AVCodec *codec;
    AVFrame *frame;
    AVPacket packet;

    av_register_all();

    if (avformat_open_input(&formatContext, filename, NULL, NULL) < 0)
        return 0;
    if (avformat_find_stream_info(formatContext, NULL) < 0)
        return 0;

    av_dump_format(formatContext, 0, filename, 0);

    for (i = 0; i < formatContext->nb_streams; i ++)
    {
        if (formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
            break;
    }

    codecContext = formatContext->streams[i]->codec;
    codec = avcodec_find_decoder(codecContext->codec_id);

    if (avcodec_open(codecContext, codec) < 0)
        return 0;

    av_init_packet(&packet);
    frame = avcodec_alloc_frame();

    outfile = fopen("test.pcm", "wb");

    while (av_read_frame(formatContext, &packet) == 0)
    {
        if (packet.stream_index == i)
        {
            if (decode_packet(codecContext, &packet, frame) < 0)
            {
                printf("Errorz!\n");
                break;
            }
        }
    }

    fclose(outfile);

	sampleRate = codecContext->sample_rate;

    printf("Total frames : %d\n", frames);
    printf("Total samples: %d\n", samples);

    avcodec_close(codecContext);
    avformat_close_input(&formatContext);

    return sampleRate;
}

int main(int argc, char **argv)
{
    int sampleRate;
    int render_result;

    if (argc < 3)
        return 1;

    sampleRate = dump_pcm(argv[1]);
    if (sampleRate == 0)
        return 2;

    render_result = render_freq("test.pcm", argv[2], samples, sampleRate);
    if (render_result != 0)
        return render_result;

    return 0;
}
