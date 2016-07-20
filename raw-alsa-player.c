#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sound/asound.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#define INTERVAL_INIT (struct snd_interval) {0, UINT_MAX, 0, 0, 0, 0}
#define MASK_ZI(p) (p - SNDRV_PCM_HW_PARAM_FIRST_MASK)
#define INTERVAL_ZI(p) (p - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL)
#define AUDIO_BUF_MAX 32768
static const uint8_t sample_length_by_format[] = {1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 8, 8, 0, 0, 1, 1};
size_t pcm_write_loop(int fd, char *buf, size_t frames, size_t frame_len) {
	size_t result = 0;
st:;
	struct snd_xferi fi = {0, buf, frames};
	int ires = ioctl(fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &fi);
	if (ires==-1) {
		if (errno != EPIPE) {
			perror("SNDRV_PCM_IOCTL_WRITEI_FRAMES");
		}else{
			fputs("Underrun occurred\n", stderr);
			ioctl(fd, SNDRV_PCM_IOCTL_PREPARE, 0);
			ioctl(fd, SNDRV_PCM_IOCTL_START, 0);
			goto st;
		}
	}
	if ((ires == 0) && (fi.result > 0)) {
		result += fi.result;
		buf += fi.result * frame_len;
		frames -= fi.result;
		if (frames > 0) goto st;
	}else{
		if (result == 0) result = -1;
	}
	return result;
}
void hw_p_set_imask(int device_node, struct snd_pcm_hw_params *params, int param, unsigned int value, int is_min, int is_max) {
	if (param >= SNDRV_PCM_HW_PARAM_FIRST_INTERVAL) {
		struct snd_interval *iv = &params->intervals[INTERVAL_ZI(param)];
		iv->integer = !(is_min || is_max);
		iv->min = value;
		iv->max = value;
		iv->openmin = is_min;
		iv->openmax = is_max;
	}else{
		struct snd_mask *mask = &params->masks[MASK_ZI(param)];
		memset(mask, 0, sizeof(struct snd_mask));
		FD_SET(value, (fd_set *) mask);
	}
	params->cmask |= 1<<param;
	ioctl(device_node, SNDRV_PCM_IOCTL_HW_REFINE, params);
}
int main (int argc, char **argv) {
	int opt_char = 0;
	const char *device_nodename = "/dev/snd/pcmC0D0p";
/*	int timer_sched = 0; */
	unsigned int rate = 44100;
	unsigned int channels = 2;
	unsigned int format = SNDRV_PCM_FORMAT_S16_LE;
	unsigned int sample_length = 2;
	while ((opt_char = getopt(argc, argv, "D:r:c:f:V")) != -1) {
		switch(opt_char) {
			case 'D':
				device_nodename = optarg;
				break;
			case 'r':
				rate = atoi(optarg);
				break;
			case 'c':
				channels = atoi(optarg);
				break;
			case 'f':
				format = atoi(optarg);
				if ((format >= sizeof(sample_length_by_format)) || ((sample_length = sample_length_by_format[format]) == 0)) {
					fprintf(stderr, "Invalid sample format number %d\n", format);
					return 1;
				}
				break;
			case 'V':
/*				timer_sched = 1; */
				break;
		}
	}
	char *filename = argv[optind];
	int input_data_fd = filename ? atoi(filename) : STDIN_FILENO;
	size_t frame_len = (sample_length * channels);
	int device_node = open(device_nodename, O_ACCMODE);
	if (device_node == -1) {
		perror("Cannot open audio device");
		return 2;
	}
	struct snd_pcm_hw_params hw_params = {0};
	hw_params.rmask = 0xffffffff;
	hw_params.cmask = 0;
	hw_params.info = 0xffffffff;
	for (int i=0; i<=INTERVAL_ZI(SNDRV_PCM_HW_PARAM_LAST_INTERVAL); i++) {
		hw_params.intervals[i] = INTERVAL_INIT;
	}
	for (int i=0; i<=MASK_ZI(SNDRV_PCM_HW_PARAM_LAST_MASK); i++) {
		memset(&hw_params.masks[i], 255, sizeof(struct snd_mask));
	}
	hw_p_set_imask(device_node, &hw_params, SNDRV_PCM_HW_PARAM_RATE, rate, 0, 0);
	hw_p_set_imask(device_node, &hw_params, SNDRV_PCM_HW_PARAM_CHANNELS, channels, 0, 0);
	hw_p_set_imask(device_node, &hw_params, SNDRV_PCM_HW_PARAM_SAMPLE_BITS, sample_length * CHAR_BIT, 0, 0);
	hw_p_set_imask(device_node, &hw_params, SNDRV_PCM_HW_PARAM_FRAME_BITS, frame_len * CHAR_BIT, 0, 0);
	hw_p_set_imask(device_node, &hw_params, SNDRV_PCM_HW_PARAM_ACCESS, SNDRV_PCM_ACCESS_RW_INTERLEAVED, 0, 0);
	hw_p_set_imask(device_node, &hw_params, SNDRV_PCM_HW_PARAM_FORMAT, format, 0, 0);
	hw_p_set_imask(device_node, &hw_params, SNDRV_PCM_HW_PARAM_SUBFORMAT, 0, 0, 0);
	hw_params.info |= 0x80000000;
	hw_params.msbits = sample_length * CHAR_BIT;
	if (ioctl(device_node, SNDRV_PCM_IOCTL_HW_PARAMS, &hw_params)==-1) {
		perror("SNDRV_PCM_IOCTL_HW_PARAMS");
		return 1;
	}
	ioctl(device_node, SNDRV_PCM_IOCTL_PREPARE, 0);
	ioctl(device_node, SNDRV_PCM_IOCTL_START, 0);
	struct snd_pcm_status status = {0};
	size_t period_size = 2048;
	if (period_size > (AUDIO_BUF_MAX / frame_len)) period_size = AUDIO_BUF_MAX / frame_len;
	while (1) {
		char *buf = malloc(period_size * frame_len);
		ssize_t nread = read(input_data_fd, buf, period_size * frame_len);
		if (nread <= 0) {
			while ((ioctl(device_node, SNDRV_PCM_IOCTL_DRAIN, 0) == -1) && (errno == EINTR)) ;
			free(buf);
			break;
		}
		ssize_t n = pcm_write_loop(device_node, buf, nread / frame_len, frame_len);
		free(buf);
		if (n != (nread / frame_len)) {
			break;
		}
		ioctl(device_node, SNDRV_PCM_IOCTL_STATUS, &status);
	}
	ioctl(device_node, SNDRV_PCM_IOCTL_HW_FREE, 0);
	close(device_node);
	return 0;
}
