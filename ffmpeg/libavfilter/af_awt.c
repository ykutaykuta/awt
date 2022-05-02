/*
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "audio.h"
#include "avfilter.h"
#include "internal.h"
#include "awt5_sdk.h"
#include "libavutil/opt.h"
#include <math.h>
#include "libavformat/avio.h"
#define AWT_DURATION 5
static int awt_is_numberic(char* str, int len) {
    unsigned i;

    for (i = 0; i < len; i++) {
        if (str[i] < 48 || str[i] > 57)
            return 0;
    }
    return 1;
}
static int awt_license(char** license, int decode) {
    char* license_request = NULL;
    char url[512];
    unsigned valid_for_sec = 0;
    unsigned licensed_duration_sec = 0;
    int ret = 0, len = 0;
    AVIOContext *in;

    // Generate license request
    license_request = awt5_license_request(AWT_DURATION);
    if (license_request == NULL){
    	av_log(NULL, AV_LOG_ERROR, "License init failed\n");
        return -1;
    }
    sprintf(url, "http://sonobytes.net/awtservice/thudo/client_thudo.php?method=%d&pswrd=512153634&licreq=%s", decode, license_request);
    free(license_request);
    ret = avio_open(&in, url, AVIO_FLAG_READ);
	if(ret){
		av_log(NULL, AV_LOG_ERROR, "Unable to load %s\n", url);
		return ret;
	}
	*license = malloc(128);
	while(1){
		ret = avio_read(in, (*license) + len, 2048);
		if(ret <= 0)
			break;
		len += ret;
	}
	avio_close(in);
    // check server response -- is it a license (numeric)?
    if (len <= 0 || awt_is_numberic(*license, len) != 1) {
    	av_log(NULL, AV_LOG_ERROR, "License generation failed\n");
        return -1;
    }
    (*license)[len] = 0;
    // checking license info and whether the license is valid
    ret = awt5_license_info(*license, decode, &valid_for_sec, &licensed_duration_sec);
    if (ret != 0 || licensed_duration_sec < AWT_DURATION) {
    	av_log(NULL, AV_LOG_ERROR, "License info failed: duration %d, valid %d\n", licensed_duration_sec, valid_for_sec);
        return ret;
    }
    av_log(NULL, AV_LOG_WARNING, "License info: duration %d, valid %d\n", licensed_duration_sec, valid_for_sec);
    return 0;
}

typedef struct AWTBuffer{
	char *data;
	int size;
	int max;
}AWTBuffer;
typedef struct AWTContext {
    const AVClass *class;
	PAWT5_DEC_OBJ decoder[2];
	int nb_decoder;
	unsigned algorithmic_delay;
	// Buffer
	int awt_size;
	int pkt_size;
	AWTBuffer **input;
	AWTBuffer *in, *out;
	AWTBuffer **output;
	// Common encoder+decoder settings
	int bottom_freq;
	int top_freq;
	int framesync_freq;
	int payload_frames;
	int crc_percent;
	// Encoding-only settings
	float aggressiveness;
	signed emboss_gain;
	signed emphasis_gain;
	// Decoding settings
	unsigned lookback_sec;
	int payload_length;
	// Config
	char *license;
	int preset;
	int counter;
} AWTContext;
static void awt_buffer_size(int *count){
	*count = (*count) * sizeof(float);
}
static void awt_buffer_ensure(AWTBuffer *buffer, int add){
	awt_buffer_size(&add);
	if(buffer->size + add <= buffer->max)
		return;
	buffer->max = FFMAX(buffer->max * 2, buffer->max + add);
	buffer->data = realloc(buffer->data, buffer->max);
}
static void awt_buffer_update(AWTBuffer *buffer, int count){
	awt_buffer_size(&count);
	if(count < buffer->size){
		memmove(buffer->data, buffer->data + count, buffer->size - count);
	}
	buffer->size = FFMAX(0, buffer->size - count);
}
static void awt_buffer_move_in(AWTBuffer *buffer, char *data, int off, int count){
	awt_buffer_ensure(buffer, count);
	awt_buffer_size(&off);
	awt_buffer_size(&count);
	memcpy(buffer->data + buffer->size, data + off, count);
	buffer->size += count;
}
static void awt_buffer_move_out(AWTBuffer *buffer, char *data, int off, int count){
	awt_buffer_size(&off);
	awt_buffer_size(&count);
	memcpy(data, buffer->data + off, count);
}
static void awt_buffer_create(AWTBuffer **ptr, int init_size){
	if(*ptr)
		return;
	*ptr = av_mallocz(sizeof(AWTBuffer));
	awt_buffer_ensure(*ptr, init_size);
}
static void awt_buffer_create_arr(AWTBuffer ***ptr, int count, int init_size){
	int i;
	if(*ptr)
		return;
	*ptr = av_mallocz(count*sizeof(AWTBuffer*));
	for(i = 0; i < count; i++){
		awt_buffer_create((*ptr) + i, init_size);
	}
}
static void awt_buffer_destroy(AWTBuffer **ptr){
	if(!*ptr)
		return;
	av_free((*ptr)->data);
	av_freep(ptr);
}
static void awt_buffer_destroy_arr(AWTBuffer ***ptr, int count){
	int i;
	if(!*ptr)
		return;
	for(i = 0; i < count; i++)
		awt_buffer_destroy((*ptr) + i);
	av_freep(ptr);
}
#define OFFSET(x) offsetof(AWTContext, x)
#define A AV_OPT_FLAG_AUDIO_PARAM
#define F AV_OPT_FLAG_FILTERING_PARAM
static const AVOption awt_options[] = {
	{ "bottom_freq", "bottom watermark carrier frequency, Hz (e.g. 1750)",
    		OFFSET(bottom_freq), AV_OPT_TYPE_INT, { .i64 = 1750 }, INT_MIN, INT_MAX, .flags = A|F },
    { "top_freq", "top watermark carrier frequency, Hz (e.g. 11000)",
    		OFFSET(top_freq), AV_OPT_TYPE_INT, { .i64 = 11000 }, INT_MIN, INT_MAX, .flags = A|F },
    { "framesync_freq", "frame syncronization marker frequency, Hz (e.g. 2500)",
    		OFFSET(framesync_freq), AV_OPT_TYPE_INT, { .i64 = 2500 }, INT_MIN, INT_MAX, .flags = A|F },
    { "payload_frames", "number of frames used to carry the watermark (e.g. 1, 2)",
    		OFFSET(payload_frames), AV_OPT_TYPE_INT, { .i64 = 1 }, INT_MIN, INT_MAX, .flags = A|F },
    { "crc_percent", "checksum size relative to the watermark length, in % (e.g. 20)",
    		OFFSET(crc_percent), AV_OPT_TYPE_INT, { .i64 = 20 }, INT_MIN, INT_MAX, .flags = A|F },
    { "aggressiveness", "encoding aggressiveness, higher = more robust & more audible, lower = less robust & more transparent (e.g. 0.75)",
    		OFFSET(aggressiveness), AV_OPT_TYPE_FLOAT, { .dbl = 0.75f }, INT_MIN, INT_MAX, .flags = A|F },
    { "emboss_gain", "emboss gain, dB (e.g. -40)",
    		OFFSET(emboss_gain), AV_OPT_TYPE_INT, { .i64 = -40 }, INT_MIN, INT_MAX, .flags = A|F },
    { "emphasis_gain", "emphasis gain, dB (e.g. +6)",
    		OFFSET(emphasis_gain), AV_OPT_TYPE_INT, { .i64 = 6 }, INT_MIN, INT_MAX, .flags = A|F },
    { "lookback_sec", "loopback second",
    		OFFSET(lookback_sec), AV_OPT_TYPE_INT, { .i64 = 5 }, INT_MIN, INT_MAX, .flags = A|F },
	{ "wm_size", "payload size in bytes",
			OFFSET(payload_length), AV_OPT_TYPE_INT, { .i64 = 0 }, 0, 4, .flags = A|F },
	{ "preset", "set preset", OFFSET(preset), AV_OPT_TYPE_INT, { .i64 = -1 }, -1, INT_MAX, A|F, "preset" },
		{ "0", "Unused", 0, AV_OPT_TYPE_CONST, { .i64 = 0 }, 0, 0, A|F, "preset" },
		{ "1", "recommended for 1 bytes long payload, using 1 data frames", 0, AV_OPT_TYPE_CONST, { .i64 = 1 }, 0, 0, A|F, "preset" },
		{ "2", "recommended for 2 bytes long payload, using 1 data frames", 0, AV_OPT_TYPE_CONST, { .i64 = 2 }, 0, 10, A|F, "preset" },
		{ "3", "recommended for 2 bytes long payload, using 2 data frames", 0, AV_OPT_TYPE_CONST, { .i64 = 3 }, 0, 10, A|F, "preset" },
		{ "4", "recommended for 3 bytes long payload, using 1 data frames", 0, AV_OPT_TYPE_CONST, { .i64 = 4 }, 0, 10, A|F, "preset" },
		{ "5", "recommended for 3 bytes long payload, using 2 data frames", 0, AV_OPT_TYPE_CONST, { .i64 = 5 }, 0, 10, A|F, "preset" },
		{ "6", "recommended for 4 bytes long payload, using 2 data frames", 0, AV_OPT_TYPE_CONST, { .i64 = 6 }, 0, 10, A|F, "preset" },
		{ "7", "recommended for 4 bytes long payload, using 3 data frames", 0, AV_OPT_TYPE_CONST, { .i64 = 7 }, 0, 10, A|F, "preset" },
		{ "8", "recommended for 6 bytes long payload, using 3 data frames", 0, AV_OPT_TYPE_CONST, { .i64 = 8 }, 0, 10, A|F, "preset" },
		{ "9", "recommended for 6 bytes long payload, using 4 data frames", 0, AV_OPT_TYPE_CONST, { .i64 = 9 }, 0, 10, A|F, "preset" },
		{ "10","recommended for 8 bytes long payload, using 4 data frames", 0, AV_OPT_TYPE_CONST, { .i64 = 10 }, 0, 10, A|F, "preset" },
    { NULL }
};

AVFILTER_DEFINE_CLASS(awt);

static int awt_filter_frame(AVFilterLink *inlink, AVFrame *in)
{
	AVFilterContext *ctx = inlink->dst;
	AVFilterLink *outlink = ctx->outputs[0];
    AWTContext *awt       = ctx->priv;
    int ret = 0, from, to;
    int channels = inlink->channels;
    int sample_size = sizeof(float);
    AVFrame *out;
    int i;
	float percent;
	char found[32];
    for(i = 0; i < channels; i++){
    	awt_buffer_move_in(awt->input[i], in->extended_data[i], 0, in->nb_samples);
	}
    from = 0;
    to = awt->input[0]->size / sample_size;
    if(to < awt->awt_size)
    	goto end;
    while(to - from >= awt->awt_size){
		awt->in->size = 0;
    	for(i = 0; i < channels; i++){
    		awt_buffer_move_in(awt->in, awt->input[i]->data, from, awt->awt_size);
    	}
		for(i = 0; i < awt->nb_decoder; i++){
			ret = awt5_decode_stream_buffer(awt->decoder[i], awt->in->data + i*awt->awt_size*sizeof(float),
				found, &percent);
			if (ret) {
				av_log(ctx, AV_LOG_ERROR, "AWT5 Decoder error %d occurred.\n", ret);
				awt5_print_err_text(ret);
				goto end;
			}
			if(*found){
				av_log(NULL, AV_LOG_ERROR, "Found watermark %d 0x%s %.2f\n", i, found, percent);
				awt->counter++;
			}
		}
		for(i = 0; i < channels; i++){
    		awt_buffer_move_in(awt->output[i], awt->out->data, i*awt->awt_size, awt->awt_size);
    	}
    	from += awt->awt_size;
    }
    for(i = 0; i < channels; i++){
    	awt_buffer_update(awt->input[i], from);
	}
    from = 0;
    to = awt->output[0]->size / sample_size;
    if(to < awt->pkt_size)
    	goto end;
    while(to - from >= awt->pkt_size){
    	out = ff_get_audio_buffer(outlink, awt->pkt_size);
		if(!out){
			ret = AVERROR(ENOMEM);
			goto end;
		}
    	av_frame_copy_props(out, in);
    	out->format         = in->format;
    	out->channels       = in->channels;
    	out->channel_layout = in->channel_layout;
    	out->sample_rate    = in->sample_rate;
		for(i = 0; i < channels; i++){
    		awt_buffer_move_out(awt->output[i], out->extended_data[i], from, awt->pkt_size);
		}
        ret = ff_filter_frame(outlink, out);
        if(ret)
        	goto end;
        from += awt->pkt_size;
    }
    for(i = 0; i < channels; i++){
    	awt_buffer_update(awt->output[i], from);
	}
end:
    av_frame_free(&in);
    return ret;
}
static int awt_config_props(AVFilterLink *inlink){
    AVFilterContext *ctx = inlink->dst;
    AWTContext *awt = ctx->priv;
    int i, ret;
	if(awt->preset){
		if(awt->preset == -1){
			int wm_len = awt->payload_length;
			switch(wm_len){
			case 1:
				awt->preset = 1;
				break;
			case 2:
				awt->preset = 2;
				break;
			case 3:
				awt->preset = 4;
				break;
			case 4:
				awt->preset = 6;
				break;
			case 6:
				awt->preset = 8;
				break;
			case 8:
				awt->preset = 10;
				break;
			default:
				break;
			}
		}
		if(awt->preset == -1 || awt->preset > 10){
			av_log(awt, AV_LOG_ERROR, "Invalid preset %d\n", awt->preset);
			return AVERROR(EINVAL);
		}
		av_log(awt, AV_LOG_WARNING, "Use preset %d\n", awt->preset);
		switch(awt->preset){
		case 1: //(recommended for 1 bytes long payload, using 1 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 5000;
			awt->framesync_freq = 2500;
			awt->payload_frames = 1;
			awt->crc_percent = 20;
			awt->aggressiveness = 0.75;
			awt->emboss_gain = -40;
			awt->emphasis_gain = 6;
			break;
		case 2: //(recommended for 2 bytes long payload, using 1 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 7500;
			awt->framesync_freq = 2500;
			awt->payload_frames = 1;
			awt->crc_percent = 20;
			awt->aggressiveness = 0.75;
			awt->emboss_gain = -40;
			awt->emphasis_gain = 6;
			break;
		case 3: //(recommended for 2 bytes long payload, using 2 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 5250;
			awt->framesync_freq = 2500;
			awt->payload_frames = 2;
			awt->crc_percent = 20;
			awt->aggressiveness = 0.75;
			awt->emboss_gain = -40;
			awt->emphasis_gain = 6;
			break;
		case 4: //(recommended for 3 bytes long payload, using 1 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 11000;
			awt->framesync_freq = 2500;
			awt->payload_frames = 1;
			awt->crc_percent = 20;
			awt->aggressiveness = 0.75;
			awt->emboss_gain = -40;
			awt->emphasis_gain = 6;
			break;
		case 5: //(recommended for 3 bytes long payload, using 2 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 7000;
			awt->framesync_freq = 2500;
			awt->payload_frames = 2;
			awt->crc_percent = 20;
			awt->aggressiveness = 0.75;
			awt->emboss_gain = -40;
			awt->emphasis_gain = 6;
		    break;
		case 6: //(recommended for 4 bytes long payload, using 2 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 7900;
			awt->framesync_freq = 2500;
			awt->payload_frames = 2;
			awt->crc_percent = 20;
			awt->aggressiveness = 0.75;
			awt->emboss_gain = -40;
			awt->emphasis_gain = 6;
		    break;
		case 7: //(recommended for 4 bytes long payload, using 3 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 6750;
			awt->framesync_freq = 2500;
			awt->payload_frames = 3;
			awt->crc_percent = 20;
			awt->aggressiveness = 0.75;
			awt->emboss_gain = -40;
			awt->emphasis_gain = 6;
			break;
		case 8: //(recommended for 6 bytes long payload, using 3 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 9000;
			awt->framesync_freq = 2500;
			awt->payload_frames = 3;
			awt->crc_percent = 20;
			awt->aggressiveness = 0.75;
			awt->emboss_gain = -40;
			awt->emphasis_gain = 6;
		    break;
		case 9: //(recommended for 6 bytes long payload, using 4 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 7500;
			awt->framesync_freq = 2500;
			awt->payload_frames = 4;
			awt->crc_percent = 20;
			awt->aggressiveness = 0.75;
			awt->emboss_gain = -40;
			awt->emphasis_gain = 6;
			break;
		case 10: //(recommended for 8 bytes long payload, using 4 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 9000;
			awt->framesync_freq = 2500;
			awt->payload_frames = 4;
			awt->crc_percent = 20;
			awt->aggressiveness = 0.75;
			awt->emboss_gain = -40;
			awt->emphasis_gain = 6;
		    break;
		}
	}
	av_log(awt, AV_LOG_INFO, "Configure: "
			"\n  bottom_freq: %d"
			"\n  top_freq: %d"
			"\n  framesync_freq: %d"
			"\n  payload_frames: %d"
			"\n  crc_percent: %d"
			"\n  aggressiveness: %.2f"
			"\n  emboss_gain: %d"
			"\n  emphasis_gain: %d"
			"\n  payload size: %d"
			"\n  packet size: %d\n",
			awt->bottom_freq,
			awt->top_freq,
			awt->framesync_freq,
			awt->payload_frames,
			awt->crc_percent,
			awt->aggressiveness,
			awt->emboss_gain,
			awt->emphasis_gain,
			awt->awt_size,
			awt->pkt_size);

    ret = awt_license(&awt->license, 1);
    if(ret)
    	return ret;
	awt->nb_decoder = FFMIN(2, inlink->channels);
	for(i = 0; i < awt->nb_decoder; i++){
		ret = awt5_decode_stream_init(&awt->decoder[i], &awt->awt_size, awt->payload_length,
			inlink->sample_rate, awt->bottom_freq, awt->top_freq, awt->framesync_freq,
			awt->payload_frames, awt->crc_percent, awt->lookback_sec, awt->license);
		if (ret) {
			av_log(ctx, AV_LOG_ERROR, "Could not initialize AWT5 Decoder object, error %d occurred.\n", ret);
			awt5_print_err_text(ret);
			return AVERROR(EINVAL);
		}
	}
    av_log(ctx, AV_LOG_WARNING, "AWT5 started with %d frame(s)\n", awt->awt_size);
	awt->pkt_size = FFMIN(2048, FFMAX(2048 / awt->awt_size, 1)*awt->awt_size);
	awt_buffer_create_arr(&awt->input, inlink->channels, awt->pkt_size);
	awt_buffer_create_arr(&awt->output, inlink->channels, awt->pkt_size);
	awt_buffer_create(&awt->in, awt->awt_size*inlink->channels);
	awt_buffer_create(&awt->out, awt->in->max);
	return 0;
}
static const AVFilterPad awt_inputs[] = {
    {
        .name         = "default",
        .type         = AVMEDIA_TYPE_AUDIO,
        .filter_frame = awt_filter_frame,
		.config_props = awt_config_props
    },
    { NULL }
};

static const AVFilterPad awt_outputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_AUDIO
    },
    { NULL }
};
static av_cold int awt_init(AVFilterContext *ctx)
{
	char *sn;
	awt5_sn(&sn);
	av_log(ctx, AV_LOG_INFO, "Displaying AWT5 product Serial Number: %s\n", sn);
	free(sn);
	return 0;
}
static av_cold void awt_uninit(AVFilterContext *ctx)
{
	AWTContext *awt = ctx->priv;
	if(awt->nb_decoder){
		int i;
		for(i = 0; i < awt->nb_decoder; i++){
			awt5_decode_stream_kill(&awt->decoder[i]);
		}
		awt->nb_decoder = 0;
	}
	awt_buffer_destroy(&awt->in);
	awt_buffer_destroy(&awt->out);
	awt_buffer_destroy_arr(&awt->input, ctx->inputs[0]->channels);
	awt_buffer_destroy_arr(&awt->output, ctx->inputs[0]->channels);
	av_freep(&awt->license);
	av_log(NULL, AV_LOG_WARNING, "Found %d watermark\n", awt->counter);
}
AVFilter ff_af_awt = {
    .name          = "awt",
    .description   = NULL_IF_CONFIG_SMALL("Add watermark for audio"),
    .priv_size     = sizeof(AWTContext),
    .priv_class     = &awt_class,
    .inputs        = awt_inputs,
    .outputs       = awt_outputs,
    FILTER_SAMPLEFMTS(AV_SAMPLE_FMT_FLTP),
	.init          = awt_init,
	.uninit        = awt_uninit
};
