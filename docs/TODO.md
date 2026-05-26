TODO list for F3RP70/F3RP71/F3RP61 support
====

====
- [x] Reorganize treatment of option characters.
- [x] Reorganize BCD operations.
- [ ] More documentation.

F3RP61Seq devices
====
- [x] Add cache register (F register) support.
- [x] Add special register (Z register) support.
- [x] Add special relay (M relay) support.
- [x] Add Internal relays (I) for longin/longout records.
  - [x] Add &L, &F, &D options.
- [x] Add Internal relays (I) for ai/ao records.
  - [x] Add &L, &F, &D options.
- [x] Add support for mbbiDirect/mbboDirect records.
  - [x] Add &U option support
  - [x] Add &L option
  - [x] set NOBT to 32 with &L option, 16 without &L option
- [x] Add support mbbi/mbbo records.
  - [x] Add &U option support
  - [x] Add &L option support
  - [x] set NOBT to 32 with &L option, 16 without &L option
- [x] Fix byte-swap on &F and &D options.
- [ ] Consider ASLO/AOFF and SMOO fields for ai records.
- [ ] Consider ASLO/AOFF fields for ao records.
- [ ] Add Internal/file/cache registers (D/B/F).
  - [ ] for stringin records.
  - [ ] for stringout records.
- [ ] Add waveform record support.
- [ ] Add aai/aao record support.
- [ ] Add lsi/lso record support.
- [ ] Add int64in/int64out record support.

F3RP61 devices
====
- [ ] Control debug print via IOC shell variable
- [ ] Add F3RPxx internal relay/register support
- [x] Reorganize input relay interrupts
  - [x] Allow multiple interrupt source from different channels on the same slot
  - [ ] Allow interupt-based process for F3RP61Seq devices
  - [ ] Allow interupt-based process for F3RP61SysCtl devices
- [x] Add Shared relays (E) and Link relays (L).
  - [x] longin/longout records.
  - [ ] ai records.
  - [ ] ao records.
- [x] Add Shared registers (R) and Link registers (W).
  - [x] longin/longout records.
  - [x] ai/ao records.
  - [ ] stringin/stringout records.
- [ ] Revise I/O registers (A) support:
  - [ ] Add &L, &F and &D option for ai/ao records (do we need this?).
- [x] Revise input relays (X):
  - [x] Add &F and &D options for ai/ao records.
- [x] Revise Shared memory (r) support for F3RP7x.
  - [x] longin/longout records.
    - [x] Add &B and &L option for longin/longout records.
  - [x] for mbbi/mbbo records.
  - [x] ai/ao records.
    - [x] Add &L, &F and &D option for ai/ao records.
  - [ ] stringin/stringout records.
- [ ] Revise waveform records.
  - [ ] Type of FTVL field (such as DBF_LONG/DBF_FLOAT/DBF_DOUBLE) implies &U/&L/&F/&D option. FTVL and option shall be independent.
  - [ ] Add &B option support.
  - [ ] for Shared memory (r).
  - [ ] for I/O registers on special devices (A).
  - [ ] DOUBLE and FLOAT were unexpectedly rejected.
  - [ ] support for CHAR and UCHAR.
  - [ ] support for LONG.
- [ ] Revise mbbi/mbbo records
  - [x] Add &U option
  - [x] Add &L option
  - [x] set NOBT to 32 with &L option, 16 without &L option
  - [ ] Add &B option
  - [ ] Add &F option
  - [ ] Add &D option
- [ ] Revise mbbiDirect/mbboDirect records
  - [x] Add &U option
  - [x] Add &L option
  - [x] set NOBT to 32 with &L option, 16 without &L option
- [x] Add &B option for input relays (X) - do we need this?
  - [x] longin records.
- [x] Add &B option for output relays (Y) - do we need this?
  - [x] longin/longout records.
- [x] Workaround for reading output registers (Y) with F3RP61
  - [x] for mbbi records.
  - [x] for mbbiDirect records.
- [ ] Revise Mode register (M) support:
  - [ ] Consider that mode registers have different word size in f3rp61 and f3rp71. Perhaps we'd better to use IOC shell function to configure mode registers rather than PV
  - [x] Add mode register (M) support to longin/longout records.
    - [ ] Add &U option for longin/longout records - do we need this?.
    - [ ] Add &L option for longin/longout records - do we need this?.
  - [ ] Add aai/aao record supports - do we need this?
  - [ ] Drop mode register (M) support from Mbbi/Mbbo records (?).
- [ ] Consider ASLO/AOFF and SMOO fields for ai records.
- [ ] Consider ASLO/AOFF fields for ao records.
- [ ] Add support for aai/aao records.
- [ ] Add support for lsi/lso records.
- [ ] Add int64in/int64out record support.
- [ ] Secure against reading waveform larger than 4kB.
- [ ] Read waveform data beyond 4kB.
- [ ] Add option to access I/O modules in 8-bit data width.
- [ ] Define and use dpvt structure which is common for various records.
- [ ] Support for double-word access modules (e.g. F3XP01, F3XP02).
  - [ ] We mey need either dedicated option or dedicated device for those double-word access modules.

F3RP61Sysctl devices
====
- [ ] Add rotary-switch support for longin devices.
