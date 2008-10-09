#ifndef _DVS_FIFO_H
#define _DVS_FIFO_H 

#ifdef __cplusplus
extern "C" {
#endif

typedef void * sv_fifo;

typedef struct {
  int version;
  int size;
  int id;
  int flags;
  struct {
    char * addr;
    int size;
  } dma;
  struct {
    char * addr;
    int size;
  } video[2];
  struct {
    char * addr[4];
    int size;
  } audio[2];
  struct {
    int ltc_tc;
    int ltc_ub;
    int vitc_tc;
    int vitc_ub;
    int vtr_tick;
    int vtr_tc;
    int vtr_ub;
    int vtr_info3;
    int vtr_info;
    int vtr_info2;
    int vitc_tc2;
    int vitc_ub2;
    int pad[4];
  } timecode;
  struct {
    int tick;
    int clock_high;
    int clock_low;
    int gpi;
    int aclock_high;
    int aclock_low;
    int pad[2];
  } control;
  struct {
    int cmd;
    int length;
    unsigned char data[16];
  } vtrcmd;
  struct {
    char * addr[4];
  } audio2[2];
  struct {
    int storagemode;
    int xsize;
    int ysize;
    int xoffset;
    int yoffset;
    int dataoffset;
    int lineoffset;
    int pad[8];
  } storage;
  struct {
    int dvitc_tc[2];
    int dvitc_ub[2];
    int film_tc[2];
    int film_ub[2];
    int prod_tc[2];
    int prod_ub[2];
    int dltc_tc;
    int dltc_ub;
    int closedcaption[2];
    int afilm_tc[2];
    int afilm_ub[2];
    int aprod_tc[2];
    int aprod_ub[2];
  } anctimecode;
  int pad[32];
} sv_fifo_buffer;

typedef struct {
  int nbuffers;
  int availbuffers;
  int tick;
  int clock_high;
  int clock_low;
  int dropped;
  int clocknow_high;
  int clocknow_low;
  int waitdropped;
  int waitnotintime;
  int audioinerror;
  int videoinerror;
  int displaytick;
  int recordtick;
  int openprogram;
  int opentick;
  int pad26[8];
} sv_fifo_info;

typedef struct {
  int version;
  int size;
  int when;
  int clock_high;
  int clock_low;
  int clock_tolerance;
  int padding[32-6];
} sv_fifo_bufferinfo;

#define SV_FIFO_FLAG_FLUSH      0x000010
#define SV_FIFO_FLAG_NO_LIVE    0x800000

int sv_fifo_init(sv_handle * sv, sv_fifo ** ppfifo, int bInput, int bShared, int bDMA, int flagbase, int nframes);
int sv_fifo_free(sv_handle * sv, sv_fifo * pfifo);
int sv_fifo_start(sv_handle * sv, sv_fifo * pfifo);
int sv_fifo_getbuffer(sv_handle * sv, sv_fifo * pfifo, sv_fifo_buffer ** pbuffer, sv_fifo_bufferinfo * bufferinfo, int flags);
int sv_fifo_putbuffer(sv_handle * sv, sv_fifo * pfifo, sv_fifo_buffer * pbuffer, sv_fifo_bufferinfo * bufferinfo);
int sv_fifo_stop(sv_handle * sv, sv_fifo * pfifo, int flags);
int sv_fifo_status(sv_handle * sv, sv_fifo * pfifo, sv_fifo_info * pinfo);
int sv_fifo_reset(sv_handle * sv, sv_fifo * pfifo);

#ifdef __cplusplus
}
#endif

#endif
