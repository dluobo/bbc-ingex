#ifndef __MULTICAST_COMPRESSED_H__
#define __MULTICAST_COMPRESSED_H__

#ifdef __cplusplus
extern "C" 
{
#endif

typedef void mpegts_encoder_t;

extern mpegts_encoder_t *mpegts_encoder_init(const char *filename, int width, int height, uint32_t kbit_rate, int thread_count);
extern int mpegts_encoder_encode(mpegts_encoder_t *in_ts, uint8_t *p_video, int16_t *p_audio, int32_t frame_number);
extern int mpegts_encoder_close(mpegts_encoder_t *in_ts);

#ifdef __cplusplus
}
#endif

#endif
