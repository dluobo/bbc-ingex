#ifndef _DVS_CLIB_H
#define _DVS_CLIB_H 

#ifdef __cplusplus
extern "C" {
#endif


#define uint32 unsigned int

typedef struct {
  int magic;
  int size;
  int version;
  int vgui;
  int prontovideo;
  int debug;
  int pad[10];

} sv_handle;

typedef struct {
  union {
    unsigned int version;
    struct {
      unsigned char patch;
      unsigned char fix;
      unsigned char major;
      unsigned char minor;
    } v;
  } release;
  int flags;
  int rbeta;
  union {
    int date;
    struct {
      short yyyy;
      char mm;
      char dd;
    } d;
  } date;
  union {
    int time;
    struct {
      char nn;
      char hh;
      char mm;
      char ss;
    } t;
  } time;
  int devicecount;
  int modulecount;
  char comment[64];
  char module[32];
  char res1[20];
} sv_version;

typedef struct {
  int size;
  int version;
  int config;
  int colormode;
  int xsize;
  int ysize;
  int sync;
  int nbit;

  struct {
    int timecode;
    int info;
    int error;
    int preset;
    int preroll;
    int postroll;
    int editlag;
    int parktime;
    int autopark;
    int timecodetype;
    int tolerance;
    int forcedropframe;
    int device;
    int inpoint;
    int nframes;
    int recoffset;
    int disoffset;
    int framerate;
    int debug;
    int flags;
    int infobits[4];
  } master;

  struct {
    int ramsize;
    int disksize;
    int nframes;
    int genlock;
    int ccube;
    int audio;
    int preset;
    int key;
    int mono;
    int compression;
    int rgbm;
    int flags;
    int licence;
    int storagexsize;
    int storageysize;
    int pad[1];
  } setup;

  struct {
    int position;
    int inpoint;
    int outpoint;
    int speed;
    int speedbase;
    int slavemode;
    int state;
    int error;
    int sofdelay;
    int iomode;
    int clip_updateflag;
    int tl_position;
    int supervisorflag;
    int framerate_mHz;
    int positiontc;
    int flags;
  } video;
} sv_info;

typedef struct {
  int size;
  int version;
  int cookie;
  int pad1[5];

  int alignment;
  int bigendian;
  int buffersize;
  int components;
  int dominance21;
  int dropframe;
  int fps;
  int fullrange;
  int interlace;
  int nbits;
  int rgbformat;
  int subsample;
  int xsize;
  int ysize;
  int yuvmatrix;
  int colormode;
  int videomode;
  int vinterlace;
  int vfps;
  int filmmaterial;
  int nbit10type;
  int bottom2top;
  int nbittype;

  int pixelsize_num;
  int pixelsize_denom;
  int pixeloffset_num[8];
  int dataoffset_num[8];
  int linesize;
  int lineoffset[2];
  int fieldoffset[2];
  int fieldsize[2];
  int storagexsize;
  int storageysize;
  int pad3[14];

  int abits;
  int abigendian;
  int aunsigned;
  int afrequency;
  int achannels;
  int aoffset[2];
  int pad4[8];
} sv_storageinfo;

typedef struct {
  int size;
  int tick;
  int altc_tc;
  int altc_ub;
  int altc_received;
  int avitc_tc[2];
  int avitc_ub[2];
  int afilm_tc[2];
  int afilm_ub[2];
  int aprod_tc[2];
  int aprod_ub[2];
  int vtr_tc;
  int vtr_ub;
  int vtr_received;
  int dltc_tc;
  int dltc_ub;
  int dvitc_tc[2];
  int dvitc_ub[2];
  int dfilm_tc[2];
  int dfilm_ub[2];
  int dprod_tc[2];
  int dprod_ub[2];
  int pad[32];
} sv_timecode_info;

#define TRUE 1
#define FALSE 0

#define SV_OPENPROGRAM_TESTPROGRAM 0x00000003
#define SV_OPENPROGRAM_DEMOPROGRAM 0x00000004
#define SV_OPENPROGRAM_APPLICATION 0x01000000

#define SV_OPENTYPE_DEFAULT 0
#define SV_OPENTYPE_INPUT 0x0000000c
#define SV_OPENTYPE_QUERY 0x00040000

#define SV_MODE_MASK 0x00001FFF
#define SV_MODE_PAL 0x00
#define SV_MODE_NTSC 0x01

#define SV_MODE_SMPTE274_25I 0x28
#define SV_MODE_SMPTE274_30I 0x22
#define SV_MODE_SMPTE296_50P 0x66
#define SV_MODE_SMPTE296_60P 0x23
#define SV_MODE_COLOR_YUV422 0x00000000
#define SV_MODE_STORAGE_FRAME 0x40000000
#define SV_MODE_AUDIO_MASK 0x00e00000
#define SV_MODE_AUDIO_4CHANNEL 0x00c00000

#define SV_SYNC_INTERNAL 0
#define SV_SYNC_EXTERNAL 1
#define SV_SYNC_GENLOCK_ANALOG 2
#define SV_SYNC_GENLOCK_DIGITAL 3
#define SV_SYNC_SLAVE 4
#define SV_SYNC_AUTO 5
#define SV_SYNC_MODULE 6
#define SV_SYNC_BILEVEL 7
#define SV_SYNC_TRILEVEL 8
#define SV_SYNC_HVTTL 9
#define SV_SYNC_LTC 10
#define SV_SYNC_MASK 0xffff

#define SV_QUERY_TEMPERATURE 87
#define SV_QUERY_AUDIOINERROR 94
#define SV_QUERY_VIDEOINERROR 95
#define SV_QUERY_VALIDTIMECODE 134
#define SV_QUERY_INPUTRASTER_SDIA 135

#define SV_OPTION_AUDIOINPUT 48
#define SV_AUDIOINPUT_AESEBU 1

#define SV_CURRENTTIME_CURRENT 0

#define SV_VALIDTIMECODE_LTC 0x000001
#define SV_VALIDTIMECODE_DLTC 0x000002
#define SV_VALIDTIMECODE_VITC_F1 0x000100
#define SV_VALIDTIMECODE_DVITC_F1 0x000200
#define SV_VALIDTIMECODE_VITC_F2 0x010000
#define SV_VALIDTIMECODE_DVITC_F2 0x020000

enum {
SV_OK = 0,
SV_ERROR_INPUT_VIDEO_NOSIGNAL = 112,
SV_ERROR_INPUT_AUDIO_NOAESEBU = 116,
SV_ERROR_INPUT_AUDIO_NOAIV = 120,
SV_ERROR_WRONGMODE = 151,
SV_ERROR_SVOPENSTRING = 160,
SV_ERROR_DEVICENOTFOUND = 162
};

int sv_query(sv_handle * sv, int cmd, int par, int *val);
int sv_option_set(sv_handle * sv, int code, int val);
int sv_option_get(sv_handle * sv, int code, int *val);
int sv_currenttime(sv_handle * sv, int brecord, int *ptick, uint32 *pclockhigh, uint32 *pclocklow);
int sv_version_status( sv_handle * sv, sv_version * version, int versionsize, int deviceid, int moduleid, int spare);
int sv_videomode(sv_handle * sv, int videomode);
int sv_sync(sv_handle * sv, int sync);
int sv_openex(sv_handle ** psv, char * setup, int openprogram, int opentype, int timeout, int spare);
int sv_close(sv_handle * sv);
char * sv_geterrortext(int code);
int sv_status(sv_handle * sv, sv_info * info);
int sv_storage_status(sv_handle *sv, int cookie, sv_storageinfo * psiin, sv_storageinfo * psiout, int psioutsize, int flags);
int sv_stop(sv_handle * sv);
void sv_errorprint(sv_handle * sv, int code);
int sv_timecode_feedback(sv_handle * sv, sv_timecode_info* input, sv_timecode_info* output);

#ifdef __cplusplus
}
#endif

#endif
