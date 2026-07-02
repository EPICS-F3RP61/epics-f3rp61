#ifndef DEVF3RP61UTIL_H
#define DEVF3RP61UTIL_H

//
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

//
#include <alarm.h>
#include <dbCommon.h>
#include <dbFldTypes.h>
#include <errlog.h>
#include <recGbl.h>

//
typedef enum {
    // These value are defined appropriately for as value of the
    // subCode member of the MCMD_REQUEST structure for F3RP61Seq
    // device, but no significanse for F3RP61 device.
    kRead  = 0x01,
    kWrite = 0x02,
} F3RP61_RW;

// Access type for sequence CPU device
typedef enum {
    // These value are defined appropriately for as value of the
    // accessType member of the M3_READ_SEQDEV and the M3_WRITE_SEQDEV
    // structure for F3RP61Seq device, but no significanse for F3RP61
    // device.
    kBit   = 0x00,
    kWord  = 0x02,
    // kLong = 0x04, // F3RP71 native API does not suport long-word access
} F3RP61_ACCESS_TYPE;

//
int f3rp61CheckConversion(F3RP61_ACCESS_TYPE, const dbfType, const char);

//
int32_t f3rp61CheckAddrRange(dbCommon *prec, F3RP61_ACCESS_TYPE type, const int addr, int32_t count, const int32_t nelm, const int limit);

//
int32_t  devF3RP61buf2long(void *buf, int32_t *val, const int8_t conv, int32_t nord);
int32_t  devF3RP61long2buf(int32_t *val, void *buf, const int8_t conv, int32_t nord);

//
int32_t  devF3RP61buf2ulong(void *buf, uint32_t *val, const int8_t conv, int32_t nord);
int32_t  devF3RP61ulong2buf(uint32_t *val, void *buf, const int8_t conv, int32_t nord);

//
int32_t  devF3RP61buf2short(void *buf, int16_t *val, const int8_t conv, int32_t nord);
int32_t  devF3RP61short2buf(int16_t *val, void *buf, const int8_t conv, int32_t nord);

//
int32_t  devF3RP61buf2double(void *buf, double *val, const int8_t conv, int32_t nord);
int32_t  devF3RP61double2buf(double *val, void *buf, const int8_t conv, int32_t nord);

//
int32_t  devF3RP61buf2float(void *buf, float *val, const int8_t conv, int32_t nord);
int32_t  devF3RP61float2buf(float *val, void *buf, const int8_t conv, int32_t nord);

#endif
