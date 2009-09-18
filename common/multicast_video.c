#include "multicast_video.h"
#include "YUV_scale_pic.h"

#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

#define TESTING_FLAG		0

// Define DUMP_STATS to print out stats
//#define DUMP_STATS  1

// Define DEBUG_UDP_SEND_RECV to have packets read/written to a file instead of sockets
// This is useful for testing out-of-order and never-arrives packet reception.
#ifdef DEBUG_UDP_SEND_RECV

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

extern int connect_to_multicast_address(const char *address, int port __attribute__ ((unused)))
{
#if TESTING_FLAG
	printf("Inside multicast_video: connect_to_multicast_address with DEBUG_UDP_SEND_RECV; return\n)"
#endif
	return open(address, O_RDONLY);
}
extern int open_socket_for_streaming(const char *remote, int port __attribute__ ((unused)))
{
#if TESTING_FLAG
	printf("Inside multicast_video: open_socket_for_streaming with DEBUG_UDP_SEND_RECV; return\n)"
#endif
	return open(remote, O_CREAT|O_TRUNC|O_RDWR, 0664);
}

#else

extern int connect_to_multicast_address(const char *address, int port)
{
#if TESTING_FLAG
	printf("Inside multicast_video: connect_to_multicast_address\n");
#endif
int FLAG_V6 = 1;
if (strchr(address, ':')==NULL)
	FLAG_V6 = 0;
#if TESTING_FLAG
	printf("FLAG_V6 = %d \n",FLAG_V6);
#endif
int fd;
if (FLAG_V6 == 1)
{
	if ((fd = socket( AF_INET6, SOCK_DGRAM, 0 )) == -1) {	// SOCK_DGRAM means UDP socket
		perror("cannot create socket");
		return -1;
	}
}
else
{

	if ((fd = socket( AF_INET, SOCK_DGRAM, 0 )) == -1) {	// SOCK_DGRAM means UDP socket
		perror("cannot create socket");
		return -1;
	}
}//end of FLAG_V6

	/* Make sure bind will work if socket already in use */
	int i_opt = 1;
	if (setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (void *) &i_opt, sizeof( i_opt ) ) == -1 )
	{
		perror("cannot configure socket (SO_REUSEADDR)");
		return -1;
	}

	/* Attempt to increase buffer size */
	i_opt = 0x80000;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *) &i_opt, sizeof(i_opt)) == -1) {
		perror("cannot configure socket (SO_RCVBUF)");
	}
	i_opt = 0;
	unsigned i_opt_size = sizeof(i_opt);
	getsockopt(fd, 1, 8, (void*) &i_opt, &i_opt_size);
	//printf("socket buffer set to 0x%x (%d)\n", i_opt, i_opt);
	if (FLAG_V6 ==1)
	{
		/* setup address for bind */
		struct sockaddr_in6 sock;
		memset(&sock, 0, sizeof(struct sockaddr_in6));
		sock.sin6_family = AF_INET6;
		sock.sin6_port = htons(port);
		inet_pton( AF_INET6, address,sock.sin6_addr.s6_addr );
		if (bind(fd, (struct sockaddr *)&sock, sizeof( sock )) < 0) {
			perror("cannot bind socket");
			return -1;
		}
		/* Join multicast group if multicast address */
		if ( ((((in_addr_t)(ntohl(sock.sin6_addr.s6_addr))) & 0xf0000000) == 0xe0000000) ) {
			struct ip_mreq imr;
	
			imr.imr_multiaddr.s_addr = sock.sin6_addr.s6_addr;
			imr.imr_interface.s_addr = INADDR_ANY;
	
			if (setsockopt( fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
							(char*)&imr, sizeof(struct ip_mreq) ) == -1) {
				perror("failed to join IP multicast group");
				return -1;
			}
		}
		printf("Joined multicast group for %s:%d\n", address, port);
		return fd;
	}					
	else
	{
		/* setup address for bind */
		struct sockaddr_in sock;
		memset(&sock, 0, sizeof(struct sockaddr_in));
		sock.sin_family = AF_INET;
		sock.sin_port = htons(port);
		sock.sin_addr.s_addr = inet_addr(address);		// convert e.g. "224.1.0.20" string
		
		if (bind(fd, (struct sockaddr *)&sock, sizeof( sock )) < 0) {
			perror("cannot bind socket");
			return -1;
		}
		/* Join multicast group if multicast address */
		if ( ((((in_addr_t)(ntohl(sock.sin_addr.s_addr))) & 0xf0000000) == 0xe0000000) ) {
			struct ip_mreq imr;
	
			imr.imr_multiaddr.s_addr = sock.sin_addr.s_addr;
			imr.imr_interface.s_addr = INADDR_ANY;
	
			if (setsockopt( fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
							(char*)&imr, sizeof(struct ip_mreq) ) == -1) {
				perror("failed to join IP multicast group");
				return -1;
			}
		}
		printf("Joined multicast group for %s port %d\n", address, port);
		return fd;
	}//end of FLAG_V6
}

extern int open_socket_for_streaming(const char *remote, int port)
{
#if TESTING_FLAG
	printf("Inside multicast_video: open_socket_for_streaming\n");
#endif
	int fd, i_opt;
	unsigned			i_opt_size;
	int FLAG_V6 = 1;
	if (strchr(remote, ':')==NULL)
		FLAG_V6 = 0;
#if TESTING_FLAG
	printf("FLAG_V6 = %d \n",FLAG_V6);
#endif
	if (FLAG_V6 == 1)
	{
		if ((fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
			perror("cannot create socket");
			return -1;
		}
	}
	else
	{
		if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
			perror("cannot create socket");
			return -1;
		}
	}
	/* Make sure bind will work if socket already in use */
	i_opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *) &i_opt, sizeof(i_opt)) == -1) {
		perror("cannot configure setsockopt1 for socket (SO_REUSEADDR)");
		return -1;
	}
#if TESTING_FLAG
	printf("setsockopt1 \n");
#endif
	/* TODO: do we need to make this non blocking? */
	/* TODO: fcntl64(8, F_SETFL, O_RDWR|O_NONBLOCK) */

	/* Attempt to increase buffer size */
	i_opt = 0x80000;
	if (setsockopt( fd, SOL_SOCKET, SO_RCVBUF, (void *) &i_opt, sizeof(i_opt)) == -1) {
		perror("cannot configure setsockopt2 for socket (SO_RCVBUF)");
	}
#if TESTING_FLAG
	printf("setsockopt2 \n");
#endif
	if (setsockopt( fd, SOL_SOCKET, SO_SNDBUF, (void *) &i_opt, sizeof(i_opt)) == -1) {
		perror("cannot configure setsockopt3 for socket (SO_SNDBUF)");
	}
#if TESTING_FLAG
	printf("setsockopt3 \n");
#endif
	i_opt = 0;
	i_opt_size = sizeof( i_opt );
	getsockopt( fd, 1, 8, (void*) &i_opt, &i_opt_size );
	//if (i_opt != 0x80000) {				// debug to test kernel tuning
	//	printf("socket buffer set to %d bytes\n", i_opt);
	//}

	/* Set multicast time-to-live */
	i_opt = 12;
	if (setsockopt(fd, SOL_IP, IP_MULTICAST_TTL, &i_opt, sizeof(i_opt)) == -1) {
		perror("cannot set multicast hop limit");
	}
#if TESTING_FLAG
	printf("setsockopt4 \n");
#endif
	
	if (FLAG_V6 ==1)
	{
		/* setup address for connect */
		struct sockaddr_in6 sock;
		memset(&sock, 0, sizeof( struct sockaddr_in6 ) );
		sock.sin6_family = AF_INET6;
		sock.sin6_port = htons( port );
		inet_pton( AF_INET6, remote,sock.sin6_addr.s6_addr );		// convert "224.1.0.20" string
		if (connect(fd, (struct sockaddr *)&sock, sizeof(sock)) < 0) {
			perror("cannot connect socket");
			return -1;
		}
}
	else
	{
		/* setup address for bind */
		struct sockaddr_in sock;
		memset(&sock, 0, sizeof(struct sockaddr_in));
		sock.sin_family = AF_INET;
		sock.sin_port = htons(port);
		sock.sin_addr.s_addr = inet_addr( remote );		// convert "224.1.0.20" string
		if (connect(fd, (struct sockaddr *)&sock, sizeof(sock)) < 0) {
			perror("cannot connect socket");
			return -1;
		}
	}
#if TESTING_FLAG
	printf("connected to socket. Fingers crossed everthing should work \n");
#endif
	return fd;
}
#endif 
// END of #ifdef DEBUG_UDP_SEND_RECV

// Packet structure for uncompressed video and audio with timecode transmission:
//  packet-header       [4] ('I'[1], flags,frame_num[1], packet_num[2] (little-endian))
//  packet-data         [PACKET_SIZE - 4]
//  
// packet-data contains:
//  packet_num=0                      IngexNetworkHeader, audio
//  packet_num=1,2                    audio
//  packet_num=3..packets_per_frame   video
//

// flags set in second byte of packet (after 'I')
#define UDP_FLAG_HEADER	0x80
#define UDP_FLAG_AUDIO	0x40
#define UDP_FLAG_VIDEO	0x20

#define PTHREAD_MUTEX_LOCK(x) if (pthread_mutex_lock( x ) != 0 ) fprintf(stderr, "pthread_mutex_lock failed\n");
#define PTHREAD_MUTEX_UNLOCK(x) if (pthread_mutex_unlock( x ) != 0 ) fprintf(stderr, "pthread_mutex_unlock failed\n");

static void *udp_reader_thread(void *arg);

extern int udp_init_reader(int width, int height, udp_reader_thread_t *p_udp_reader)
{
#if TESTING_FLAG
	printf("Inside multicast_video: udp_init_reader\n");
#endif
	int res;

	// setup element size and offsets
	int video_size = width*height*3/2;
	int audio_size = 1920*2 * 2;
	int element_size = sizeof(IngexNetworkHeader) + audio_size + video_size;
	p_udp_reader->audio_size = audio_size;
	p_udp_reader->video_size = video_size;
	p_udp_reader->ring_audio_offset = sizeof(IngexNetworkHeader);
	p_udp_reader->ring_video_offset = p_udp_reader->ring_audio_offset + audio_size;

	// allocate ring buffer
	int i;
	for (i = 0; i < UDP_FRAME_BUFFER_MAX; i++) {
		p_udp_reader->ring[i] = (uint8_t *)malloc(element_size);

		// clear entire frame since packet loss may cause some data to be read uninitialised
		memset(p_udp_reader->ring[i], 0, element_size);
		memset(&p_udp_reader->stats[i], 0, sizeof(FrameStats));

		// Initialise mutexes
		if ((res = pthread_mutex_init(&p_udp_reader->m_frame_copy[i], NULL)) != 0) {
			fprintf(stderr, "pthread_mutex_init() failed: %s\n", strerror(res));
			return 0;
		}
	}

	// Although frame 0 is not ready at this point, the frame data will not be
	// read until the FrameStats's frame_complete flag is signalled.
	p_udp_reader->next_frame = 0;

	// Make sure first frame read succeeds
	p_udp_reader->last_header_frame_read = -1;

	// create reader thread which will immediately start filling buffers
	if ((res = pthread_create(&p_udp_reader->udp_reader_thread_id, NULL, udp_reader_thread, p_udp_reader)) != 0) {
		fprintf(stderr, "Failed to create udp reader thread: %s\n", strerror(res));
		return 0;
	}

	return 1;
}

extern int udp_shutdown_reader(udp_reader_thread_t *p_udp_reader)
{
#if TESTING_FLAG
	printf("Inside multicast_video: udp_shutdown_reader\n");
#endif
	int res;
	if ((res = pthread_cancel(p_udp_reader->udp_reader_thread_id)) != 0) {
		fprintf(stderr, "Failed to cancel udp reader thread: %s\n", strerror(res));
		return 0;
	}

	if ((res = pthread_join(p_udp_reader->udp_reader_thread_id, NULL)) != 0) {
		fprintf(stderr, "pthread_join returned %d: %s\n", res, strerror(res));
	}

	// free ring buffer
	int i;
	for (i = 0; i < UDP_FRAME_BUFFER_MAX; i++) {
		free(p_udp_reader->ring[i]);
	}

	return 1;
}

static int64_t gettimeofday64(void)
{
#if TESTING_FLAG
	printf("Inside multicast_video: gettimeofday64\n");
#endif
	struct timeval tv;
	gettimeofday(&tv, NULL);
	int64_t tod = (int64_t)tv.tv_sec * 1000000 + tv.tv_usec ;
	return tod;
}

static void *udp_reader_thread(void *arg)
{
#if TESTING_FLAG
	printf("Inside multicast_video: udp_reader_thread\n");
#endif
	udp_reader_thread_t *p_udp_reader = (udp_reader_thread_t*)arg;
	int fd = p_udp_reader->fd;

	uint8_t buf[PACKET_SIZE];
	int packets_read = 0;
	int total_bytes = 0;

	int audio_size = p_udp_reader->audio_size;
	int video_size = p_udp_reader->video_size;
	int packets_per_frame = (int)( (sizeof(IngexNetworkHeader) + audio_size + video_size) / (double)(PACKET_SIZE-4) + 0.999999 );

	/* Read a frame from the network */
	while (1)
	{
		// Block while waiting to read from socket
#ifdef DEBUG_UDP_SEND_RECV
		ssize_t bytes_read = read(fd, buf, PACKET_SIZE);
		char junk[1];
		read(fd, junk, 1);
#else
		ssize_t bytes_read = recv(fd, buf, PACKET_SIZE, 0);
#endif

		// Check for corrupted packet - should never happen
		if ((bytes_read != PACKET_SIZE) || (buf[0] != 'I')) {
			printf("Bad packet: bytes_read=%d (PACKET_SIZE=%d) buf[0]=0x%02x\n", bytes_read, PACKET_SIZE, buf[0]);
			continue;
		}

		total_bytes += bytes_read;

		// Packet may not arrive in order, so sort them into a ring buffer of frames
		// given by the frame_number value.

		uint8_t flags = buf[1];
		int frame_number = buf[1] & UDP_FRAME_BUFFER_FLAGS_MASK;

		uint16_t packet_num;
		memcpy(&packet_num, &buf[2], sizeof(packet_num));

		//printf("0x%02x,0x%02x,0x%02x,0x%02x  packets_read=%d frame_number=%2d packet_num=%2d flags=%s%s%s\n", buf[0], buf[1], buf[2], buf[3], packets_read, frame_number, packet_num, (flags & UDP_FLAG_HEADER) ? "H":"", (flags & UDP_FLAG_AUDIO) ? "A":"", (flags & UDP_FLAG_VIDEO) ? "V":"");

		packets_read++;

		FrameStats *p_stats = &p_udp_reader->stats[frame_number];
		IngexNetworkHeader *p_header = (IngexNetworkHeader *)p_udp_reader->ring[frame_number];
		uint8_t *audio = p_udp_reader->ring[frame_number] + p_udp_reader->ring_audio_offset;
		uint8_t *video = p_udp_reader->ring[frame_number] + p_udp_reader->ring_video_offset;

		if (p_stats->packets == 0) {
			p_stats->first_time = gettimeofday64();
		}

		// Use mutex to avoid overwriting frame read in main thread
		PTHREAD_MUTEX_LOCK( &p_udp_reader->m_frame_copy[frame_number] )

		if ((flags & UDP_FLAG_HEADER) && packet_num == 0) {
			// copy header into structure
			memcpy(p_header, &buf[4], sizeof(IngexNetworkHeader));
			video_size = p_header->width * p_header->height * 3 / 2;
			audio_size = p_header->audio_size;

			// copy audio
			int audio_chunk_size = PACKET_SIZE - (4 + sizeof(IngexNetworkHeader));
			if (audio_chunk_size >= 0 && audio_chunk_size <= audio_size)
				memcpy(audio, &buf[4 + sizeof(IngexNetworkHeader)], audio_chunk_size);

			// update frame stats
			p_stats->header_frame_number = p_header->frame_number;
		}
		else if ((flags & UDP_FLAG_AUDIO)) {
			// The first portion of audio from the header has already been filled
			int audio_from_header_size = PACKET_SIZE - (4 + sizeof(IngexNetworkHeader));
			int audio_chunk_size = PACKET_SIZE - 4;

			// Packets containing only audio data start from packet_num==1
			// We must skip over the audio which was carried in the header.
			int audio_pos = audio_from_header_size + (packet_num - 1) * audio_chunk_size;
			if (audio_pos + audio_chunk_size > audio_size)
				audio_chunk_size = audio_size - audio_pos;

			if (audio_pos >= 0 && (audio_pos + audio_chunk_size) <= audio_size)
				memcpy(audio + audio_pos, &buf[4], audio_chunk_size);
			else
				printf("Bad memcpy calc: packet_num=%d audio_pos=%d audio_chunk_size=%d audio_size=%d\n", packet_num, audio_pos, audio_chunk_size, audio_size);
		}
		else if (flags & UDP_FLAG_VIDEO) {
			// video packet
			int video_chunk_size = PACKET_SIZE - 4;

			// packets can arrive out-of-order so
			// calculate position in video frame for data based on packet_num
			int first_video_packet = (int)( (sizeof(IngexNetworkHeader) + audio_size) / (double)(PACKET_SIZE-4) + 0.999999 );
			int video_pos = (packet_num - first_video_packet) * video_chunk_size;
			if (video_pos + video_chunk_size > video_size)
				video_chunk_size = video_size - video_pos;

			if (video_pos >= 0 && (video_pos + video_chunk_size) <= video_size)
				memcpy(video + video_pos, &buf[4], video_chunk_size);
			else
				printf("Bad memcpy calc: packet_num=%d video_pos=%d video_chunk_size=%d video_size=%d\n", packet_num, video_pos, video_chunk_size, video_size);
		}

		PTHREAD_MUTEX_UNLOCK( &p_udp_reader->m_frame_copy[frame_number] )

		p_stats->packets++;
		if (p_stats->packets == packets_per_frame) {
			p_stats->last_time = gettimeofday64();
			p_stats->frame_complete = 1;
			//printf("packet complete: first=%lld, last=%lld, diff=%lld, packets=%d\n", p_stats->first_time, p_stats->last_time, p_stats->last_time - p_stats->first_time, p_stats->packets);
		}
	}
	return NULL;
}

#if defined(DUMP_STATS)
static void dump_stats(udp_reader_thread_t *p_udp_reader)
{
#if TESTING_FLAG
	printf("Inside multicast_video: dump_stats\n");
#endif
	int i;
	for (i = 0; i < UDP_FRAME_BUFFER_MAX; i++) {
		printf("%2d:%s packets=%2d frame_number=%6d %s\n", i, p_udp_reader->next_frame == i ? "*":"-",
			p_udp_reader->stats[i].packets,
			p_udp_reader->stats[i].header_frame_number,
			p_udp_reader->stats[i].frame_complete ? "*":".");
	}

}
#endif

extern int udp_read_next_frame(udp_reader_thread_t *p_udp_reader, double timeout, IngexNetworkHeader *p_header_out, uint8_t *video_out, uint8_t *audio_out, int *p_total)
{
#if TESTING_FLAG
	printf("Inside multicast_video: udp_read_next_frame\n");
#endif


#if defined(DUMP_STATS)
	printf("\nBefore timed wait (last_header_frame_read=%d)\n", p_udp_reader->last_header_frame_read);
	dump_stats(p_udp_reader);
#endif

	int frame_number = p_udp_reader->next_frame;
	FrameStats *p_stats = &p_udp_reader->stats[frame_number];

	if (p_stats->header_frame_number <= p_udp_reader->last_header_frame_read ||
		! p_stats->frame_complete) {
		usleep(timeout * 1000000);
		if (p_stats->header_frame_number <= p_udp_reader->last_header_frame_read ||
			! p_stats->frame_complete) {
			// timeout waiting for complete frame

			// process incomplete frame if header and enough audio available
			if (p_stats->header_frame_number == 0 || p_stats->packets < 5) {
				// Not enough data for a worthwhile frame
				*p_total = p_stats->packets;
				return -1;
			}
		}
	}

#if defined(DUMP_STATS)
	printf("After\n");
	dump_stats(p_udp_reader);
#endif

	// Use mutex to guard against data being overwritten during the memcpy
	PTHREAD_MUTEX_LOCK( &p_udp_reader->m_frame_copy[frame_number] )

	// copy buffered frame out
	IngexNetworkHeader *p_header = (IngexNetworkHeader *)p_udp_reader->ring[frame_number];
	uint8_t *audio = p_udp_reader->ring[frame_number] + p_udp_reader->ring_audio_offset;
	uint8_t *video = p_udp_reader->ring[frame_number] + p_udp_reader->ring_video_offset;

	memcpy(p_header_out, p_header, sizeof(IngexNetworkHeader));
	memcpy(audio_out, audio, p_udp_reader->audio_size);
	memcpy(video_out, video, p_udp_reader->video_size);

	*p_total = p_stats->packets;

	PTHREAD_MUTEX_UNLOCK( &p_udp_reader->m_frame_copy[frame_number] )

	// increment next frame indicator
	p_udp_reader->next_frame++;
	if (p_udp_reader->next_frame == UDP_FRAME_BUFFER_MAX)
		p_udp_reader->next_frame = 0;

	p_udp_reader->last_header_frame_read = p_header->frame_number;

	// clear frame we just read
	memset(p_udp_reader->ring[frame_number], 0, sizeof(FrameStats) + sizeof(IngexNetworkHeader));
	memset(p_stats, 0, sizeof(FrameStats));

	return 0;
}

extern int udp_read_frame_audio_video(int fd, double timeout, IngexNetworkHeader *p_header, uint8_t *video, uint8_t *audio, int *p_total)
{
#if TESTING_FLAG
	printf("Inside multicast_video: udp_read_frame_audio_video\n");
#endif

	long sec_val = (int)timeout; 
	long microsec_val = (timeout - sec_val) * 1000000; 
	struct timeval timeout_val = {sec_val, microsec_val};

	int packets_read = 0;
	int packets_lost_for_sync = 0;
	int total_bytes = 0;
	int last_packet_num = -1;
	int video_size = 0;
	int audio_size = 0;
	int found_first_packet = 0;
	uint16_t packet_num;

	uint8_t *orig_video = video;
	uint8_t *orig_audio = audio;

	uint8_t buf[PACKET_SIZE];

	//printf("\n*** udp_read_frame_audio_video() orig video=%p orig audio=%p\n", orig_video, orig_audio);

    /* Read a frame from the network */
	while (1)
    {
		fd_set	readfds;
		int		n;

		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);

		n = select(fd + 1, &readfds, NULL, NULL, &timeout_val);
		if (n == 0)
			return -1;		// timeout

		if (! FD_ISSET(fd, &readfds))
			continue;		// not readable - try again

#ifdef DEBUG_UDP_SEND_RECV
		ssize_t bytes_read = read(fd, buf, PACKET_SIZE);
		char junk[1];
		read(fd, junk, 1);
#else
		ssize_t bytes_read = recv(fd, buf, PACKET_SIZE, 0);
#endif

		if ((bytes_read != PACKET_SIZE) || (buf[0] != 'I'))
			return -1;		// bad packet

		total_bytes += bytes_read;

		// Packet reading code must cope with packets not arriving in order or at all.
		// Since the video is intended for monitoring only, it doesn't matter if
		// we mix data from different frames.

		uint8_t flags = buf[1];
		memcpy(&packet_num, &buf[2], sizeof(packet_num));

		//printf("0x%02x,0x%02x,0x%02x,0x%02x 0x%02x,0x%02x,0x%02x,0x%02x packet_num=%d packets_read=%d found_first_packet=%d\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], packet_num, packets_read, found_first_packet);

		// keeping trying until we read a packet number 0
		if (!found_first_packet && packet_num != 0) {
			packets_lost_for_sync++;
			continue;
		}

		packets_read++;

		if ((flags & UDP_FLAG_HEADER) && packet_num == 0) {
			if (found_first_packet) {
				// If we've only read one previous packet (the first start packet)
				// then reset variables and overwrite data
				if (packets_read == 2) {
					printf("- found second start packet - discarding previous start packet\n");
					audio = orig_audio;
					video = orig_video;
				}
				else {
					printf("- break - stopping since found second start packet (packets_read=%d)\n", packets_read);
					break;
				}
			}
			found_first_packet = 1;

			// copy header into structure
			memcpy(p_header, &buf[4], sizeof(IngexNetworkHeader));
			video_size = p_header->width * p_header->height * 3 / 2;
			audio_size = p_header->audio_size;

			// copy audio
			int audio_chunk_size = PACKET_SIZE - (4 + sizeof(IngexNetworkHeader));
			if (audio_chunk_size >= 0 && audio_chunk_size <= audio_size)
				memcpy(audio, &buf[4 + sizeof(IngexNetworkHeader)], audio_chunk_size);
		}
		else if ((flags & UDP_FLAG_AUDIO)) {
			// The first portion of audio from the header has already been filled
			int audio_from_header_size = PACKET_SIZE - (4 + sizeof(IngexNetworkHeader));
			int audio_chunk_size = PACKET_SIZE - 4;

			// Packets containing only audio data start from packet_num==1
			// We must skip over the audio which was carried in the header.
			int audio_pos = audio_from_header_size + (packet_num - 1) * audio_chunk_size;
			if (audio_pos + audio_chunk_size > audio_size)
				audio_chunk_size = audio_size - audio_pos;

			if (audio_pos >= 0 && (audio_pos + audio_chunk_size) <= audio_size)
				memcpy(audio + audio_pos, &buf[4], audio_chunk_size);
			else
				printf("Bad memcpy calc: packet_num=%d audio_pos=%d audio_chunk_size=%d audio_size=%d\n", packet_num, audio_pos, audio_chunk_size, audio_size);
		}
		else if (flags & UDP_FLAG_VIDEO) {
			// video packet
			int video_chunk_size = PACKET_SIZE - 4;

			// packets can arrive out-of-order so
			// calculate position in video frame for data based on packet_num
			int first_video_packet = (int)( (sizeof(IngexNetworkHeader) + audio_size) / (double)(PACKET_SIZE-4) + 0.999999 );
			int video_pos = (packet_num - first_video_packet) * video_chunk_size;
			if (video_pos + video_chunk_size > video_size)
				video_chunk_size = video_size - video_pos;

			if (video_pos >= 0 && (video_pos + video_chunk_size) <= video_size)
				memcpy(video + video_pos, &buf[4], video_chunk_size);
			else
				printf("Bad memcpy calc: packet_num=%d video_pos=%d video_chunk_size=%d video_size=%d\n", packet_num, video_pos, video_chunk_size, video_size);
		}

		if (p_header->packets_per_frame > 0) {
			// If we got the last packet or we've read the right number of packets, stop
			if (packet_num == p_header->packets_per_frame - 1 ||
				(found_first_packet && packets_read == p_header->packets_per_frame))
			break;
		}

		last_packet_num = packet_num;
    }
	if (packets_lost_for_sync > 0 || packet_num+1 != p_header->packets_per_frame
		|| packet_num+1 != packets_read)
		printf("packets_per_frame=%d packets_read=%d packet_num=%d last_num=%d packets_lost_for_sync=%d header.frame_number=%d\n", p_header->packets_per_frame, packets_read, packet_num, last_packet_num, packets_lost_for_sync, p_header->frame_number);

	*p_total = packets_read;
	return 0;
}

extern int udp_read_frame_header(int fd, IngexNetworkHeader *p_header)
{
#if TESTING_FLAG
	printf("Inside multicast_video: udp_read_frame_header\n");
#endif
	uint8_t buf[PACKET_SIZE];

	printf("Reading video parameters from multicast stream...");
	fflush(stdout);

    /* Read a frame from the network */
	while (1)
    {
		fd_set	readfds;
		int		n;

		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);

		n = select(fd + 1, &readfds, NULL, NULL, NULL);		// last arg NULL means no timeout
		if (n == 0)
			return -1;		// select failed for some reason

		if (! FD_ISSET(fd, &readfds))
			continue;		// not readable - try again

#ifdef DEBUG_UDP_SEND_RECV
		ssize_t bytes_read = read(fd, buf, PACKET_SIZE);
		char junk[1];
		read(fd, junk, 1);
#else
		ssize_t bytes_read = recv(fd, buf, PACKET_SIZE, 0);
#endif

		if ((bytes_read != PACKET_SIZE) || (buf[0] != 'I'))
			return -1;		// bad packet

		// The header is always in packet number 0
		uint16_t packet_num;
		memcpy(&packet_num, &buf[2], sizeof(packet_num));

		if (packet_num != 0)
			continue;

		// fill out header
		memcpy(p_header, &buf[4], sizeof(IngexNetworkHeader));

		break;
    }
	printf(" width=%d height=%d framerate=%d/%d audio_size=%d packets_per_frame=%d frame_number=%d source_name=\"%s\"\n",
			p_header->width, p_header->height, p_header->framerate_numer, p_header->framerate_denom, p_header->audio_size, p_header->packets_per_frame, p_header->frame_number, p_header->source_name);
	return 0;
}

// Write a full frame of video and audio to the network, breaking it up into
// a number of UDP packets for transmission.
extern int send_audio_video(int fd, int width, int height, int audio_channels,
						const uint8_t *video, const uint8_t *audio,
						int frame_number, int vitc, int ltc, const char *source_name)
{

#if TESTING_FLAG
	printf("HERE !!!! Inside multicast_video: send_audio_video\n");
#endif
	// FIXME: change this to a stack variable after testing with valgrind
	unsigned char *buf = malloc(PACKET_SIZE);
	int total_bytes_written = 0;

	int video_size = width * height * 3/2;				// video is YUV planar 4:2:0
	int audio_size = audio_channels * (1920 * 2);		// each audio channel is 48kHz, 16bit
	int header_size = sizeof(IngexNetworkHeader);
	IngexNetworkHeader header;

	// Number of whole packets required to transmit header, audio and video.
	// There may be unused space at the end of the last video packet.
	int packets_per_frame = (int)( (header_size + audio_size + video_size) / (double)(PACKET_SIZE-4) + 0.999999 );

	// fill out header
	memset(&header, 0, sizeof(header));
	header.packets_per_frame = packets_per_frame;
	header.signal_ok = 1;
	header.framerate_numer = 25;
	header.framerate_denom = 1;
	header.width = width;
	header.height = height;
	header.audio_size = audio_size;
	header.frame_number = frame_number;
	header.vitc = vitc;
	header.ltc = ltc;
	if (source_name) {
		strncpy(header.source_name, source_name, sizeof(header.source_name)-1);
		header.source_name[sizeof(header.source_name)-1] = '\0';
	}
	else
		header.source_name[0] = '\0';

	int video_chunk_size = PACKET_SIZE - 4;
	const uint8_t *p_video_chunk = video;
	int remaining_audio = audio_size;

	uint16_t packet_num = 0;
	while (1) {
		// packet header is 4 bytes starting with 'I' sync byte
		buf[0] = 'I';		// sync byte 'I' for ingex
		buf[1] = 0;			// flags

		// lower bits of buf[1] has frame_number 0 .. 15
		buf[1] |= (frame_number % UDP_FRAME_BUFFER_MAX) & UDP_FRAME_BUFFER_FLAGS_MASK;

		// packet_num is sent as native endian, 16 bits
		memcpy(&buf[2], &packet_num, sizeof(packet_num));

		if (packet_num == 0) {
			// write header followed by some audio data
			buf[1] |= UDP_FLAG_HEADER;			// set flag to indicate start-of-frame
			memcpy(&buf[4], &header, sizeof(IngexNetworkHeader));

			// write as much audio as will fit
			buf[1] |= UDP_FLAG_AUDIO;			// contains audio flag
			int audio_chunk_size = PACKET_SIZE - (4 + sizeof(IngexNetworkHeader));
			memcpy(&buf[4 + sizeof(IngexNetworkHeader)], audio, audio_chunk_size);
			audio += audio_chunk_size;
			remaining_audio -= audio_chunk_size;
		}
		else if (remaining_audio > 0) {
			// write remaining audio data
			buf[1] |= UDP_FLAG_AUDIO;			// contains audio flag
			int audio_chunk_size = PACKET_SIZE - 4;
			if (remaining_audio < audio_chunk_size)
				audio_chunk_size = remaining_audio;
			memcpy(&buf[4], audio, audio_chunk_size);
			audio += audio_chunk_size;
			remaining_audio -= audio_chunk_size;
		}
		else {
			// write video data
			buf[1] |= UDP_FLAG_VIDEO;			// contains video flag
			if (p_video_chunk - video + video_chunk_size > video_size)
				video_chunk_size = video_size - (p_video_chunk - video);
			memcpy(&buf[4], p_video_chunk, video_chunk_size);
			p_video_chunk += video_chunk_size;
		}

#ifdef DEBUG_UDP_SEND_RECV
		// We could use stat() and (((mode) & _S_IFMT) == _S_IFSOCK))
		ssize_t bytes_written = write(fd, buf, PACKET_SIZE);
		write(fd, "\n", 1);
#else
		ssize_t bytes_written = send(fd, buf, PACKET_SIZE, 0);
#endif
		if (bytes_written == -1) {
			perror("send");
			return 0;
		}
		total_bytes_written += bytes_written;

		// stop when we have written video_size video data
		if (p_video_chunk - video >= video_size)
			break;

		packet_num++;
	}
	free(buf);
	return total_bytes_written;
}

extern void scale_video420_for_multicast(int in_width, int in_height, int out_width, int out_height, const uint8_t *video_frame, uint8_t *scaled_frame)
{
#if TESTING_FLAG
	printf("Inside multicast_video: scale_video420_for_multicast\n");
#endif
	YUV_frame input_frame;
	YUV_frame output_frame;
	uint8_t workspace[2*in_width*4];

	input_frame.Y.w = in_width;
	input_frame.Y.h = in_height;
	input_frame.Y.lineStride = in_width;
	input_frame.Y.pixelStride = 1;
	input_frame.Y.buff = video_frame;
	input_frame.U.w = in_width/2;
	input_frame.U.h = in_height/2;
	input_frame.U.lineStride = in_width/2;
	input_frame.U.pixelStride = 1;
	input_frame.U.buff = video_frame + in_width*in_height;
	input_frame.V.w = in_width/2;
	input_frame.V.h = in_height/2;
	input_frame.V.lineStride = in_width/2;
	input_frame.V.pixelStride = 1;
	input_frame.V.buff = video_frame + in_width*in_height * 5/4;
	
	output_frame.Y.w = out_width;
	output_frame.Y.h = out_height;
	output_frame.Y.lineStride = out_width;
	output_frame.Y.pixelStride = 1;
	output_frame.Y.buff = scaled_frame;
	output_frame.U.w = out_width/2;
	output_frame.U.h = out_height/2;
	output_frame.U.lineStride = out_width/2;
	output_frame.U.pixelStride = 1;
	output_frame.U.buff = scaled_frame + out_width*out_height;
	output_frame.V.w = out_width/2;
	output_frame.V.h = out_height/2;
	output_frame.V.lineStride = out_width/2;
	output_frame.V.pixelStride = 1;
	output_frame.V.buff = scaled_frame + out_width*out_height * 5/4;

	scale_pic(	&input_frame, &output_frame,
				0, 0, output_frame.Y.w, output_frame.Y.h,
				0,				// turn off interlace since this is for monitoring
				1, 1,			// hfil, vfil
				workspace);
}

extern void scale_video422_for_multicast(int in_width, int in_height, int out_width, int out_height, const uint8_t *video_frame, uint8_t *scaled_frame)
{
#if TESTING_FLAG
	printf("Inside multicast_video: scale_video422_for_multicast\n");
#endif
	YUV_frame input_frame;
	YUV_frame output_frame;
	uint8_t workspace[2*in_width*4];

	input_frame.Y.w = in_width;
	input_frame.Y.h = in_height;
	input_frame.Y.lineStride = in_width;
	input_frame.Y.pixelStride = 1;
	input_frame.Y.buff = video_frame;
	input_frame.U.w = in_width/2;
	input_frame.U.h = in_height/2;
	input_frame.U.lineStride = in_width;
	input_frame.U.pixelStride = 1;
	input_frame.U.buff = video_frame + in_width*in_height;
	input_frame.V.w = in_width/2;
	input_frame.V.h = in_height/2;
	input_frame.V.lineStride = in_width;
	input_frame.V.pixelStride = 1;
	input_frame.V.buff = video_frame + in_width*in_height * 3/2;
	
	output_frame.Y.w = out_width;
	output_frame.Y.h = out_height;
	output_frame.Y.lineStride = out_width;
	output_frame.Y.pixelStride = 1;
	output_frame.Y.buff = scaled_frame;
	output_frame.U.w = out_width/2;
	output_frame.U.h = out_height/2;
	output_frame.U.lineStride = out_width/2;
	output_frame.U.pixelStride = 1;
	output_frame.U.buff = scaled_frame + out_width*out_height;
	output_frame.V.w = out_width/2;
	output_frame.V.h = out_height/2;
	output_frame.V.lineStride = out_width/2;
	output_frame.V.pixelStride = 1;
	output_frame.V.buff = scaled_frame + out_width*out_height * 5/4;

	scale_pic(	&input_frame, &output_frame,
				0, 0, output_frame.Y.w, output_frame.Y.h,
				0,				// turn off interlace since this is for monitoring
				1, 1,			// hfil, vfil
				workspace);
}
