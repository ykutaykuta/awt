#include "awt.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <awt5_sdk.h>

#ifndef sml
#define sml(...) av_log(NULL,AV_LOG_ERROR,__VA_ARGS__)
#endif
#define AWT_MAX_DECODER 2
#define LOG_MSG_LEN 1024
typedef struct AWTContext {
	PAWT5_DEC_OBJ decoder[AWT_MAX_DECODER];
	int nb_decoder;
	char *buffer[AWT_MAX_DECODER];
	int nb_buffer;
	// Buffer
	int awt_size;
	// Common encoder+decoder settings
	int bottom_freq;
	int top_freq;
	int framesync_freq;
	int payload_frames;
	int crc_percent;
	// Decoding settings
	unsigned lookback_sec;
	int payload_length;
	// Config
	char *license;
	int preset;
	int sample_rate;
	int running;
	int64_t pre_duration, current_duration;
    AVFilterGraph *fg;
    AVFilterContext *fcs;
    AVFilterContext *fcd;
    pthread_mutex_t lock;
    float speed;
    int64_t second;
    int (*callback)(int channel, const char *wm, float percent, float pos_at);
	void (*log_callback)(const char *level, const char *msg, const int ff_ret);
    int counter;
    int license_duration, license_valid;
	char log_msg[LOG_MSG_LEN];
} AWTContext;

#define AWT_DURATION 5

static int awt_is_numberic(char* str, int len) {
    unsigned i;

    for (i = 0; i < len; i++) {
        if (str[i] < 48 || str[i] > 57)
            return 0;
    }
    return 1;
}
static int awt_license(AWTContext *awt, char** license, int decode) {
    char* license_request = NULL;
    char url[512];
    unsigned valid_for_sec = 0;
    unsigned licensed_duration_sec = 0;
    int ret = 0, len = 0;
    AVIOContext *in;

    // Generate license request
    license_request = awt5_license_request(AWT_DURATION);
    if (license_request == NULL){
    	snprintf(awt->log_msg, LOG_MSG_LEN, "License init failed\n");
    	awt->log_callback("ERROR", awt->log_msg, ret);
        return -1;
    }
    sprintf(url, "http://sonobytes.net/awtservice/thudo/client_thudo.php?method=%d&pswrd=512153634&licreq=%s", decode, license_request);
    free(license_request);
    ret = avio_open(&in, url, AVIO_FLAG_READ);
	if(ret){
		snprintf(awt->log_msg, LOG_MSG_LEN, "Unable to load %s\n", url);
		awt->log_callback("ERROR", awt->log_msg, ret);
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
    	snprintf(awt->log_msg, LOG_MSG_LEN, "License generation failed\n");
    	awt->log_callback("ERROR", awt->log_msg, ret);
        return -1;
    }
    (*license)[len] = 0;
    // checking license info and whether the license is valid
    ret = awt5_license_info(*license, decode, &valid_for_sec, &licensed_duration_sec);
    if (ret != 0 || licensed_duration_sec < AWT_DURATION) {
    	snprintf(awt->log_msg, LOG_MSG_LEN, "License info failed: duration %d, valid %d\n", licensed_duration_sec, valid_for_sec);
    	awt->log_callback("ERROR", awt->log_msg, ret);
        return ret;
    }
    awt->license_duration = licensed_duration_sec;
    awt->license_valid = valid_for_sec;
    return 0;
}
static int awt_setup(AWTContext *awt, int channels){
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
			snprintf(awt->log_msg, LOG_MSG_LEN, "Invalid preset %d\n", awt->preset);
			awt->log_callback("ERROR", awt->log_msg, ret);
			return AVERROR(EINVAL);
		}
		switch(awt->preset){
		case 1: //(recommended for 1 bytes long payload, using 1 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 5000;
			awt->framesync_freq = 2500;
			awt->payload_frames = 1;
			awt->crc_percent = 20;
			break;
		case 2: //(recommended for 2 bytes long payload, using 1 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 7500;
			awt->framesync_freq = 2500;
			awt->payload_frames = 1;
			awt->crc_percent = 20;
			break;
		case 3: //(recommended for 2 bytes long payload, using 2 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 5250;
			awt->framesync_freq = 2500;
			awt->payload_frames = 2;
			awt->crc_percent = 20;
			break;
		case 4: //(recommended for 3 bytes long payload, using 1 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 11000;
			awt->framesync_freq = 2500;
			awt->payload_frames = 1;
			awt->crc_percent = 20;
			break;
		case 5: //(recommended for 3 bytes long payload, using 2 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 7000;
			awt->framesync_freq = 2500;
			awt->payload_frames = 2;
			awt->crc_percent = 20;
		    break;
		case 6: //(recommended for 4 bytes long payload, using 2 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 7900;
			awt->framesync_freq = 2500;
			awt->payload_frames = 2;
			awt->crc_percent = 20;
		    break;
		case 7: //(recommended for 4 bytes long payload, using 3 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 6750;
			awt->framesync_freq = 2500;
			awt->payload_frames = 3;
			awt->crc_percent = 20;
			break;
		case 8: //(recommended for 6 bytes long payload, using 3 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 9000;
			awt->framesync_freq = 2500;
			awt->payload_frames = 3;
			awt->crc_percent = 20;
		    break;
		case 9: //(recommended for 6 bytes long payload, using 4 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 7500;
			awt->framesync_freq = 2500;
			awt->payload_frames = 4;
			awt->crc_percent = 20;
			break;
		case 10: //(recommended for 8 bytes long payload, using 4 data frames):
			awt->bottom_freq = 1750;
			awt->top_freq = 9000;
			awt->framesync_freq = 2500;
			awt->payload_frames = 4;
			awt->crc_percent = 20;
		    break;
		}
	}

    ret = awt_license(awt, &awt->license, 1);
    if(ret)
    	return ret;
	snprintf(awt->log_msg, LOG_MSG_LEN, "Configure: "
			"\n  license        : %d/%d"
			"\n  preset         : %d"
			"\n  bottom_freq    : %d"
			"\n  top_freq       : %d"
			"\n  framesync_freq : %d"
			"\n  payload_frames : %d"
			"\n  crc_percent    : %d"
			"\n  payload size   : %d\n",
			awt->license_duration, awt->license_valid,
			awt->preset,
			awt->bottom_freq,
			awt->top_freq,
			awt->framesync_freq,
			awt->payload_frames,
			awt->crc_percent,
			awt->payload_length);
	awt->log_callback("INFO", awt->log_msg, ret);
	awt->nb_decoder = FFMIN(2, channels);
	for(i = 0; i < awt->nb_decoder; i++){
		ret = awt5_decode_stream_init(&awt->decoder[i], &awt->awt_size, awt->payload_length,
			awt->sample_rate, awt->bottom_freq, awt->top_freq, awt->framesync_freq,
			awt->payload_frames, awt->crc_percent, awt->lookback_sec, awt->license);
		if (ret) {
			snprintf(awt->log_msg, LOG_MSG_LEN, "Could not initialize AWT5 Decoder object, error %d occurred.\n", ret);
			awt->log_callback("ERROR", awt->log_msg, ret);
			awt5_print_err_text(ret);
			return AVERROR(EINVAL);
		}
		awt->buffer[i] = malloc(awt->awt_size*sizeof(float));
	}
	return 0;
}

static int awt_filter_frame(AWTContext *awt, AVFrame *in)
{
    int channels = awt->nb_decoder;
    int i, off = 0, count, ret = 0;
    float *src, *dst;
	float percent;
	char found[32];
	while(off < in->nb_samples){
		count = FFMIN(awt->awt_size - awt->nb_buffer, in->nb_samples - off);
		for(i = 0; i < channels; i++){
			src = in->extended_data[i];
			dst = awt->buffer[i];
			memcpy(dst + awt->nb_buffer, src + off, count*sizeof(float));
		}
		off += count;
		awt->nb_buffer += count;
		if(awt->nb_buffer < awt->awt_size)
			break;
		awt->nb_buffer = 0;
		for(i = 0; i < channels; i++){
			ret = awt5_decode_stream_buffer(awt->decoder[i], awt->buffer[i], found, &percent);
			if (ret) {
				snprintf(awt->log_msg, LOG_MSG_LEN, "AWT5 Decoder error %d occurred.\n", ret);
				awt->log_callback("ERROR", awt->log_msg, ret);
				awt5_print_err_text(ret);
				goto end;
			}
			if(*found){
				awt->counter++;
				if(awt->callback(i, found, percent, (float)awt->current_duration/AV_TIME_BASE)){
					ret = AVERROR_EOF;
					goto end;
				}
			}
		}
	}
end:
    return ret;
}

static int awt_init_filters(AWTContext *awt, const char *filters_descr, int format,
	int channels, AVCodecContext *decoder, AVStream *st)
{
    char args[512];
    int ret = 0;
    const AVFilter *abuffersrc  = avfilter_get_by_name("abuffer");
    const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;
    enum AVSampleFormat out_sample_fmts[] = { format, -1 };
    int64_t out_channel_layouts[] = { av_get_default_channel_layout(channels), -1 };
    int out_sample_rates[] = { awt->sample_rate, -1 };
    AVRational time_base = st->time_base;

    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* buffer audio source: the decoded frames from the decoder will be inserted here. */
    if (!decoder->channel_layout)
    	decoder->channel_layout = av_get_default_channel_layout(decoder->channels);
    snprintf(args, sizeof(args),
            "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
             time_base.num, time_base.den, decoder->sample_rate,
             av_get_sample_fmt_name(decoder->sample_fmt), decoder->channel_layout);
    ret = avfilter_graph_create_filter(&buffersrc_ctx, abuffersrc, "in",
                                       args, NULL, filter_graph);
    if (ret < 0) {
        snprintf(awt->log_msg, LOG_MSG_LEN, "Cannot create audio buffer source\n");
        awt->log_callback("ERROR", awt->log_msg, ret);
        goto end;
    }

    /* buffer audio sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx, abuffersink, "out",
                                       NULL, NULL, filter_graph);
    if (ret < 0) {
        snprintf(awt->log_msg, LOG_MSG_LEN, "Cannot create audio buffer sink\n");
        awt->log_callback("ERROR", awt->log_msg, ret);
        goto end;
    }

    ret = av_opt_set_int_list(buffersink_ctx, "sample_fmts", out_sample_fmts, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        snprintf(awt->log_msg, LOG_MSG_LEN, "Cannot set output sample format\n");
        awt->log_callback("ERROR", awt->log_msg, ret);
        goto end;
    }

    ret = av_opt_set_int_list(buffersink_ctx, "channel_layouts", out_channel_layouts, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        snprintf(awt->log_msg, LOG_MSG_LEN, "Cannot set output channel layout\n");
        awt->log_callback("ERROR", awt->log_msg, ret);
        goto end;
    }

    ret = av_opt_set_int_list(buffersink_ctx, "sample_rates", out_sample_rates, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        snprintf(awt->log_msg, LOG_MSG_LEN, "Cannot set output sample rate\n");
        awt->log_callback("ERROR", awt->log_msg, ret);
        goto end;
    }

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
                                        &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

    /* Print summary of the sink buffer
     * Note: args buffer is reused to store channel layout string */
    awt->fg = filter_graph;
    awt->fcs = buffersrc_ctx;
    awt->fcd = buffersink_ctx;
end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    if(ret)
    	avfilter_graph_free(&filter_graph);
    return ret;
}

static int awt_interupt(void *arg){
	AWTContext *awt = arg;
	return !awt->running;
}
static int awt_open_input(AWTContext *awt, AVFormatContext **fmt_ctx, AVCodecContext **dec_ctx,
		AVStream **st, const char *filename, AVDictionary **opts)
{
    int ret;
    AVCodec *dec;

	*fmt_ctx = avformat_alloc_context();
	(*fmt_ctx)->interrupt_callback = (AVIOInterruptCB) { awt_interupt, awt };

    if ((ret = avformat_open_input(fmt_ctx, filename, NULL, opts)) < 0)
    {
        snprintf(awt->log_msg, LOG_MSG_LEN, "Cannot open input file '%s'\n", filename);
        awt->log_callback("ERROR", awt->log_msg, ret);
        return ret;
    }

    if ((ret = avformat_find_stream_info(*fmt_ctx, NULL)) < 0)
    {
        snprintf(awt->log_msg, LOG_MSG_LEN, "Cannot find stream information\n");
        awt->log_callback("ERROR", awt->log_msg, ret);
        return ret;
    }

    /* select the stream */
    ret = av_find_best_stream(*fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &dec, 0);
    if (ret < 0)
    {
        snprintf(awt->log_msg, LOG_MSG_LEN, "Cannot find stream in the input file '%s'\n", filename);
        awt->log_callback("ERROR", awt->log_msg, ret);
        return ret;
    }

    *st = (*fmt_ctx)->streams[ret];
    /* create decoding context */
    *dec_ctx = avcodec_alloc_context3(dec);
    if (*dec_ctx == NULL)
        return AVERROR(ENOMEM);
    avcodec_parameters_to_context(*dec_ctx, (*st)->codecpar);

    /* init the audio decoder */
    if ((ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0)
    {
        snprintf(awt->log_msg, LOG_MSG_LEN, "Cannot open decoder\n");
        awt->log_callback("ERROR", awt->log_msg, ret);
        return ret;
    }

    return 0;
}
static int awt_open_internal(void **awt_ptr, int sample_rate, int payload_length, float speed, int second,
	int (*callback)(int channel, const char *wm, float percent, float pos_at),
	void (*log_callback)(const char *level, const char *msg, const int ff_ret)){
	AWTContext *awt = *awt_ptr = av_mallocz(sizeof(AWTContext));
	awt->callback = callback;
	awt->log_callback = log_callback;
	awt->payload_length = FFMAX(1, FFMIN(4, payload_length));
	awt->sample_rate = sample_rate;
	awt->lookback_sec = 5;
	awt->speed = FFMAX(0.05, FFMIN(1000, speed));
	awt->second = second < 1 ? INT64_MAX : (second * 1000000);
	awt->running = 1;
	return 0;
}
int awt_open(void **awt_ptr,
		int sample_rate,
		int preset,
		int payload_length,
		float speed,
		int second,
		int (*callback)(int channel, const char *wm, float percent, float pos_at),
		void (*log_callback)(const char *level, const char *msg, const int ff_ret)){
	AWTContext *awt;
	awt_open_internal(awt_ptr, sample_rate, payload_length, speed, second, callback, log_callback);
	awt = *awt_ptr;
	awt->preset = preset != -1 ? FFMAX(1, FFMIN(10, preset)) : -1;
	return 0;
}
int awt_open2(void **awt_ptr,
		int sample_rate,
		int bottom_freq,
		int top_freq,
		int framesync_freq,
		int payload_frames,
		int crc_percent,
		int payload_length,
		float speed,
		int second,
		int (*callback)(int channel, const char *wm, float percent, float pos_at),
		void (*log_callback)(const char *level, const char *msg, const int ff_ret)){
	AWTContext *awt;
	awt_open_internal(awt_ptr, sample_rate, payload_length, speed, second, callback, log_callback);
	awt = *awt_ptr;
	awt->bottom_freq = bottom_freq;
	awt->top_freq = top_freq;
	awt->framesync_freq = framesync_freq;
	awt->payload_frames = payload_frames;
	awt->crc_percent = crc_percent;
	return 0;
}
int awt_close(void **awt_ptr){
	AWTContext *awt = *awt_ptr;
	if(!awt)
		return 0;
	awt->running = 0;
	pthread_mutex_lock(&awt->lock);
	if(awt->nb_decoder){
		int i;
		for(i = 0; i < awt->nb_decoder; i++){
			awt5_decode_stream_kill(&awt->decoder[i]);
			av_free(awt->buffer[i]);
		}
		awt->nb_decoder = 0;
	}
	av_freep(&awt->license);
	avfilter_graph_free(&awt->fg);
	pthread_mutex_unlock(&awt->lock);
	free(awt);
	*awt_ptr = NULL;
	return 0;
}
static int awt_run(void **awt_ptr, const char *file_path){
	int ret;
	AWTContext *awt = *awt_ptr;
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *decoder = NULL;
	AVPacket *packet = av_packet_alloc();
	AVFrame *frame = av_frame_alloc();
	AVFrame *filt_frame = av_frame_alloc();
	AVStream *st;
	int64_t start_time = av_gettime(), start_pts = AV_NOPTS_VALUE, realtime, pts, delta;
	if (!packet || !frame || !filt_frame) {
		ret = AWT_CODE_SETUP_ERROR;
		goto end;
	}

	awt->counter = 0;
	if ((ret = awt_open_input(awt, &fmt_ctx, &decoder, &st, file_path, NULL)) < 0){
		ret = AWT_CODE_OPEN_ERROR;
		goto end;
	}
	if(awt->sample_rate < 1)
		awt->sample_rate = st->codecpar->sample_rate;
	if(!awt->fg && (ret = awt_init_filters(awt, "anull", AV_SAMPLE_FMT_FLTP, st->codecpar->channels, decoder, st))){
		ret = AWT_CODE_SETUP_ERROR;
		goto end;
	}
	if(!awt->nb_decoder && (ret = awt_setup(awt, st->codecpar->channels)) < 0){
		ret = AWT_CODE_SETUP_ERROR;
		goto end;
	}

	snprintf(awt->log_msg, LOG_MSG_LEN,
		"Start:\n"
		"  input    : %s\n"
		"  start    : %.1f\n"
		"  duration : %.1f\n"
		"  codec    : %s\n"
		"  channel  : %d\n"
		"  rate     : %d -> %d\n"
		"  block    : %d\n",
		file_path,
		fmt_ctx->start_time == AV_NOPTS_VALUE ? 0:  fmt_ctx->start_time/1000000.0,
		fmt_ctx->duration/1000000.0,
		decoder->codec->name,
		st->codecpar->channels,
		st->codecpar->sample_rate,
		awt->sample_rate,
		awt->awt_size);
	awt->log_callback("INFO", awt->log_msg, ret);
	while (awt->running) {
		if ((ret = av_read_frame(fmt_ctx, packet)) < 0){
			goto out;
		}
		if (packet->stream_index == st->index) {
			pts = packet->pts;
			if(pts != AV_NOPTS_VALUE){
				pts = av_rescale_q(pts, st->time_base, AV_TIME_BASE_Q);
				if(start_pts == AV_NOPTS_VALUE)
					start_pts = pts;
				awt->current_duration = pts - start_pts;
				realtime = av_gettime() - start_time;
				delta = awt->current_duration/awt->speed - realtime;
				if(awt->current_duration + awt->pre_duration >= awt->second){
					goto out;
				}
				if(delta > 0){
					usleep(delta);
				}
			}
			ret = avcodec_send_packet(decoder, packet);
			if (ret < 0) {
				snprintf(awt->log_msg, LOG_MSG_LEN, "Error while sending a packet to the decoder\n");
				awt->log_callback("ERROR", awt->log_msg, ret);
				goto out;
			}

			while (ret >= 0) {
				ret = avcodec_receive_frame(decoder, frame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					break;
				} else if (ret < 0) {
					snprintf(awt->log_msg, LOG_MSG_LEN, "Error while receiving a frame from the decoder\n");
					awt->log_callback("ERROR", awt->log_msg, ret);
					goto out;
				}

				if (ret >= 0) {
					if ((ret = av_buffersrc_add_frame_flags(awt->fcs, frame, AV_BUFFERSRC_FLAG_KEEP_REF)) < 0) {
						snprintf(awt->log_msg, LOG_MSG_LEN, "Error while feeding the audio filtergraph\n");
						awt->log_callback("ERROR", awt->log_msg, ret);
						goto out;
					}
					while (1) {
						ret = av_buffersink_get_frame(awt->fcd, filt_frame);
						if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
							break;
						}
						if (ret < 0)
							goto out;
						ret = awt_filter_frame(awt, filt_frame);
						av_frame_unref(filt_frame);
						if(ret)
							goto out;
					}
					av_frame_unref(frame);
				}
			}
		}
		av_packet_unref(packet);
	}
out:
	if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
		ret = 0;
	}else if(ret)
		ret = AWT_CODE_PROCESS_ERROR;
end:
	awt->pre_duration += awt->current_duration;
	snprintf(awt->log_msg, LOG_MSG_LEN,
		"End:\n"
		"  time  : %.1f\n"
		"  found : %d\n"
		"  ec    : %d\n",
		(av_gettime() - start_time) / 1000000.0,
		awt->counter,
		ret);
	awt->log_callback("INFO", awt->log_msg, ret);
	avcodec_free_context(&decoder);
	avformat_close_input(&fmt_ctx);
	av_packet_free(&packet);
	av_frame_free(&frame);
	av_frame_free(&filt_frame);
	return ret;
}
int awt_exec(void **awt_ptr, const char *file_path){
	AWTContext *awt = *awt_ptr;
	int ret;
	if(!awt)
		return -1;
	pthread_mutex_lock(&awt->lock);
	ret = awt_run(awt_ptr, file_path);
	pthread_mutex_unlock(&awt->lock);
	return ret;
}
