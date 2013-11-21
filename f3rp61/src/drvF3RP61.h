#define NUM_IO_INTR  8

typedef struct {
  int channel;
  dbCommon *prec;
} F3RP61_IO_SCAN;

typedef struct {
  F3RP61_IO_SCAN ioscan[NUM_IO_INTR];
  int count;
} F3RP61_IO_INTR;

typedef struct {
  long mtype;
  M3IO_IO_EVENT mtext;
} MSG_BUF;


long f3rp61GetIoIntInfo(int, dbCommon *, IOSCANPVT *);
long f3rp61_register_io_interrupt(dbCommon *, int, int, int);
