#ifndef __MULTICAST_VIDEO_H__
#define __MULTICAST_VIDEO_H__

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" 
{
#endif

#define MULTICAST_SOURCE_NAME_SIZE 64

// Payload size of 1472 chosen through experiment to get the maximum payload
// without any IP fragmentation observed using network tracing.
// Wireshark shows fragmentation as "IP  Fragmented IP Protocol (proto=UDP 0x11, off=0)".
#define PACKET_SIZE 1472

#define MULTICAST_DEFAULT_PORT 2000

typedef struct {
	uint32_t	frame_number;
	uint16_t	packets_per_frame;
	uint16_t	audio_size;
	uint16_t	width;
	uint16_t	height;
	uint32_t	vitc;
	uint32_t	ltc;
	uint16_t	framerate_numer;
	uint16_t	framerate_denom;
	uint8_t		signal_ok;
	char		source_name[MULTICAST_SOURCE_NAME_SIZE];	// includes terminating NUL
} IngexNetworkHeader;

extern int connect_to_multicast_address(const char *address, int port);
extern int open_socket_for_streaming(const char *remote, int port);

extern int udp_read_frame_audio_video(int fd, double timeout, IngexNetworkHeader *p_header, uint8_t *video, uint8_t *audio, int *p_total);
extern int udp_read_frame_header(int fd, IngexNetworkHeader *p_header);

extern int send_audio_video(int fd, int width, int height, int audio_channels, const uint8_t *video, const uint8_t *audio, int frame_number, int vitc, int ltc, const char *source_name);

// Number of frames in ring buffer - encoded into flags byte
#define UDP_FRAME_BUFFER_MAX            4
#define UDP_FRAME_BUFFER_FLAGS_MASK     0x03

typedef struct {
	int64_t first_time;
	int64_t last_time;
	int packets;
	int header_frame_number;
	int frame_complete;
} FrameStats;

typedef struct {
	int fd;
	pthread_t udp_reader_thread_id;
	uint8_t *ring[UDP_FRAME_BUFFER_MAX];
	FrameStats stats[UDP_FRAME_BUFFER_MAX];
	pthread_mutex_t m_frame_copy[UDP_FRAME_BUFFER_MAX];
	int audio_size;
	int video_size;
	int ring_audio_offset;
	int ring_video_offset;
	int next_frame;
	int last_header_frame_read;
} udp_reader_thread_t;

extern int udp_init_reader(int width, int height, udp_reader_thread_t *p_udp_reader);
extern int udp_shutdown_reader(udp_reader_thread_t *p_udp_reader);
extern int udp_read_next_frame(udp_reader_thread_t *p_udp_reader, double timeout, IngexNetworkHeader *p_header_out, uint8_t *video_out, uint8_t *audio_out, int *p_total);


extern void scale_video420_for_multicast(int in_width, int in_height, int out_width, int out_height, const uint8_t *video_frame, uint8_t *scaled_frame);
extern void scale_video422_for_multicast(int in_width, int in_height, int out_width, int out_height, const uint8_t *video_frame, uint8_t *scaled_frame);

#ifdef __cplusplus
}
#endif

#endif
