/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* drvF3RP61Seq.c - Driver Support Routines for F3RP61 Sequence device
*
*      Author: Jun-ichi Odagiri
*      Date: 09-02-08
*/

//
#include <drvF3RP61Seq.h>

//
static long report();
static long init();

struct {
    long       number;
    DRVSUPFUN  report;
    DRVSUPFUN  init;
} drvF3RP61Seq = {
    2L,
    report,
    init,
};

epicsExportAddress(drvet, drvF3RP61Seq);

int f3rp61seqFd = -1;

//
void showreq(const iocshArgBuf *);
void stopshow(const iocshArgBuf *);
static const iocshFuncDef showreqDef = {"showreq", 0, NULL};
static const iocshFuncDef stopshowDef = {"stopshow", 0, NULL};

static int debug_flag;
static unsigned long request_id;

static void mcmd_thread(void *);
static void dump_mcmd_request(MCMD_STRUCT *);
static epicsMutexId f3rp61seq_queueMutex;
static epicsEventId f3rp61seq_queueEvent;
static ELLLIST f3rp61seq_queueList;

static F3RP61SEQ_DPVT *get_request_from_queue(void);

//////////////////////////////////////////////////////////////////////////
//
static long report(void)
{
    return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// Open and store file descriptor for CPU device access
//
static long init(void)
{
    static int init_flag = 0;
    if (init_flag) {
        return 0;
    }
    init_flag = 1;

    f3rp61seqFd = open(DEVFILE, O_RDWR);
    if (f3rp61seqFd < 0) {
        errlogPrintf("drvF3RP61Seq: can't open " DEVFILE "\n");
        return -1;
    }

    f3rp61seq_queueMutex = epicsMutexCreate();
    if (f3rp61seq_queueMutex == 0) {
        errlogPrintf("drvF3RP61Seq: epicsMutexCreate failed\n");
        return -1;
    }

    f3rp61seq_queueEvent = epicsEventCreate(epicsEventEmpty);
    if (f3rp61seq_queueEvent == 0) {
        errlogPrintf("drvF3RP61Seq: epicsEventCreate failed\n");
        return -1;
    }

    ellInit(&f3rp61seq_queueList);

    if (epicsThreadCreate("f3rp61seq_mcmd",
                          epicsThreadPriorityHigh,
                          epicsThreadGetStackSize(epicsThreadStackSmall),
                          (EPICSTHREADFUNC) mcmd_thread,
                          NULL) == 0) {
        errlogPrintf("drvF3RP61Seq: epicsThreadCreate failed\n");
        return -1;
    }

    iocshRegister(&showreqDef, showreq);
    iocshRegister(&stopshowDef, stopshow);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// Parses INP or OUT link and initializes F3RP61SEQ_DPVT structure.
//
// Returns: NORD on success (may smaller than NELM due to hardware constraints, such as relay number on the I/O module)
//          -1 on error
//
int f3rp61seqParseLink(const struct link *plink, F3RP61_RW rw, F3RP61_ACCESS_TYPE type, dbCommon *prec, const dbfType ftvl, const uint32_t nelm)
{
    const size_t size = strlen(plink->value.instio.string) + 1; // + 1 for terminating null character
    //char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    char buf[size];
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    // Clear dpvt so that subsequent failure of init_record() can be detected
    prec->dpvt = 0;

    // Parse conversion specifier
    char conv = 'W'; // default for Word access
    char *popt = strchr(buf, '&');
    if (popt) {
        *popt++ = '\0';
        if (sscanf(popt, "%c", &conv) < 1) {
            errlogPrintf("%s: %s : can't get conversion specifier\n", __func__, prec->name);
            return -1;
        }
    }

    // Check conversion specifier
    const char *ftvlstr = (pamapdbfType[ftvl].strvalue) + 4;
    if (f3rp61CheckConversion(type, ftvl, conv) < 0) {
        errlogPrintf("%s: %s : unsupported conversion specifier \'%c\' with FTVL field %s\n", __func__, prec->name, conv, ftvlstr);
        return -1;
    }

    // Parse for possible IO interrupt source
    //
    // Empty
    //

    // Parse slot, device and register number
    int8_t device = 0;
    int srcSlot = 0, destSlot = 0, addr = 0;
    if (sscanf(buf, "CPU%d,%c%d", &destSlot, &device, &addr) < 3) {
        errlogPrintf("%s: %s : can't get device address\n", __func__, prec->name);
        return -1;
    }

    // Check device validity
    switch (type) {
    case kBit:
        switch (device) {
        case 'X': // input relay // preliminary
            if (rw == kWrite) {
                errlogPrintf("%s: %s : write access to read-only device \'%c\'\n", __func__, prec->name, device);
                return -1;
            }
        case 'Y': // output relay // preliminary
        case 'I': // internal relay
        case 'M': // special relay
            break;
        default:
            errlogPrintf("%s: %s : unsupported device \'%c\'\n", __func__, prec->name, device);
            return -1;
        }
        break;
    default: // kWord
        switch (device) {
        case 'X': // input relay // preliminary
            if (rw == kWrite) {
                errlogPrintf("%s: %s : write access to read-only device \'%c\'\n", __func__, prec->name, device);
                return -1;
            }
        case 'Y': // output relay // preliminary
        case 'I': // internal relay
        case 'M': // special relay
        case 'D': // data register
        case 'B': // file register
        case 'F': // cache register
        case 'Z': // special register
            break;
        default:
            errlogPrintf("%s: %s : unsupported device \'%c\'\n", __func__, prec->name, device);
            return -1;
        }
        break;
    }

    // Read the slot number of this CPU module
    if (ioctl(f3rp61seqFd, M3CPU_GET_NUM, &srcSlot) < 0) {
        errlogPrintf("%s: %s : ioctl failed [%d]\n", __func__, prec->name, errno);
        return -1;
    }

    //debug
    //fprintf(stderr, "%s : %s : srcSlot=%d destSlot=%d device=%c pos=%d\n", __func__, prec->name, srcSlot, destSlot, device, addr);

    if (destSlot == srcSlot) {
        // Its better to use local device access API rather than sequence CPU device API (when local device access is supported by the device support)
        //errlogPrintf("%s: %s : Its recommended using \"F3RP61\" device type rather than \"F3RP61Seq\"\n", __func__, prec->name);

        // In order to access the local device on the F3RP61 itself as
        // a sequence CPU device, the command processing server must
        // be running. On F3RP7x, the command processing server should
        // already running, but just to be sure.
        static int mcmd_done = 0;
        if (! mcmd_done) {
            //debug
            //fprintf(stderr, "starting mcmdsrvmain\n");
            mcmdsrvmain(10);
            mcmd_done = 1;
        }
    }

    // Check address validity when accessing relays in byte-wise
    if (type == kWord &&
        (device == 'X' || device == 'Y')) {
        int32_t unit =  addr / 10000;
        int32_t slot = (addr % 10000) / 100;
        int32_t pos  =  addr % 100;
        if (pos%16 != 1) {
            errlogPrintf("%s: %s : Illegal relay number : %d\n", __func__, prec->name, pos);
            return -1;
        }
    }

    // Consider I/O data length
    const int width = 2; // We don't use long-word access, so width is fixed to 2
    int num = 1;
    if (conv == 'D') {
        num = 4;
    } else if (conv == 'F' ||conv == 'L') {
        num = 2;
    }

    //
    uint32_t nord = nelm;

    // Allocate private data storage area
    F3RP61SEQ_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61SEQ_DPVT), "calloc failed");
    prec->dpvt = dpvt;
    dpvt->nord = nord;
    dpvt->conv = conv;

    // Compose data structure for I/O request to CPU module
    MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
    pmcmdStruct->timeOut = 1;

    MCMD_REQUEST *pmcmdRequest = &pmcmdStruct->mcmdRequest;
    pmcmdRequest->formatCode = 0xf1;
    pmcmdRequest->responseOption = 1;
    pmcmdRequest->srcSlot = srcSlot;
    pmcmdRequest->destSlot = destSlot;
    pmcmdRequest->mainCode = 0x26;
    pmcmdRequest->subCode = rw;

    if (rw == kRead) {
        M3_READ_SEQDEV *pM3ReadSeqdev = (M3_READ_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];
        pM3ReadSeqdev->accessType = type;
        pM3ReadSeqdev->dataNum = num;
        pM3ReadSeqdev->devType = device - '@'; // 'D'=>0x04, 'B'=>0x02, 'F'=>0x06, 'Z'=>0x1A, 'I'=>0x09
        pM3ReadSeqdev->topDevNo = addr;
        pmcmdRequest->dataSize = 10;
    } else {
        M3_WRITE_SEQDEV *pM3WriteSeqdev = (M3_WRITE_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];
        pM3WriteSeqdev->accessType = type;
        pM3WriteSeqdev->dataNum = num;
        pM3WriteSeqdev->devType = device - '@'; // 'D'=>0x04, 'B'=>0x02, 'F'=>0x06, 'Z'=>0x1A, 'I'=>0x09
        pM3WriteSeqdev->topDevNo = addr;
        pmcmdRequest->dataSize = 10 + num * width;
    }

    //
    callbackSetUser(prec, &dpvt->callback);

    // success
    return nord;
}

//////////////////////////////////////////////////////////////////////////
//
int8_t f3rp61seqGetDevice(F3RP61SEQ_DPVT *dpvt)
{
    MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
    MCMD_REQUEST *pmcmdRequest = &pmcmdStruct->mcmdRequest;
    M3_READ_SEQDEV *pM3ReadSeqdev = (M3_READ_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];
    int8_t device = pM3ReadSeqdev->devType + '@';
    return device;
}

//////////////////////////////////////////////////////////////////////////
//
static void mcmd_thread(void *arg)
{
    for (;;) {
        epicsEventMustWait(f3rp61seq_queueEvent);

        F3RP61SEQ_DPVT *dpvt;
        while ((dpvt = get_request_from_queue())) {
            dpvt->ret = 0;
            MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
            pmcmdStruct->mcmdRequest.comId = ++request_id;

            if (debug_flag) {
                dump_mcmd_request(pmcmdStruct);
            }

            CALLBACK *pcallback = &dpvt->callback;
            dbCommon *prec;
            callbackGetUser(prec, pcallback);

            if (ioctl(f3rp61seqFd, M3CPU_ACCS_CMD, pmcmdStruct) < 0) {
                MCMD_RESPONSE *pmcmdResponse = &pmcmdStruct->mcmdResponse;
                uint16_t errorCode = pmcmdResponse->errorCode;
                if (errno == EIO) {
                    errlogPrintf("drvF3RP61Seq: %s : ioctl failed [%d] : %s : errorCode 0x%04x\n", prec->name, errno, strerror(errno), errorCode);
                } else {
                    errlogPrintf("drvF3RP61Seq: %s : ioctl failed [%d] : %s\n", prec->name, errno, strerror(errno));
                }
                dpvt->ret = -1;
            } else if (pmcmdStruct->mcmdResponse.comId != request_id) {
                errlogPrintf("drvF3RP61Seq: %s : comId does not match : expected=0x%08lx received=0x%08lx\n", prec->name, request_id, pmcmdStruct->mcmdResponse.comId);
                dpvt->ret = -1;
            }

            callbackRequestProcessCallback(pcallback, priorityLow, prec);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//
int f3rp61seqQueueRequest(F3RP61SEQ_DPVT *dpvt)
{
    if (!dpvt) {
        errlogPrintf("drvF3RP61Seq: null request\n");
        return -1;
    }

    epicsMutexMustLock(f3rp61seq_queueMutex);
    ellAdd(&f3rp61seq_queueList, &dpvt->node);
    epicsMutexUnlock(f3rp61seq_queueMutex);

    epicsEventSignal(f3rp61seq_queueEvent);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//
static F3RP61SEQ_DPVT *get_request_from_queue(void)
{
    epicsMutexMustLock(f3rp61seq_queueMutex);
    F3RP61SEQ_DPVT *dpvt = (F3RP61SEQ_DPVT *) ellGet(&f3rp61seq_queueList);
    epicsMutexUnlock(f3rp61seq_queueMutex);

    return dpvt;
}

//////////////////////////////////////////////////////////////////////////
//
static void dump_mcmd_request(MCMD_STRUCT *pmcmdStruct)
{
    MCMD_REQUEST *pmcmdRequest = &pmcmdStruct->mcmdRequest;
    M3_WRITE_SEQDEV *pM3WriteSeqdev = (M3_WRITE_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];

    printf("\n");
    printf("formatCode    : 0x%02x\n", pmcmdRequest->formatCode);
    printf("responseOption: %d\n",     pmcmdRequest->responseOption);
    printf("srcSlot       : %d\n",     pmcmdRequest->srcSlot);
    printf("destSlot      : %d\n",     pmcmdRequest->destSlot);
    printf("mainCode      : 0x%02x\n", pmcmdRequest->mainCode);
    printf("subCode       : 0x%02x\n", pmcmdRequest->subCode);
    printf("dataSize      : %d\n",     pmcmdRequest->dataSize);
    printf("accessType    : %d\n",     pM3WriteSeqdev->accessType);
    printf("devType       : 0x%02x\n", pM3WriteSeqdev->devType);
    printf("dataNum       : %d\n",     pM3WriteSeqdev->dataNum);
    printf("topDevNo      : %ld\n",    pM3WriteSeqdev->topDevNo);
    printf("\n");
}

//////////////////////////////////////////////////////////////////////////
//
void showreq(const iocshArgBuf *args)
{
    debug_flag = 1;
}

//////////////////////////////////////////////////////////////////////////
//
void stopshow(const iocshArgBuf *args)
{
    debug_flag = 0;
}
