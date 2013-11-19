
typedef struct {
  ELLNODE           node;
  MCMD_STRUCT       mcmdStruct;
  dbCommon         *prec;
  CALLBACK          callback;
  int               ret;
} F3RP61_SEQ_DPVT;


long f3rp61Seq_queueRequest();

