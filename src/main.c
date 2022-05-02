
#include "awt.h"
#include <unistd.h>
#include <sys/resource.h>
#include <pthread.h>

int counter = 0;
void *awt = 0;
int callback(int channel, const char *wm, float percent, float pos_at){
	printf("found at %.2fs #%d: %s %.2f\n", pos_at, channel, wm, percent);
	counter++;
	return 0;
}
void log_callback(const char *level, const char *msg, const ff_ret){
	printf("%s ffmpeg ret_code %d: %s\n", level, ff_ret, msg);
}
void print_mem(const char *tag){
	struct rusage rusage;
	getrusage(RUSAGE_SELF, &rusage);
	printf(" %s %ld\n", tag, rusage.ru_maxrss);
}
void *test_thread(){
	while(1){
		printf("checking\n");
		usleep(2000000);
		// stop when found 2 watermark
		if(counter >= 2){
			awt_close(&awt);
			break;
		}
	}
	return 0;
}
int main(int argc, char **argv)
{
    int ret;
    const char *input = argc > 1 ? argv[1] : "record.ts";
    awt_open(&awt, 44100, -1, 3, 1, 0, callback, log_callback);
    int run = 52;
    int i = 10;
    char path[16];
	pthread_t th;
    if((ret = pthread_create(&th, 0, test_thread, 0))){
		goto end;
	}
    while(i++ < run){
    	sprintf(path,"p%d.ts", i);
    	ret = awt_exec(&awt, path);
    	if(ret)
    		break;
    	usleep(200000);
    }
	pthread_join(th, 0);
    awt_close(&awt);
    printf("found %d watermark\n", counter);
end:
    return ret;
}
