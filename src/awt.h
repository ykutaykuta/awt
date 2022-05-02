#ifndef __AWT_H__
#define __AWT_H__
#define AWT_CODE_SUCCESS        0
#define AWT_CODE_OPEN_ERROR    -1
#define AWT_CODE_SETUP_ERROR   -2
#define AWT_CODE_PROCESS_ERROR -3
/*
 * awt_ptr: point to main object
 * sample_rate: convert before processing
 * preset: 1...10
 *   -1: auto with payload_length
 * payload_length: 1...4
 * speed: 0.05...1000
 * second: 0...n, 0 = no limit, limit duration when processing
 * callback: call when detect watermark, return non-zero will stop
 */
int awt_open(void **awt_ptr,
	int sample_rate,
	int preset,
	int payload_length,
	float speed,
	int second,
	int (*callback)(int channel, const char *wm, float percent, float pos_at),
	void (*log_callback)(const char *level, const char *msg, const int ff_ret));
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
	void (*log_callback)(const char *level, const char *msg, const int ff_ret));
int awt_exec(void **awt_ptr, const char *file_path);
int awt_close(void **awt_ptr);
#endif
