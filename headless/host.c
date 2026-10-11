/* Only host presentation/input callbacks are replaced. CPUs, chips, sound
 * synthesis, geometry and software rendering are the production upstream. */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
uint16_t tc2_inputs, g_ss22_gun_x=457, g_ss22_gun_y=161;
bool g_ss22_gun_off;
int tc2_window_active(void) { return 0; }
int tc2_window_open(int scale, int fs) { (void)scale; (void)fs; return 0; }
void tc2_window_frame(void) {
    extern uint32_t rr_frame;
    static int initialized, pace, skew, jitter;
    static double start;
    struct timespec now;
    if(!initialized) {
        initialized=1; pace=getenv("TC2_PACE")!=NULL;
        skew=getenv("TC2_SKEW_MS")?atoi(getenv("TC2_SKEW_MS")):0;
        jitter=getenv("TC2_JITTER_MS")?atoi(getenv("TC2_JITTER_MS")):0;
        clock_gettime(CLOCK_MONOTONIC,&now); start=now.tv_sec+now.tv_nsec/1e9;
    }
    if(pace) {
        double target=start+rr_frame/59.906+skew/1000.0+(jitter?((rr_frame*17)%7)*jitter/6000.0:0);
        clock_gettime(CLOCK_MONOTONIC,&now); double delay=target-(now.tv_sec+now.tv_nsec/1e9);
        if(delay>0) { struct timespec wait={(time_t)delay,(long)((delay-(time_t)delay)*1e9)}; while(nanosleep(&wait,&wait)&&errno==EINTR){} }
    }
    const char *s=getenv("TC2_FRAMES");
    if (s && rr_frame >= strtoul(s, 0, 10)) {
        void tc2_on_exit(void); tc2_on_exit(); exit(0);
    }
}
void tc2_crash_box(const char *s) { fprintf(stderr,"%s\n",s); }
void tc2_audio_push(const int16_t *samples, int n) { (void)samples; (void)n; }
void tc2_out_poll(uint16_t v) { (void)v; }
void tc2_gl_read(uint32_t *rgba) { (void)rgba; abort(); }
