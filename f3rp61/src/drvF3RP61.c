/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* drvF3RP61.c - Driver Support Routines for F3RP61
*
*      Author: Jun-ichi Odagiri
*      Date: 6-30-08
*/

//
#include <drvF3RP61.h>

#if defined(__powerpc__)
#define S_m3dev_INVALID_NUMBER     S_m3data_INVALID_NUMBER      // 392
#define S_m3dev_DEVICE_NOT_FOUND   S_m3data_DEVICE_NOT_FOUND    // 393
#define S_m3dev_DEVICE_ENTRY_ERROR S_m3data_DEVICE_ENTRY_ERROR  // 394
#endif

//
#define M3IO_NUM_CPUS   4

// A single F3RP61/71 module can work with up to two FL-net interface modules.
#define M3IO_NUM_LINKS  2

// Interrupts are supported by input modules up to 32 channels; 64-channel modules do not support interrupt.
#define NUM_IRQ_CH  64 // 32?

static IOSCANPVT ioscanpvt[M3IO_NUM_UNIT][M3IO_NUM_SLOT][NUM_IRQ_CH] = {{{0}}};

//
typedef enum {
    IRQ_FD      = 1, // use device file
    IRQ_MSQ     = 0, // use SysV message queue
} F3RP61_IRQ_INTERFACE;

static F3RP61_IRQ_INTERFACE irq_interface = IRQ_FD;

//
typedef struct {
    long mtype;
#if defined(__arm__)
    M3IO_MSG_IO mtext;
#elif defined(__powerpc__)
    M3IO_IO_EVENT mtext;
#else
#  error
#endif
} MSG_BUF;

//
static long report();
static long init();

struct {
    long      number;
    DRVSUPFUN report;
    DRVSUPFUN init;
} drvF3RP61 = {
    2L,
    report,
    init,
};

epicsExportAddress(drvet, drvF3RP61);

int f3rp61_fd = -1;

// I/O interrupt handlers
static void msgrcv_thread(void *);
static void read_thread(void *);

// helper fuction(s)
static int f3rp61RegisterIoInterrupt(const dbCommon *, int, int, int);
static int f3rp61EnableIoInterrupt(void);

// iocsh commands

//
static void getModuleInfoCallFunc(const iocshArgBuf *);
static int  getModuleInfo(int);

//
static void getInternalDeviceConfigCallFunc(const iocshArgBuf *);
static int  getInternalDeviceConfig(void);

static int rly_size;
static int reg_size;
static int local_loc;
static void setInternalDeviceConfigCallFunc(const iocshArgBuf *);
static int  setInternalDeviceConfig(int, int, int);

//
static void getSharedDeviceConfigCallFunc(const iocshArgBuf *);
static int  getSharedDeviceConfig(void);

//
static M3COMDATACONFIG com_data_config;
static M3COMDATACONFIG ext_com_data_config;
static void setSharedDeviceConfigCallFunc(const iocshArgBuf *);
static int  setSharedDeviceConfig(int, int, int, int, int);

//
static void getLinkDeviceConfigCallFunc(const iocshArgBuf *);
static int  getLinkDeviceConfig(void);

//
static M3LINKDATACONFIG link_data_config;
static void setLinkDeviceConfigCallFunc(const iocshArgBuf *);
static int  setLinkDeviceConfig(int, int, int);

//
static void drvF3RP61RegisterCommands(void);

//////////////////////////////////////////////////////////////////////////
//
static long report(void)
{
    return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// Open and store file descriptor for I/O module access
//
static long init()
{
    //debug
    //printf("%s:%s\n", __FILE__, __func__);
    //printf("io_irq:  %zu\n", sizeof(io_irq));

    static int init_flag = 0;
    if (init_flag) {
        return 0;
    }
    init_flag = 1;

    f3rp61_fd = open("/dev/m3io", O_RDWR);
    if (f3rp61_fd < 0) {
        errlogPrintf("drvF3RP61: can't open /dev/m3io [%d] : %s\n", errno, strerror(errno));
        return -1;
    }

    // Set internal device (local device) assingment
    {
        // Shall we put setM3InternalDataTable() here?
        //setM3InternalDataTable(loc, nrly, nreg);
    }

    // Set shared device assignment
    for (int i = 0; i < M3IO_NUM_CPUS; i++) {
        if (com_data_config.wNumberOfRelay[i] || com_data_config.wNumberOfRegister[i] ||
            ext_com_data_config.wNumberOfRelay[i] || ext_com_data_config.wNumberOfRegister[i]) {
            if (setM3ComDataConfig(&com_data_config, &ext_com_data_config) < 0) {
                errlogPrintf("drvF3RP61: setM3ComDataConfig failed [%d]\n", errno);
                // 414 (invalid number)  : invalid parameter was specified (F3RP7x/61)
                return -1;
            }

            break;
        }
    }

    // Set link device assignment
    for (int i = 0; i < M3IO_NUM_LINKS; i++) {
        if (link_data_config.wNumberOfRelay[i] || link_data_config.wNumberOfRegister[i]) {

            if (setM3LinkDeviceConfig(&link_data_config) < 0) {
                errlogPrintf("drvF3RP61: setM3LinkDeviceConfig failed [%d]\n", errno);
                return -1;
            }

            if (setM3FlnSysNo(0, NULL) < 0) {
                errlogPrintf("drvF3RP61: setM3FlnSysNo failed [%d]\n", errno);
                // 414 (invalid number)  : invalid parameter was specified (F3RP7x/61)
                // 415 (device mismatch) : specified modules is not FL-net (F3RP7x)
                // 416 (number over)     : an excessive number of system is specified (F3RP7x)
                // 417 (entry error)     : unable to access module or I/O bus error (F3RP7x)
                // 397 (internal error)  : (F3RP61)
                // 394                   : Not documented in the manual; setM3FlnSysNo() was called after m3rfrsTsk(). We'd better to reset the CPU.
                return -1;
            }

            if (m3rfrsTsk(10) < 0) {
                errlogPrintf("drvF3RP61: m3rfrsTsk failed [%d]\n", errno);
                return -1;
            }

            break;
        }
    }

    // check interface for the input-relay IRQ
    struct utsname buf;
    if (uname(&buf) != 0) {
        errlogPrintf("drvF3RP61: uname failed [%d]\n", errno);
        return -1;
    }

    int major, patch, sub;
    if (sscanf(buf.release, "%d.%d.%d", &major, &patch, &sub) != 3) {
        errlogPrintf("drvF3RP61: Invalid kernel release format: %s\n", buf.release);
        return -1;
    }

    const int kver = KERNEL_VERSION(major, patch, sub);
    if (kver<KERNEL_VERSION(6, 1, 120)) {
        //
        irq_interface = IRQ_MSQ;

        //debug
        //errlogPrintf("drvF3RP61: Use SysV message queue for I/O interrupt\n");

    } else {
        //
        irq_interface = IRQ_FD;

        //debug
        //errlogPrintf("drvF3RP61: Use read() for I/O interrupt\n");
    }

    //
    return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// Thread for I/O interrupt handling (for kernel < 6.1.120)
// When an interrupt occurs, a SysV message is queued.
// This thread waits for the message and process the PV corresponding to
// the interrupt source described in the message.
//
static void msgrcv_thread(void *arg)
{
    int msqid = (int) arg;

    //debug
    //fprintf(stderr, "%s:%s %d\n", __FILE__, __func__, msqid);

    for (;;) {
        MSG_BUF msgbuf;
        const ssize_t size = msgrcv(msqid, &msgbuf, sizeof(MSG_BUF), M3IO_MSGTYPE_IO, MSG_NOERROR);

        if (size == -1) {
            errlogPrintf("drvF3RP61: msgrcv failed [%d] : %s\n", errno, strerror(errno));
            // msgbuf might be uninitialized if msgrcv() failed.
            continue;
        }

        if (size < sizeof(msgbuf.mtext)) {
            // for just in case
            errlogPrintf("drvF3RP61: message received by msgrcv() is too small (%zu bytes)\n", size);
            continue;
        }

        //const long mtype  = msgbuf.mtype;
        const int unit    = msgbuf.mtext.unit;
        const int slot    = msgbuf.mtext.slot;
        const int channel = msgbuf.mtext.channel;

        // Note that slot# and channel# starts from 1
        IOSCANPVT pvt = ioscanpvt[unit][slot-1][channel-1];

        // debug
        //printf("%s:%s U%d,S%d,X%02d %p\n", __FILE__, __func__, unit, slot, channel, pvt);

        if (! pvt) {
            // this may not happen, as previously enabled I/O interrupt must has been cleared.
            errlogPrintf("drvF3RP61: no record for I/O interrupt (U%d,S%d,X%d). Previously enable interrupt has not been cleared.\n", unit, slot, channel);
            continue; // just ignore
        }

        //
        scanIoRequest(pvt);
    }
}

//////////////////////////////////////////////////////////////////////////
//
// Thread for I/O interrupt handling (for kernel >= 6.1.120)
// Reading from the device file is blocked until an interrupt occures.
// When an interrupt occurs, its source information is read and
// process the PV corresponding to the interrupt source described in
// the message.
//
static void read_thread(void *arg)
{
    int fd = (int) arg;

    //debug
    //fprintf(stderr, "%s:%s %d\n", __FILE__, __func__, fd);

    for (;;) {
        struct read_buffer {
            int qid;
            short unit;
            short slot;
            uint16_t bits1;
            uint16_t bits2;
            // read() may return interrupt information for 64 channels, but only 32 channels are valid.
            //uint16_t bits3;
            //uint16_t bits4;
        } buf;

        ssize_t size = read(fd, &buf, sizeof(buf));
        if (size < 0) {
            errlogPrintf("drvF3RP61: read failed [%d] : %s\n", errno, strerror(errno));
            continue;
        }

        const int unit = buf.unit;
        const int slot = buf.slot;
        uint32_t bits  = (buf.bits2<<16) | buf.bits1;

        // debug
        //fprintf(stderr, "size=%d, qid=%d, unit=%d, slot=%d, di=0x%08x\n", size, buf.qid, unit, slot, bits);

        for (int channel=1; channel<=8*sizeof(bits); channel++) {
            const int enabled = (bits>>(channel-1)) & 0x01;

            // Note that slot# and channel# starts from 1
            IOSCANPVT pvt = ioscanpvt[unit][slot-1][channel-1];

            if (!enabled) {
                continue;
            }

            // debug
            //fprintf(stderr, "%s:%s U%d,S%d,X%02d %p %d\n", __FILE__, __func__, unit, slot, channel, pvt, enabled);

            if (! pvt) {
                // this may not happen, as previously enabled I/O interrupt must has been cleared.
                errlogPrintf("drvF3RP61: no record for I/O interrupt (U%d,S%d,X%d). Previously enable interrupt has not been cleared.\n", unit, slot, channel);
                continue; // just ignore
            }

            //
            scanIoRequest(pvt);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//
// Initialize device support in 2 pass.
// - after==0 : 1st pass: befor initDatabase(), in which calls init_record() for each record instances.
// - after==0 : 2nd pass: after initDatabase()
//
long f3rp61Init(int after)
{
    //debug
    //printf("%s:%s %d\n", __FILE__, __func__, after);

    //
    if (after) {
        f3rp61EnableIoInterrupt();
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// Parses INP or OUT link and initializes F3RP61_DPVT structure.
// Registeres IO as well, if specified.
//
int f3rp61ParseLink(const struct link *plink, F3RP61_DPVT *dpvt, const dbCommon *prec, const char *sup)
{
    const size_t size = strlen(plink->value.instio.string) + 1; // + 1 for terminating null character
    //char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    char buf[size];
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    // Parse conversion specifier
    dpvt->conv = 'W'; // default conversion for Word access
    char *popt = strchr(buf, '&');
    if (popt) {
        *popt++ = '\0';
        if (sscanf(popt, "%c", &dpvt->conv) < 1) {
            errlogPrintf("%s: %s : can't get conversion specifier\n", sup, prec->name);
            return -1;
        }
    }

    // Parse for possible IO interrupt source
    dpvt->irqunit = 0;
    dpvt->irqslot = 0;
    dpvt->irqaddr = 0;
    char *pint = strchr(buf, ':'); // check if SCAN is interrupt based (example: @U0,S3,Y1:U0,S4,X1)
    if (pint) {
        *pint++ = '\0';

        int unit = 0, slot = 0, addr = 0;
        if (0) {
            //
        } else if (sscanf(pint, "U%d,S%d,X%d", &unit, &slot, &addr) == 3) {
            //
        } else if (sscanf(pint, "S%d,X%d", &slot, &addr) == 2) {
            //
        } else if (sscanf(pint, "X%d", &addr) == 1) {
            //
            unit =  addr / 10000;
            slot = (addr % 10000) / 100;
            addr =  addr % 100;
        } else {
            errlogPrintf("%s: %s : can't get interrupt source address\n", sup, prec->name);
            return -1;
        }

        if (unit<0  || unit>=M3IO_NUM_UNIT || // unit : 0,2,..., 7
            slot<=0 || slot>M3IO_NUM_SLOT  || // slot : 1,2,...,16
            addr<=0 || addr>NUM_IRQ_CH    ) { // addr : 1,2,...,64 (or 32)
            errlogPrintf("%s: %s : Invalid interrupt source : U%d,S%d,X%d\n", sup, prec->name, unit, slot, addr);
            return -1;
        }

        // Register IO Interrupt
        dpvt->irqunit = unit;
        dpvt->irqslot = slot;
        dpvt->irqaddr = addr;
        if (f3rp61RegisterIoInterrupt(prec, unit, slot, addr) < 0) {
            errlogPrintf("%s: %s : can't register I/O interrupt\n", sup, prec->name);
            return -1;
        }
    }

    // Parse slot, device and register number
    uint8_t device = 0;
    uint32_t unit = 0, slot = 0, addr = 0;
    uint32_t cpuno = 0; // for Shared memory (or 'Old interface' for shared registers/relays)
    if (0) {
        //
    } else if (sscanf(buf, "CPU%d,R%d", &cpuno, &addr) == 2) {
        device = 'r'; // Shared memory (or 'Old interface' for shared registers/relays)
    } else if (sscanf(buf, "U%d,S%d,%c%d", &unit, &slot, &device, &addr) == 4) {
        //
    } else if (sscanf(buf, "S%d,%c%d", &slot, &device, &addr) == 3) {
        //
    } else if (sscanf(buf, "%c%d", &device, &addr) == 2) {
        if (0) {
        } else if (device == 'X' || device == 'Y' || // Input and output relays on I/O modules
                   device == 'M') {                  // Mode registers on I/O modules
            unit =  addr / 10000;
            slot = (addr % 10000) / 100;
            addr =  addr % 100;
        } else if (device == 'A') { // Address for 'A' may exceed 1000
            errlogPrintf("%s: %s : Invalid device : %s\n", sup, prec->name, buf);
            return -1;
        }
    } else {
        errlogPrintf("%s: %s : can't get I/O address\n", sup, prec->name);
        return -1;
    }

    //
    dpvt->device = device;
    dpvt->unit   = unit;
    dpvt->slot   = slot;
    dpvt->addr   = addr;

    // Consider I/O data length
    dpvt->count = 1;
    if (dpvt->conv == 'F' || dpvt->conv == 'L') {
        dpvt->count = 2;
    } else if (dpvt->conv == 'D') {
        dpvt->count = 4;
    }

    // debug
    //printf("%s:%s %s U%d,S%d%c%d,U%d,S%d,X%d\n", __FILE__, __func__, prec->name, dpvt->unit, dpvt->slot, dpvt->device, dpvt->addr, dpvt->irqunit, dpvt->irqslot, dpvt->irqaddr);

    // success
    return 0;
}

//////////////////////////////////////////////////////////////////////////
//
static
int f3rp61RegisterIoInterrupt(const dbCommon *prec, int unit, int slot, int channel)
{
    // debug
    //printf("%s:%s %s <= U%d,S%d,X%02d\n", __FILE__, __func__, prec->name, unit, slot, channel);

    //
    if (channel >= NUM_IRQ_CH) {
        errlogPrintf("drvF3RP61: %s : interrupt source U%d,S%d,X%02d exceeds ch# limit\n", prec->name, unit, slot, channel);
        return -1;
    }

    //
    if (ioscanpvt[unit][slot-1][channel-1] == 0) { // slot# and channel# starts from 1
        // Add requested interrup source on the table.
        scanIoInit(&ioscanpvt[unit][slot-1][channel-1]);
    } else {
    //    errlogPrintf("drvF3RP61: interrupt source U%d,S%d,X%02d already in use for %s\n", unit, slot, channel, prec->name);
    //    return -1;
    }

    //
    return 0;
}

//////////////////////////////////////////////////////////////////////////
//
//
//
static
int f3rp61EnableIoInterrupt(void)
{
    static int init_flag = 0;

    //debug
    //printf("%s:%s %d\n", __FILE__, __func__, init_flag);

    //
    if (init_flag) {
        return 0;
    }
    init_flag = 1;

    //
    EPICSTHREADFUNC handler = NULL;
    int msqid = -1;
    if (irq_interface == IRQ_MSQ) { // for kernel < 6.1.120
        handler = msgrcv_thread;

        // Create a SysV message queue
        msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
        if (msqid  == -1) {
            errlogPrintf("drvF3RP61: msgget failed [%d] : %s\n", errno, strerror(errno));
            return -1;
        }

#if defined(__powerpc__)
        if (msqid == 0) {
            // Get another message queue ID when it's 0.
            // Message queue id 0 is valid in SysV IPC but invalid in F3RP61 BSP.
            msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
            if (msqid == -1) {
                errlogPrintf("drvF3RP61: msgget failed [%d] : %s\n", errno, strerror(errno));
                return -1;
            }
        }
#endif
    } else {                        // for kernel >= 6.1.120
        handler = read_thread;

        // Use file descriptor to /dev/m3io
        msqid = f3rp61_fd;
    }

    // Enable I/O interrupt if requested
    int irq_requested = 0;
    for (int unit = 0; unit < M3IO_NUM_UNIT; unit++) {
        for (int slot = 1; slot <= M3IO_NUM_SLOT; slot++) { // slot# starts from 1

            // First, check if I/O module exist and clear previously enabled I/O interrupt
            M3IO_MODULE_INFORMATION module_info = {
                .unitno = unit,
                .slotno = slot,
            };

            ioctl(f3rp61_fd, M3IO_GET_MODULE_INFO, &module_info);

            if (module_info.enable) {
                // module exists
                M3IO_INTER_DEFINE arg = {
                    .unitno = unit,
                    .slotno = slot,
                    .defData.interMask = {0, 0, 0, 0},
                    .msgQId = msqid,
                };

                if (ioctl(f3rp61_fd, M3IO_MASK_INTER, &arg) < 0) {
                    // ioctl() will fail if this module does not support I/O interrupt. Just ignore ther error for now.

                    // debug
                    //char name[5];
                    //memcpy(name, module_info.name, 4);
                    //name[4] = '\0';
                    //errlogPrintf("drvF3RP61: failed to clear previously enable I/O interrupt. U%d,S%d %s [%d]\n", unit, slot, name, errno);
                }
            }


            // Then, check if I/O interrupt from this unit/slot is requested
            uint16_t mask[4] = {0};
            for (int channel = 1; channel <= NUM_IRQ_CH; channel++) { // channel# starts from 1
                IOSCANPVT pvt = ioscanpvt[unit][slot-1][channel-1];
                if (pvt) {
                    // debug
                    //printf("%s:%s U%d,S%d,X%02d %p\n", __FILE__, __func__, unit, slot, channel, pvt);
                    int idx = (channel-1)/16;
                    int bit = channel-(idx*16)-1;
                    mask[idx] |= (1<<bit);
                }
            }

            // If requested, enable I/O interrupt
            if (mask[0]>0||mask[1]>0||mask[2]>0||mask[3]>0) {
                irq_requested = 1;

                // debug
                //printf("%s:%s U%d,S%d mask: 0x%04x%04x%04x%04x\n", __FILE__, __func__, unit, slot, mask[3], mask[2], mask[1], mask[0]);

                M3IO_INTER_DEFINE arg = {
                    .unitno = unit,
                    .slotno = slot,
                    .defData.interMask = {mask[0], mask[1], mask[2], mask[3]},
                    .msgQId = msqid,
                };

                //#if defined(__arm__)
                //    if (enableM3IoIrqP(unit, slot, channel, id) < 0) {
                //        errlogPrintf("drvF3RP61: enableM3IoIrqP failed [%d]\n", errno);
                //        return -1;
                //    }
                //#else
                if (ioctl(f3rp61_fd, M3IO_MASK_INTER, &arg) < 0) {
                    errlogPrintf("drvF3RP61: ioctl failed [%d]\n", errno);
                    return -1;
                }
                //#endif
            }
        }
    }

    // Start IO IRQ handler thread
    char thread_name[32];
    sprintf(thread_name, "f3rp61_ioirq");

    //debug
    //printf("epicsThreadCreate %s\n", thread_name);
    if (irq_requested) {
        if (epicsThreadCreate(thread_name,
                              epicsThreadPriorityHigh,
                              epicsThreadGetStackSize(epicsThreadStackSmall),
                              handler,
                              (void *)msqid) == 0) {
            errlogPrintf("drvF3RP61: epicsThreadCreate failed\n");
            return -1;
        }
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// Get io interrupt info
//
long f3rp61GetIoIntInfo(int cmd, dbCommon *prec, IOSCANPVT *ppvt)
{
    F3RP61_DPVT *dpvt = prec->dpvt;
    if (!dpvt) {
        // this may happen if record initialization failed
        //errlogPrintf("drvF3RP61: f3rp61GetIoIntInfo is called with null dpvt: %s\n", prec->name);
        return -1;
    }

    // I/O intr handling
    int unit    = dpvt->irqunit;
    int slot    = dpvt->irqslot;
    int channel = dpvt->irqaddr;

    IOSCANPVT pvt = ioscanpvt[unit][slot-1][channel-1];
    // debug
    //printf("%s:%s %s U%d,S%d,X%02d %p\n", __FILE__, __func__, prec->name, unit, slot, channel, pvt);
    if (! pvt) {
        // this may not happen ...
    }

    *ppvt = pvt;

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// Register iocsh command 'f3rp61GetModuleInfo'
//
// usage: f3rp61GetModuleInfo [arg]
//
// List FA-M3/e-RT3 modules installed on the system.
// Empty slots are shown if whatever argument is given.
//
static const iocshArg      getModuleInfoArg0    = { "[verbose]", iocshArgString };
static const iocshArg     *getModuleInfoArgs[]  = { &getModuleInfoArg0 };
static const iocshFuncDef  getModuleInfoFuncDef = { "f3rp61GetModuleInfo", 1, getModuleInfoArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
    "Obtain information on all installed modules.\n"
#endif
};

static void getModuleInfoCallFunc(const iocshArgBuf *args)
{
    int verbosity = args[0].sval ? 1 : 0 ; // non-NULL if an argument is passed
    if (! getModuleInfo(verbosity)) {
#ifdef IOCSHFUNCDEF_HAS_USAGE
        iocshSetError(-1);
#endif
    }
}

static int getModuleInfo(int verbosity)
{
    printf("%4s %4s %4s %5s %4s %4s %4s\n",
           "Unit", "Slot", "Name", "MSize", "Xreg", "Yreg", "Dreg");
    for (int unit = 0; unit < M3IO_NUM_UNIT; unit++) {
        for (int slot = 1; slot < M3IO_NUM_SLOT + 1; slot++) {

            M3IO_MODULE_INFORMATION module_info = {
                .unitno = unit,
                .slotno = slot,
            };

            if (ioctl(f3rp61_fd, M3IO_GET_MODULE_INFO, &module_info)<0) {
                // ioctl() fails if no module is plugged in the specified slot, so the error should be ignored.
                //errlogPrintf("drvF3RP61: ioctl M3IO_GET_MODULE_INFO failed [%d]\n", errno);
                //return -1;
            }

            if (!module_info.enable) {
                if (!verbosity) {
                    continue;
                }
                module_info.name[3] =
                module_info.name[2] =
                module_info.name[1] =
                module_info.name[0] = '-';
            }

            char name[5];
            memcpy(name, module_info.name, 4);
            name[4] = '\0';
            printf("%4d %4d %4s %5d %4d %4d %4d\n",
                   module_info.unitno,
                   module_info.slotno,
                   name,
                   module_info.msize,
                   module_info.num_xreg,
                   module_info.num_yreg,
                   module_info.num_dreg);
        }
    }

    return 0; // Success
}

//////////////////////////////////////////////////////////////////////////
//
// Register iocsh command 'f3rp61GetInternalDeviceConfig'
//
static const iocshFuncDef  getInternalDeviceConfigFuncDef = { "f3rp61GetInternalDeviceConfig", 0, 0,
#ifdef IOCSHFUNCDEF_HAS_USAGE
    "Get local device assignment information.\n"
#endif
};

static void getInternalDeviceConfigCallFunc(const iocshArgBuf *args)
{
    if (! getInternalDeviceConfig()) {
#ifdef IOCSHFUNCDEF_HAS_USAGE
        iocshSetError(-1);
#endif
    }
}

static int getInternalDeviceConfig(void)
{
    unsigned int nrly;
    unsigned int nreg;
    if (referM3InternalDataTable(&nrly, &nreg)<0) {
        errlogPrintf("drvF3RP61: referM3InternalDataTable failed [%d]\n", errno);
        return -1;
    }

    printf("%7s %7s\n","rly", "reg");
    printf("%7d %7d\n", nrly, nreg);

    return 0; // Success
}

//////////////////////////////////////////////////////////////////////////
//
// Register iocsh command 'f3rp61SetInternalDeviceConfig'
//
static const iocshArg     setInternalDeviceConfigArg0     = { "nRlys", iocshArgInt };
static const iocshArg     setInternalDeviceConfigArg1     = { "nRegs", iocshArgInt };
static const iocshArg     setInternalDeviceConfigArg2     = { "[loc]", iocshArgInt };
static const iocshArg     *setInternalDeviceConfigArgs[]  = {
    &setInternalDeviceConfigArg0,
    &setInternalDeviceConfigArg1,
    &setInternalDeviceConfigArg2,
};
static const iocshFuncDef  setInternalDeviceConfigFuncDef = { "f3rp61SetInternalDeviceConfig", 3, setInternalDeviceConfigArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
    "Configure local device assignment.\n"
    "nRlys : Number of relays (0, 32, 64, ...)\n"
    "nRegs : Number of data registers (0, 2, 4, ...)\n"
    "loc   : Location of local device\n"
    "        0:SDRAM\n"
#  if defined(__powerpc__)
    "        1:Sytem SRAM on F3RP6x\n" // 256kB
#  endif
    "        2:User SRAM on F3RPxx-2L\n" // F3RP61-2L supports up to 4MB, while not sure for F3RP7x-2L
//    "Calling f3rp61SetInternalDeviceConfig after iocInit has no effect.\n"
#endif
};

static void setInternalDeviceConfigCallFunc(const iocshArgBuf *args)
{
    if (! setInternalDeviceConfig(args[0].ival, args[1].ival, args[2].ival)) {
#ifdef IOCSHFUNCDEF_HAS_USAGE
        iocshSetError(-1);
#endif
    }
}

static int setInternalDeviceConfig(int nrly, int nreg, int loc)
{
    static char *locname[] = { "SDRAM", "System-SRAM", "User-SRAM" };

    if (nrly < 0) {
        errlogPrintf("setInternalDeviceConfig: number of internal relays out of range\n");
        return -1;
    }
    if (nreg < 0) {
        errlogPrintf("setInternalDeviceConfig: number of internal registers out of range\n");
        return -1;
    }
    if (loc != 0 && loc != 2) {
        errlogPrintf("setInternalDeviceConfig: parameter for internal device out of range\n");
        return -1;
    }

    rly_size  = nrly;
    reg_size  = nreg;
    local_loc = loc;

    unsigned int cur_nrly;
    unsigned int cur_nreg;
    if (referM3InternalDataTable(&cur_nrly, &cur_nreg)<0) {
        errlogPrintf("drvF3RP61: referM3InternalDataTable failed [%d]\n", errno);
        return -1;
    }

    if (cur_nrly == nrly && cur_nreg == nreg) {
        // do nothing
        return 0;
    }

    if (setM3InternalDataTable(loc, nrly, nreg)<0) {
        switch (errno) {
        case S_m3dev_INVALID_NUMBER: // 392
            fprintf(stderr, "error : Parameter out of range\n");
            break;
        case S_m3dev_DEVICE_NOT_FOUND: //393
            fprintf(stderr, "error : Specified device not found : %s\n", locname[loc]);
            break;
        case S_m3dev_DEVICE_ENTRY_ERROR: //394; F3RP6x only
            //fprintf(stderr, "%s failed.\n", cmdname);
            fprintf(stderr, "Local device appears to be already configured.\n");
            fprintf(stderr, "System reboot is required to alter the configuration.\n");
            break;
        default:
            fprintf(stderr, "error : %s\n", strerror(errno));
        }
        return -1;
    }

    return 0; // Success
}

static const iocshFuncDef locDeviceConfigureFuncDef = { "f3rp61LocDeviceConfigure", 3, setInternalDeviceConfigArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
    "Alias for f3rp61SetInternalDeviceConfig, for debugging. Subject for removal.\n"
#endif
};

//////////////////////////////////////////////////////////////////////////
//
// Register iocsh command 'f3rp61GetSharedDeviceConfig'
//
static const iocshFuncDef  getSharedDeviceConfigFuncDef = { "f3rp61GetSharedDeviceConfig", 0, 0,
#ifdef IOCSHFUNCDEF_HAS_USAGE
    "Get shared device assignment information.\n"
#endif
};

static void getSharedDeviceConfigCallFunc(const iocshArgBuf *args)
{
    if (! getSharedDeviceConfig()) {
#ifdef IOCSHFUNCDEF_HAS_USAGE
        iocshSetError(-1);
#endif
    }
}

static int getSharedDeviceConfig(void)
{
    M3COMDATACONFIG com;
    M3COMDATACONFIG ext;
    if (referM3ComDataConfig(&com, &ext)<0) {
        errlogPrintf("drvF3RP61: referM3ComDataConfig failed [%d]\n", errno);
        return -1;
    }

    printf("%3s %4s %4s %4s %4s\n", "CPU", "rly", "reg",  "erly", "ereg");
    for (int i=0; i<4; i++) {
        printf("%3d %4d %4d %4d %4d\n", i, com.wNumberOfRelay[i], com.wNumberOfRegister[i], ext.wNumberOfRelay[i], ext.wNumberOfRegister[i]);
    }

    return 0; // Success
}

//////////////////////////////////////////////////////////////////////////
//
// Register iocsh command 'f3rp61SetSharedDeviceConfig'
//
static const iocshArg      setSharedDeviceConfigArg0    = { "cpuNo",     iocshArgInt };
static const iocshArg      setSharedDeviceConfigArg1    = { "nRlys",     iocshArgInt };
static const iocshArg      setSharedDeviceConfigArg2    = { "nRegs",     iocshArgInt };
static const iocshArg      setSharedDeviceConfigArg3    = { "ext_nRlys", iocshArgInt };
static const iocshArg      setSharedDeviceConfigArg4    = { "ext_nRegs", iocshArgInt };
static const iocshArg     *setSharedDeviceConfigArgs[]  = {
    &setSharedDeviceConfigArg0,
    &setSharedDeviceConfigArg1,
    &setSharedDeviceConfigArg2,
    &setSharedDeviceConfigArg3,
    &setSharedDeviceConfigArg4
};
static const iocshFuncDef  setSharedDeviceConfigFuncDef = { "f3rp61SetSharedDeviceConfig", 5, setSharedDeviceConfigArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
    "Configure shared device assignment for given CPU.\n"
    "Calling f3rp61SetSharedDeviceConfig after iocInit has no effect.\n"
#endif
};

static void setSharedDeviceConfigCallFunc(const iocshArgBuf *args)
{
    if (! setSharedDeviceConfig(args[0].ival, args[1].ival, args[2].ival, args[3].ival, args[4].ival)) {
#ifdef IOCSHFUNCDEF_HAS_USAGE
        iocshSetError(-1);
#endif
    }
}

static int setSharedDeviceConfig(int cpuno, int nrlys, int nregs, int ext_nrlys, int ext_nregs)
{
    if (cpuno < 0 || cpuno > 3) {
        errlogPrintf("f3rp61SetSharedDeviceConfig: CPU number out of range\n");
        return -1;
    }
    if (nrlys < 0 || nrlys >  2048) {
        errlogPrintf("f3rp61SetSharedDeviceConfig: Number of shared relays out of range\n");
        return -1;
    }
    if (nregs < 0 || nregs > 1024) {
        errlogPrintf("f3rp61SetSharedDeviceConfig: Number of shared registers out of range\n");
        return -1;
    }
    if (ext_nrlys < 0 || ext_nrlys >  2048) {
        errlogPrintf("f3rp61SetSharedDeviceConfig: Number of extended shared relays out of range\n");
        return -1;
    }
    if (ext_nregs < 0 || ext_nregs > 3072) {
        errlogPrintf("f3rp61SetSharedDeviceConfig: Number of extended shared registers out of range\n");
        return -1;
    }

    com_data_config.wNumberOfRelay[cpuno] = nrlys;
    com_data_config.wNumberOfRegister[cpuno] = nregs;
    ext_com_data_config.wNumberOfRelay[cpuno] = ext_nrlys;
    ext_com_data_config.wNumberOfRegister[cpuno] = ext_nregs;

    return 0; // Success
}

static const iocshFuncDef comDeviceConfigureFuncDef = { "f3rp61ComDeviceConfigure", 5, setSharedDeviceConfigArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
    "Old name for f3rp61SetSharedDeviceConfig, retained for backward compatibility.\n"
#endif
};

//////////////////////////////////////////////////////////////////////////
//
// Register iocsh command 'f3rp61GetLinkDeviceConfig'
//
static const iocshFuncDef  getLinkDeviceConfigFuncDef = { "f3rp61GetLinkDeviceConfig", 0, 0,
#ifdef IOCSHFUNCDEF_HAS_USAGE
    "Get shared device assignment information.\n"
#endif
};

static void getLinkDeviceConfigCallFunc(const iocshArgBuf *args)
{
    if (! getLinkDeviceConfig()) {
#ifdef IOCSHFUNCDEF_HAS_USAGE
        iocshSetError(-1);
#endif
    }
}

static int getLinkDeviceConfig(void)
{
    M3LINKDATACONFIG link;
    if (referM3LinkDeviceConfig(&link)<0) {
        errlogPrintf("drvF3RP61: referM3LinkDeviceConfig failed [%d]\n", errno);
        return -1;
    }

    printf("%3s %4s %4s\n", "Sys", "rly", "reg");
    for (int i=0; i<8; i++) {
        printf("%3d %4d %4d\n", i, link.wNumberOfRelay[i], link.wNumberOfRegister[i]);
    }

    return 0; // Success
}

//////////////////////////////////////////////////////////////////////////
//
// Register iocsh command 'f3rp61SetLinkDeviceConfig'
//
static const iocshArg      setLinkDeviceConfigArg0    = { "sysNo", iocshArgInt };
static const iocshArg      setLinkDeviceConfigArg1    = { "nRlys", iocshArgInt };
static const iocshArg      setLinkDeviceConfigArg2    = { "nRegs", iocshArgInt };
static const iocshArg     *setLinkDeviceConfigArgs[]  = {
    &setLinkDeviceConfigArg0,
    &setLinkDeviceConfigArg1,
    &setLinkDeviceConfigArg2
};
static const iocshFuncDef  setLinkDeviceConfigFuncDef = { "f3rp61SetLinkDeviceConfig", 3, setLinkDeviceConfigArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
    "Configure link device assignment.\n"
    "Calling f3rp61SetLinkDeviceConfig after iocInit has no effect.\n"
#endif
};

static void setLinkDeviceConfigCallFunc(const iocshArgBuf *args)
{
    if (! setLinkDeviceConfig(args[0].ival, args[1].ival, args[2].ival)) {
#ifdef IOCSHFUNCDEF_HAS_USAGE
        iocshSetError(-1);
#endif
    }
}

static int setLinkDeviceConfig(int sysno, int nrlys, int nregs)
{
    if (sysno <0 || sysno > (M3IO_NUM_LINKS-1) ) {
        errlogPrintf("f3rp61SetLinkDeviceConfig: Number of FL-net interface out of range\n");
        return -1;
    }
    if (nrlys < 1 || nrlys > 8192) {
        errlogPrintf("f3rp61SetLinkDeviceConfig: Number of link relays out of range\n");
        return -1;
    }
    if (nregs < 1 || nregs > 8192) {
        errlogPrintf("f3rp61SetLinkDeviceConfig: Number of link registers out of range\n");
        return -1;
    }

    link_data_config.wNumberOfRelay[sysno] = nrlys;
    link_data_config.wNumberOfRegister[sysno] = nregs;

    return 0; // Success
}

static const iocshFuncDef linkDeviceConfigreFuncDef = { "f3rp61LinkDeviceConfig", 5, setLinkDeviceConfigArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
    "Old name for f3rp61SetLinkDeviceConfig, retained for backward compatibility.\n"
#endif
};

//////////////////////////////////////////////////////////////////////////
//
// Register iocsh commands
//
static void drvF3RP61RegisterCommands(void)
{
    static int init_flag = 0;
    if (!init_flag) {
        init_flag = 1;
        iocshRegister(&getModuleInfoFuncDef,           getModuleInfoCallFunc);
        iocshRegister(&getInternalDeviceConfigFuncDef, getInternalDeviceConfigCallFunc);
        iocshRegister(&setInternalDeviceConfigFuncDef, setInternalDeviceConfigCallFunc);
        iocshRegister(&locDeviceConfigureFuncDef,      setInternalDeviceConfigCallFunc); // for debug
        iocshRegister(&getSharedDeviceConfigFuncDef,   getSharedDeviceConfigCallFunc);
        iocshRegister(&setSharedDeviceConfigFuncDef,   setSharedDeviceConfigCallFunc);
        iocshRegister(&comDeviceConfigureFuncDef,      setSharedDeviceConfigCallFunc);   // for backward compatibility
        iocshRegister(&getLinkDeviceConfigFuncDef,     getLinkDeviceConfigCallFunc);
        iocshRegister(&setLinkDeviceConfigFuncDef,     setLinkDeviceConfigCallFunc);
        iocshRegister(&linkDeviceConfigreFuncDef,      setLinkDeviceConfigCallFunc);     // for backward compatibility
    }
}

epicsExportRegistrar(drvF3RP61RegisterCommands);
