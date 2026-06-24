<!-- -*- coding: utf-8-unix -*- -->

Device and Driver Support for F3RP70, F3RP71, and F3RP61
========================================================

<!-- npx doctoc /path/to/Manual.md -->
**Table of Contents**
<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->

- [Overview](#overview)
- [Supported Device Types, Record Types and Conversion Specifiers](#supported-device-types-record-types-and-conversion-specifiers)
  - [Device Types](#device-types)
  - [Record Types](#record-types)
  - [Conversion Specifiers](#conversion-specifiers)
  - [Reading / Writing an Array of Data](#reading--writing-an-array-of-data)
- [Accessing Input Relays, Output Relays and Data Registers on I/O Modules (F3RP61 devices)](#accessing-input-relays-output-relays-and-data-registers-on-io-modules-f3rp61-devices)
  - [Input / Output Link (`INP`/`OUT`) Fields for Input Relays, Output Relays and Data Registers](#input--output-link-inpout-fields-for-input-relays-output-relays-and-data-registers)
  - [Accessing Input Relays (X)](#accessing-input-relays-x)
  - [Accessing Output Relays (Y)](#accessing-output-relays-y)
  - [Accessing Data Registers (A)](#accessing-data-registers-a)
  - [Accessing Mode Register](#accessing-mode-register)
  - [Handling Special Module](#handling-special-module)
  - [I/O Interrupt Support](#io-interrupt-support)
- [Accessing Shared Relays and Shared Registers](#accessing-shared-relays-and-shared-registers)
  - [Important Notice on Using Linux CPU in Multi-CPU Configuration](#important-notice-on-using-linux-cpu-in-multi-cpu-configuration)
  - [Communication Based on Shared Device](#communication-based-on-shared-device)
    - [Communication Based on Shared Device Using New Interface<a name="UsingNewInterface"></a>](#communication-based-on-shared-device-using-new-interfacea-nameusingnewinterfacea)
      - [Notes on Using Shared Device with F3RP70 or F3RP71 (**not** F3RP61)<a name="SharedDeviceWithF3RP71"></a>](#notes-on-using-shared-device-with-f3rp70-or-f3rp71-not-f3rp61a-nameshareddevicewithf3rp71a)
      - [Input / Output Link (INP/OUT) Fields for Shared Relays / Registers](#input--output-link-inpout-fields-for-shared-relays--registers)
      - [Reading/Writing Shared Relays (1-bit variables)](#readingwriting-shared-relays-1-bit-variables)
      - [Reading/Writing Shared Registers](#readingwriting-shared-registers)
    - [Communication Based on Shared Memory Using Old Interface](#communication-based-on-shared-memory-using-old-interface)
- [Accessing Internal Relays or Data/File/Cache Registes on Sequence CPU (F3RP61Seq devices)](#accessing-internal-relays-or-datafilecache-registes-on-sequence-cpu-f3rp61seq-devices)
  - [Input / Output Link (INP/OUT) Fields](#input--output-link-inpout-fields)
- [FL-net Support](#fl-net-support)
- [LED / Rotary Switch / Status Register support](#led--rotary-switch--status-register-support)
  - [LED support](#led-support)
  - [Rotary switch support](#rotary-switch-support)
  - [Status register support](#status-register-support)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# Overview

This device and driver support can be used to run EPICS ioc core on an
Linux CPUs, F3RP70, F3RP71, and F3RP61, made by Yokogawa Electric
Corporation. Linux CPU is able to access most of the I/O modules of
the FA-M3 PLC on the PLC-bus. This feature opens way for making an
FA-M3 PLC itself a new type of IOC. This device and driver support
provides interfaces for iocCore to access I/O modules as well as
ordinary sequence CPUs that works on the PLC-bus. The device and driver
support is implemented by wrapping the APIs of the kernel-level driver
and the user level library, which are included in the Board Support
Package (BSP) provided by Yokogawa. Read [Install.md](Install.md) for
installation instruction.


As to I/O modules, the device and driver support offers only primitive
method to access the relays and registers. In the sense that any
relays and registers of a supported I/O module can be accessed by
using the device and driver support, it is universal. However, in order
to handle a special module that requires some sequence logic to
execute I/O operation, such as motion control module, the sequence
logic needs to be implemented by using an EPICS sequencer program by
the user. The sequence logic to handle a special module is
implemented by using a ladder program when a sequence CPU is used to
execute the I/O operation. The EPICS sequencer program is used to
replace the ladder program. See [Handling Special
Module](#handling-special-module) for more detail. If some
initialization is required on a special module, it can be done by
using an EPICS sequencer program, or a run-time database comprised of
records that have the PINI field value of `YES`.


The Linux CPU works as an IOC either with or without sequence CPUs
which run ladder programs. If there is no sequence CPU on the PLC-bus,
the Linux CPU should manage all the I/O activities. If one or more
sequence CPUs attached to the PLC-bus, some of I/O modules can be
controlled by sequence CPUs, while the others by the Linux CPUs. It is
recommended that those I/O modules under the control of the sequence
CPU be indirectly accessed by the Linux CPU via the internal devices
of the sequence CPUs. See [Important Notice on Using Linux CPU in
Multi-CPU
Configuration](#important-notice-on-using-linux-cpu-in-multi-cpu-configuration)
for more detail.


Digital input modules of FA-M3 can interrupt CPUs upon a change of the
state of the input signal. The BSP has a function that transforms the
interrupt into a message to a user-level process running on it. Based
on this function, the device and driver support supports processing
records upon an I/O interrupt.

# Supported Device Types, Record Types and Conversion Specifiers

## Device Types

In order to use the device and driver support, the device type (DTYP) field of the
record must be set to either:

* **`F3RP61`** for accessing relays and registers on I/O modules, and
  shared relays and shared registers,
* **`F3RP61Seq`** or accessing internal devices (`D`, `I`, `B`) of
  the sequence CPUs on the same base unit, or
* **`F3RP61SysCtl`** for controlling status LEDs and/or reading rotary
   switch position of the Linux CPU module.

Note that F3RP71 also uses `F3RP61`, `F3RP61Seq`, and `F3RP61SysCtl`
for its device type.

## Record Types

The table below shows the supported PLC device types along with the
relevant record types.

| PLC&nbsp;device | Description                             |  DTYP      | &nbsp;Data&nbsp;width&nbsp; | Supported record types                                                                          |
|-----------------|-----------------------------------------|------------|-----------------------------|-------------------------------------------------------------------------------------------------|
| X               | Input relays on input modules           | F3RP61     | 1-bit, 16-bit               | bi,     longin,          mbbiDirect,             mbbi,       ai,             waveform, aai      |
| Y               | Output relays on output modules         | F3RP61     | 1-bit, 16-bit               | bi, bo, longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao,         waveform, aai, aao |
| E               | (Extended) Shared relays                | F3RP61     | 1-bit, 16-bit               | bi, bo, longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo  ai, ao,         waveform, aai, aao |
| L               | Link relays (for FA Link and FL-net)    | F3RP61     | 1-bit, 16-bit               | bi, bo, longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao,         waveform, aai, aao |
| M               | Mode registers on I/O modules           | F3RP61     |        16-bit               |         longin, longout  mbbiDirect, mbboDirect, mbbi, mbbo  ao, ao,         waveform, aai, aao |
| R               | (Extented) Shared registers             | F3RP61     |        16-bit               |         longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao,         waveform, aai, aao |
| W               | Link registers (for FA Link and FL-net) | F3RP61     |        16-bit               |         longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao,         waveform, aai, aao |
| A               | Registers on special module             | F3RP61     |        16-bit               |         longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao, si, so, waveform, aai, aao |
| r               | Shared memory                           | F3RP61     |        16-bit               |         longin, longout  mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao                             |
| I               | Internal relays                         | F3RP61Seq  | 1-bit, 16-bit               | bi, bo, longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao                             |
| M               | Special relays                          | F3RP61Seq  | 1-bit, 16-bit               | bi, bo, longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao                             |
| D               | Data registers                          | F3RP61Seq  |        16-bit               |         longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao                             |
| B               | File registers                          | F3RP61Seq  |        16-bit               |         longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao                             |
| F               | Cache registers                         | F3RP61Seq  |        16-bit               |         longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao                             |
| Z               | Special registers                       | F3RP61Seq  |        16-bit               |         longin, longout, mbbiDirect, mbboDirect, mbbi, mbbo, ai, ao                             |
| T               | Timer relays                            |            |                             |                                                                                                 |
| C               | Counter relays                          |            |                             |                                                                                                 |
| V               | Index registers                         |            |                             |                                                                                                 |

**Note**: Special modules refer only to those modules accessed through
READ/WRITE instruction of the sequence CPU modules, e.g. analog
input/output, temperature control, PID control, high-speed counter,
etc.

The table below shows supported record types with DTYP fields used to
access specific devices. Some record types accepts conversion specifier.

| Record type   | DTYP         | Supported device                          | Supportedd conversion specifiers |
|---------------|--------------|-------------------------------------------|----------------------------------|
| bi            | F3RP61       | X, Y, E, L                                |                                  |
| bo            | F3RP61       |    Y, E, L                                |                                  |
| longin        | F3RP61       | X, Y, E, L, R, W, M, A  r                 | U, L, B                          |
| longout       | F3RP61       |    Y, E, L, R, W, M, A  r                 | U, L, B                          |
| mbbiDirect    | F3RP61       | X, Y, E, L, R, W, M, A, r                 | U, L                             |
| mbboDirect    | F3RP61       |    Y, E, L, R, W, M, A, r                 | U, L                             |
| mbbi          | F3RP61       | X, Y, E, L, R, W, M, A, r                 | U, L                             |
| mbbo          | F3RP61       |    Y, E, L, W, R, M, A, r                 | U, L                             |
| ai            | F3RP61       | X, Y, E, L, R, W, M, A  r                 | U, L, F, D                       |
| ao            | F3RP61       |    Y, E, L, R, W, M, A  r                 | U, L, F, D                       |
| aai, waveform | F3RP61       | X, Y, E, L, R, W, M, A, r                 | U, L, F, D                       |
| aao           | F3RP61       |    Y, E, L, R, W, M, A, r                 | U, L, F, D                       |
| si            | F3RP61       |                      A                    |                                  |
| so            | F3RP61       |                      A                    |                                  |
| bi            | F3RP61Seq    | I, M                                      |                                  |
| bo            | F3RP61Seq    | I, M                                      |                                  |
| longin        | F3RP61Seq    | I, M, D, B, F, Z                          | U, L, B                          |
| longout       | F3RP61Seq    | I, M, D, B, F, Z                          | U, L, B                          |
| mbbiDirect    | F3RP61Seq    | I, M, D, B, F, Z                          | U, L                             |
| mbboDirect    | F3RP61Seq    | I, M, D, B, F, Z                          | U, L                             |
| mbbi          | F3RP61Seq    | I, M, D, B, F, Z                          | U, L                             |
| mbbo          | F3RP61Seq    | I, M, D, B, F, Z                          | U, L                             |
| ai            | F3RP61Seq    | I, M, D, B, F, Z                          | U, L, F, D                       |
| ao            | F3RP61Seq    | I, M, D, B, F, Z                          | U, L, F, D                       |
| bi            | F3RP61SysCtl | LEDs: R, A, E, 1, 2, 3; System Stat. Reg. |                                  |
| bo            | F3RP61SysCtl | LEDs: R, A, E, 1, 2, 3                    |                                  |
| mbbi          | F3RP61SysCtl | Rotary Switch position                    |                                  |

**Notes**
- Device `r` represents shared memory (or 'Old interface' for shared registers/relays).
- In `aai`, `waveform`, and `aao` records, the supported `FTVL` fields are `DBF_DOUBLE`, `DBF_FLOAT`, `DBF_LONG`, `DBF_ULONG`, `DBF`_SHORT`, `DBF_USHORT`.
- When the relay device is accessed by records other than `bi`/`bo`, the subsequent 16 relays are accessed as 16-bit data. In this case, the device number must be multiple of 16 plus 1, i.e., 1, 17, 33, 49, and so on.

## Conversion Specifiers

For some of supported record types, conversion specifier can be used in the `INP` / `OUT` field.
The type conversion specifiers determines how to interpret he byte sequence in the registers (or relays) being read (or written) as follows:

- (no specifier) - Treat 16-bit data as signed 16-bit integer.
- &U - Treat as unsigned 16-bit integer.
- &L - Treat two subsequent 16-bit data as 32-bit integer (long-word).
- &B - Treat 16-bit data as binary-coded-decimal (BCD).
  - In case of input record, 16-bit BCD data (0 – 9999) read from the
device is converted to unsigned integer value and stored in the `VAL`
field of the record. If the data from the device includes an invalid
hexadecimal number (A – F), the invalid number is rounded to 9, and
the alarm severity and the alarm status are set to `INVALID` / `HIGH`.
  - In case of output record, integer value in `VAL` field is converted to
16-bit BCD data (0 – 9999) and sent to the device. When the value is
sent to the device, the value in `VAL` field higher than 9999 is
rounded to 9999, and the negative value is rounded to 0. If the values
is rounded to either 9999 or 0, the alarm severity and the alarm
status are set to `INVALID` / `HW_LIMIT`.
- &F - Interpret two subsequent 16-bit data as single precision floating point (32-bit)
- &D - Interpret four subsequent 16-bit data as double precision floating point (64-bit)

Not all the supported conversion specifiers are necessarily
meaningful. For example, consider reading 32 subsequent special relays
to an `ai` record using the `&F` conversion.

## Reading / Writing an Array of Data

'aai`, `waveform`, and `aao` records are supported to read / write an
array of data from subsequent relays or registers. The supported
`FTVL` fields are `DBF_DOUBLE`, `DBF_FLOAT`, `DBF_LONG`, `DBF_ULONG`,
`DBF`_SHORT`, `DBF_USHORT`. The `FTVL` field and conversion specifier
in the `INP`/`OUT` can be specified independently.

Due to hardware constraints, such as limitations of the I/O module,
the number of elements actually read or written (`NORD` field) may be
smaller than the number of elements in the record (`NELM` field).


# Accessing Input Relays, Output Relays and Data Registers on I/O Modules (F3RP61 devices)

## Input / Output Link (`INP`/`OUT`) Fields for Input Relays, Output Relays and Data Registers

The notation for input (`INP`) or output (`OUT`) link field for accessing I/O modules is as following:

```
field(INP, "@Uunit,Sslot,typenumber[&conversion")
```

e.g.

```
field(INP, "@U1,S2,X03&L")
```

- unit : Unit number (starts from 0)
- slot : slot number (starts from 1)
- type : CPU Device type such as Input relays or Data registers
- number : device number (starts from 1)
- conversion : Conversion specifier to interpret the byte sequence in the registers (or relays):
  * (no specifier) - Treat 16-bit data as signed 16-bit integer
  * &U - unsigned integer (16-bit)
  * &L - long-word (32-bit) access
  * &B - binary-coded-decimal (BCD)
  * &F - Single precision floating point (32-bit)
  * &D - Double precision floating point (64-bit)

Optionally, PLC-like notation is supported for 'X' and 'Y' devices:

```
field(INP, "@typenumber[&conversion")
```

e.g.

```
field(INP, "@X10203&L")
```

Combination of supported record types and PLC devive types are described in [Record Types](#record-types).

## Accessing Input Relays (X)

Input relays are read-only devices. Binary input (`bi`) records,
multi-binary input direct (`mbbiDirect`) records, long input
(`longin`) records and analog input (`ai`) records are supported by
the device support. The three numbers, a unit number, a slot number
and an input relay number must be specified in the `INP` field as the
following example shows.

```
record(bi, "f3rp61_example_1") {
    field(DTYP, "F3RP61")
    field(INP, "@U0,S2,X1")
}
```

The above record reads a value of either "on" (1) or "off" (0) from
the first input relay (X1) of an I/O module in the second slot (S2) of
the main unit (U0).

The next example shows how to read 16 bits of status data on a group
of input relays by using an `mbbiDirect` record.

```
record(mbbiDirect, "f3rp61_example_2") {
    field(DTYP, "F3RP61")
    field(INP, "@U0,S2,X1")
}
```

In this case, the number `1` as in `X1` specifies the first relay to
read. The status bits on the 16 relays, `X1`, `X2`, `X3`, ... are read
into the `B0`, `B1`, `B2`, ... fields of the `mbbiDirect` record.

The next example shows how to read a digital value of 16 bits on a
group of input relays which represents a state from a range of up to
16 states by using an mbbi record.

```
record(mbbi, "f3rp61_example_3") {
    field(DTYP, "F3RP61")
    field(INP, "@U0,S2,X1")
    field(ZRVL, "0")
    field(ZRST, "OFF")
    field(ONVL, "1")
    field(ONST, "ON")
    field(TWVL, "2")
    field(TWST, "Idle")
    field(THVL, "4")
    field(THST, "Error")
}
```

In this case, it is assumed that the Least Significant Bit (LSB) is on
`X1` and the Most Significant Bit (MSB) is on `X16`, and the digital value
on the input relays is assumed to be unsigned short. The unsigned
short value is set to `RVAL` field to determine the current state of
this record out of 4 states.
For example, when the unsigned short value is 4, this record represents `Error` state.


The next example shows how to read a digital value of 16 bits on a
group of input relays by using longin records.

```
record(longin, "f3rp61_example_4") {
    field(DTYP, "F3RP61")
    field(INP, "@U0,S2,X1")
}
```

In this case, it is assumed that the Least Significant Bit (LSB) is on
`X1` and the Most Significant Bit (MSB) is on `X16`, and the digital
value on the input relays is assumed to be signed short. If the value
is unsigned short, `&U` must follow the relay number as follows.

```
record(longin, "f3rp61_example_5") {
    field(DTYP, "F3RP61")
    field(INP, "@U0,S2,X1&U")
}
```

The rules explained in the last two examples apply to the ai
record. The value read from the input relays goes into the raw value
(`RVAL`) field of the ai record.


## Accessing Output Relays (Y)

Output relays are read-write devices. Just replacing `X` with `Y`
in the INP fields of the example records shown in the previous
subsection suffices to read output relays.

In order to write output relays, binary output (`bo`) records,
multi-bit binary output direct (`mbboDirect`) records, long output
(`longout`) records, and analog output (`ao`) records can be used. The
following example shows how to write an output relay by using a bo
record.

```
record(bo, "f3rp61_example_6") {
    field(DTYP, "F3RP61")
    field(OUT, "@U0,S2,Y1")
}
```

The above record writes a value of either "on" (1) or "off" (0) onto
the first output relay (Y1) of an I/O module in the second slot (S2)
of the main unit (U0).

The next example shows how to write 16 bits of data onto a group of
output relays by using an `mbboDirect` record.

```
record(mbboDirect, "f3rp61_example_7") {
    field(DTYP, "F3RP61")
    field(OUT, "@U0,S2,Y1")
}
```

In this case, the number in `Y1` specifies the first relay to
write. The 16-bits data of the `mbboDirect` record, `B0`, `B1`, `B2`, ... are
written onto the output relays, `Y1`, `Y2`, `Y3`, ... , respectively.

The next example shows how to write a value of 16 bits onto a group of
output relays by using an mbbo record.

```
record(mbbo, "f3rp61_example_8") {
    field(DTYP, "F3RP61")
    field(OUT, "@U0,S2,Y1")
    field(ZRVL, "0")
    field(ZRST, "Stop")
    field(ONVL, "1")
    field(ONST, "Play")
    field(TWVL, "2")
    field(TWST, "Pause")
    field(THVL, "4")
    field(THST, "Forward")
    field(FRVL, "8")
    field(FRST, "Back")
}
```

In this case, it is assumed that the Least Significant Bit (LSB) goes
onto Y1 and the Most Significant Bit (MSB) goes onto Y16, and the
value is unsigned short. The value set to `VAL` field is considered as
an index of one of 5 states. If `VAL` is set to 3 which represents
`Forward`, `RVAL` is set to 4 according to THVL, and then the value in
RVAL is finally written to `Y1` - `Y16` as an unsigned short value.

The next example shows how to write a value of 16 bits onto a group of
output relays by using a longout record.

```
record(longout, "f3rp61_example_9") {
    field(DTYP, "F3RP61")
    field(OUT, "@U0,S2,Y1")
}
```

In this case, it is assumed that the Least Significant Bit (LSB) goes
onto Y1 and the Most Significant Bit (MSB) goes onto Y16, and the
value is signed short. If the value is unsigned short, `&U` must
follow the relay number as follows.

```
record(longout, "f3rp61_example_10") {
    field(DTYP, "F3RP61")
    field(OUT, "@U0,S2,Y1&U")
}
```

The rules explained in the last two examples apply to the ao
record. The value in the raw value (`RVAL`) field of the ao record is
written onto the output relays.


## Accessing Data Registers (A)

Analog I/O modules and other special modules, such as motion control
modules, etc., have many registers to hold the I/O data and relevant
parameters. In order to read / write these registers, `longin` /
`longout`, `ai` / `ao`, and `mbbiDirect` / `mbboDirect` records are
supported.

The following example shows how to use a `longin` record to read a
16-bit register.

```
record(longin, "f3rp61_example_11") {
    field(DTYP, "F3RP61")
    field(INP, "@U0,S3,A1")
}
```

The above record reads a value of 16-bit data from the first register
(A1) of a module in the third slot (S3) of the main unit (U0).

The following example shows how to use a `longout` record to write a
16-bit register.

```
record(longout, "f3rp61_example_12") {
    field(DTYP, "F3RP61")
    field(OUT, "@U0,S3,A1")
}
```

The above record writes a value of 16-bit data onto the first register
(A1) of a module in the third slot (S3) of the main unit (U0).

The rules in the above two examples apply to `ai` / `ao`, `mbbiDirect`
/ `mbboDirect` records. The value read from (written onto) the device
comes in (goes out) via the `RVAL` field of the records.


<!--
### Using 'unsigned' conversion specifier

As to longin and ai type records, the `&U` conversion specifier can be
specified at the end of the INP field to read unsigned 16-bit data as
shown below.

```
record(longin, "f3rp61_example_13") {
    field(DTYP, "F3RP61")
    field(INP, "@U0,S3,A1&U")
}
```
-->

<!--
### Using 'binary-coded-decimal (BCD)' conversion specifier

As to longin and longout type records, `&B` conversion specifier can
be specified at the end of INP or OUT field.

In case of input record, 16-bit BCD data (0 – 9999) read from the
device is converted to unsigned integer value and stored in the `VAL`
field of the record. If the data from the device includes an invalid
hexadecimal number (A – F), the invalid number is rounded to 9, and
the alarm severity and the alarm status are set to `INVALID` / `HIGH`.

In case of output record, integer value in `VAL` field is converted to
16-bit BCD data (0 – 9999) and sent to the device. When the value is
sent to the device, the value in `VAL` field higher than 9999 is
rounded to 9999, and the negative value is rounded to 0. If the values
is rounded to either 9999 or 0, the alarm severity and the alarm
status are set to `INVALID` / `HW_LIMIT`.

Usage is shown below.

```
record(longin, "f3rp61_example_14") {
    field(DTYP, "F3RP61")
    field(INP, "@U0,S3,A1&B")
}
record(longout, "f3rp61_example_15") {
    field(DTYP, "F3RP61")
    field(OUT, "@U0,S3,A1&B")
}
```
-->

<!--
### Read an array of data

'aai`, `waveform`, and `aao` records are supported to read / write an
array of data from the relays or registers of I/O modules.  The
supported `FTVL` fields are `DBF_DOUBLE`, `DBF_FLOAT`, `DBF_LONG`,
`DBF_ULONG`, `DBF`_SHORT`, `DBF_USHORT`.

The following example shows how to read successive 8 registers of an
intelligent module in Slot 3 of Unit 0. The address, A1, specifies the
first register to read.

```
record(waveform, "f3rp61_example_16") {
    field(DTYP, "F3RP61")
    field(INP, "@U0,S3,A1")
    field(FTVL, "SHORT")
    field(NELM, "8")
}
```
-->

## Accessing Mode Register

Non-intelligent digital I/O modules have mode registers which hold
settings for input sampling period, input filter time constant,
interrupt edge (rising or falling), or output on faiulre (hold or
reset).

Following iocsh commands will read back these values for the I/O
module in the specified unit and slot. When unit is omitted, it is
treated as 0.

```shell
f3rp61GetInputSampling [unit] slot
f3rp61GetInterruptEdge [unit] slot
f3rp61GetInputFilter   [unit] slot
f3rp61GetOutputHold    [unit] slot
```

Following iocsh commands will set these values for specified unit and
slot. When unit is omitted, it is treated as 0.

```shell
f3rp61SetInputSampling [unit] slot ch val
f3rp61SetInterruptEdge [unit] slot ch val
f3rp61SetInputFilter   [unit] slot ch val
f3rp61SetOutputHold    [unit] slot ch val
```

The usage will be shown, for example by `help f3rp61SetInterrupEdge` (available from EPICS base R7.0.4 onwards).

Refer to User's Manual for the detail of channels and values:

- F3RP70 / F3RP71
  - **IM 34M06M52-02E**, "e-RT3 CPU Module (SDRD␣2) BSP Common Function Manual", 4.4 Mode register access
  - **IM 34M06M52-22E**, "e-RT3 Linux BSP (SFRD12) Programming Manual", 4.5.1 I/O module
- F3RP61
  - **IM 34M06M51-32E**, "e-RT3 CPU Module (F3RP6␣) BSP Common Function Manual", 4.4 Mode register access
  - **IM 34M06M51-44E**, "RTOS-CPU module (F3RP61-␣␣) Linux BSP Reference Manual", 1.4.8 ioctl

Mode registers can also be read and written as EPICS records by specifying `M` device in INP / OUT field, for example:

```
record(mbbiDirect, "f3rp61_example_17") {
    field(DTYP, "F3RP61")
    field(INP, "@U0,S3,M1")
}
```

<!--
The above record reads a value of 16-bit data from the first mode
register (M1) of a non-intelligent digital I/O module in the third
slot (S3) of the main unit (U0).

The following example shows how to use an mbboDirect record to write a
16-bit register.

```
record(mbboDirect, "f3rp61_example_18") {
    field(DTYP, "F3RP61")
    field(OUT, "@U0,S3,M1")
}
```

The above record writes a value of 16-bit data into the first mode
register (M1) of a non-intelligent digital I/O module in the third
slot (S3) of the main unit (U0). For example, if the I/O module is a
digital input module and if you set the B0 field of the above record,
it results in specifying that a part of the I/O channels (from X25 to
X32) of the module raise interrupts upon the falling edge of the input
signals. For more details, please consult relevant hardware manuals
from Yokogawa Electric Corporation.

Note that the mbbo record always outputs a value of zero when the
record gets processed by being written a value into its VAL field
regardless of whatever the value is. The author is not sure if the
behavior is just a specification or a bug of the mbbo record. At any
rate, if you complete setting the conditions on a digital I/O module
bit by bit by using B0, B1, B2, …, BF fields of an mbbo record, you
are free from the problem mentioned above.
-->

## Handling Special Module

This section describes how to handle special modules that require some
sequence logic to execute I/O operations. As an example, we consider a
motion control module that controls the motion of a stepping motor by
sending a train of pulses to the motor driver.

Suppose you drive a motor dedicated to an axis. In the first place,
you make the Linux CPU set the number of pulses (distance to move)
into a register of the motion control module by using longout or ao
record(s). (While the parameter is 32 bit long, the motion control
modules do not support long word (32 bits) read / write operation. Two
records, therefore, are necessary to set the upper 16 bits and the
lower 16 bits.)

Next, you make the Linux CPU turn on an output relay (EXE) to trigger
the action. This can be performed by putting the value of one (1) into
the VAL field of the following record. (The relay number varies from
type to type. The following example records are just for
illustration. See the manual of the module you use for more detailed
information. We assume the motion control module is in slot 4.)

```
record(bo, "f3rp61_motion_exe") {
    field(DTYP, "F3RP61")
    field(OUT, "@U0,S4,Y33")
}
```

The motion control module will respond to the EXE command by turning
on an input relay (ACK) if no error is found in the parameters
given. The Linux CPU needs to check the ACK by using a record shown
below.

```
record(bi, "f3rp61_motion_ack") {
    field(SCAN, ".1 second")
    field(DTYP, "F3RP61")
    field(INP, "@U0,S4,X1")
}
```

And then, the operation (sending pulses) starts. The Linux CPU is
required to turn off the EXE after the ACK is turned on. The motion
control module, then, turns off the ACK after the EXE is turned off.

When the operation (sending pulses) completes, the motion control
module informs the Linux CPU the completion by turning on yet
another input relay (FIN). Just like the ACK, the FIN needs to be
checked by using a record shown below.

```
record(bi, "f3rp61_motion_fine") {
    field(SCAN, ".1 second")
    field(DTYP, "F3RP61")
    field(INP, "@U0,S4,X5")
}
```

Note that the records to monitor the input relays must be processed
periodically. You might want to choose to use the interrupt support
explained in section 7 instead of periodic scanning, though the author
has not yet tried it in using motion control modules.

The essential part of the EPICS sequencer program to manage the
sequence described above in words will look like as follows.

```
int exe;
in ack;
int fin;
assign exe f3rp61_motion_exe;
assign ack f3rp61_motion_ack;
assign fin f3rp61_motion_fin;
monitor exe;
monitor ack;
monitor fin;
ss exe_move {
    state wait_exe {
        when (exe) {
        } state wait_ack
    }

    state wait_ack {
        when (ack) {
            exe = FALSE;
            pvPut(exe);
        } state wait_fin
    }

    state wait_fin {
        when(fin) {
        } state wait_exe
    }
}
```

The author reminds you that the above example is just for
illustration. The sequencer program for the real operation involves a
little bit more to handle other types of commands for the motion
control module, and to handle exceptions that can occur in the
sequence (for example, an error caused by a wrong parameter set by the
user).

## I/O Interrupt Support

Digital input modules of FA-M3 can interrupt the Linux CPU when they
detect a rising edge or falling edge of the input signals. The
kernel-level driver of the BSP can transform the interrupt into a
message to a user-level process. Based on the function, processing
records by I/O interrupt is supported with the device and driver
support. Any records that have the `DTYP` field value of `F3RP61` and
the `SCAN` field `I/O Intr` get processed upon an interrupt on a
specified channel of the specified module. This feature allows you to
trigger a read / write operation by external trigger signals.

Suppose a unit which comprises of an F3RP71 in slot 1, a digital input
module in slot2, and an A/D module in slot 3. The following record
reads the first data register (A1) of the A/D module upon a trigger
input into the first channel (X1) of the digital input module. (The
`INP` field format takes the form of
"@I/O\_data\_channel:interrupt\_source".)

```
record(ai, "f3rp61_example_46") {
    field(DTYP, "F3RP61")
    field(SCAN, "I/O Intr")
    field(INP, "@U0,S3,A1:U0,S2,X1")
}
```

If you have a D/A module in slot 4, in addition to the module
configuration mentioned above, you can write an output value into the
first data register (A1) of the D/A module upon the same trigger input
by using the following record.

```
record(ao, "f3rp61_example_47") {
    field(DTYP, "F3RP61")
    field(SCAN, "I/O Intr")
    field(OUT, "@U0,S4,A1:U0,S2,X1")
}
```

The following example might seem a little bit strange, but it helps
you see how quick the Linux CPU can respond to interrupts.

```
record(bi, "f3rp61_example_48") {
    field(DTYP, "F3RP61")
    field(SCAN, "I/O Intr")
    field(INP, "@U0,S2,X1:U0,S2,X1")
}
```

The `bi` record reads the status (level) of the relay for the trigger
input. Suppose the trigger signal is a pulse and the digital module
generates an interrupt at the rising edge. If the `bi` record reads
the status before the signal level falls down, the record reads the
status of "on" (1). Otherwise, it reads the status of "off" (0). By
changing the pulse duration with checking the record value, you can
roughly measure the time required for the record to get processed with
the rising edge of the trigger signal as the starting point.


# Accessing Shared Relays and Shared Registers

There are two ways to access the sequence CPU from the Linux CPU: One
is a shared-memory-based method and the other is message-based one.
The former is synchronous access that completes instantly, just like
the access to the I/O relays and registers of an I/O module. The
latter is asynchronous and takes a few milliseconds to complete. For
this reason, two different `DTYP`s are defined in the device and driver
support, namely `F3RP61` for the former (synchronous) and `F3RP61Seq`
for the latter (asynchronous).

This section describes the shared-memory-based method, which specifies
`F3RP61` for the `DTYP` field:

```
field(DTYP, "F3RP61")
```

## Important Notice on Using Linux CPU in Multi-CPU Configuration

This section gives you an important notice on using a Linux CPU
together with sequence CPUs on the same unit.

A typical use case of multi-CPU configuration is a sequence CPU
implementing an interlock logic which requires high reliability, and a
Linux CPU controlling or monitoring the interlock system. In such a
case, one have to pay attention to the following two points:

- The sequence CPU must be in the first slot (slot 1). The CPU in the
  first slot becomes the master of the unit, which resets the whole
  system upon rebooting.
- The Linux CPU **should NOT** access, regardless of read or write, to
  those I/O modules used by the sequence CPU for the interlock system.

The reason of the second point is as follows. If an I/O module is
accessed by a Linux CPU, the I/O module recognizes and remembers that
the Linux CPU is one of its masters. When the Linux system on the CPU
is rebooted, the Linux CPU broadcasts that it was reset by using a
signal on the PLC-bus. The I/O modules that have been accessed by the
Linux CPU and recognize it as one of their masters reset themselves
when they detect the signal. It makes the I/O modules inaccessible by
the sequence CPU and makes the ladder program stop with I/O
errors. For this reason, it is highly recommended that you make the
Linux CPU read the status of interlock indirectly via some internal
devices (`I`, `D`, `B`) of the sequence CPU or, through the shared
devices (`E`, `R`) by using a method described in the next section.

On the other hand, if one or more I/O modules are used with a Linux
CPU for some control in the multi-CPU configuration, you need to tell
the sequence CPU not to touch the I/O modules under the Linux CPU's
control. This setting can be done on the sequence CPU by using the
ladder development software, WideField3 (or WideField2). From the menu
of your `Project`, select `Configuration` and then, select `DIO
Setup`. Change the default setup from `Use` to `Not used` for the I/O
modules. Otherwise, the sequence CPU overwrites the data of the output
channels of the modules with the value of zero even when any I/O
execution commands on the I/O modules do not appear explicitly in the
ladder program.


## Communication Based on Shared Device

The following is the basics to understand how the communication
between Linux CPUs and sequence CPUs works.

* Each CPU, a sequence CPU or a Linux CPU can have shared memory
  regions allocated to it.
* Any CPU, a sequence CPU or a Linux CPU can write into only the
  regions allocated to it.
* Any CPU, a sequence CPU or a Linux CPU can read out from any
  regions.

In order to make the story simple, we consider a case where only one
sequence CPU in slot 1 works with only one Linux CPU in slot 2 on the
same base unit. From the rules mentioned above, how we use the shared
memory to make the two CPUs communicate with each other is clear. If
the data go from the sequence CPU (CPU1) to the Linux CPU (CPU2), use
the area allocated to the sequence CPU (CPU1), and vice versa, as
shown in the figure below.

![Shared Memory](./shared-memory.png)


### Communication Based on Shared Device Using New Interface<a name="UsingNewInterface"></a>

The device and driver support make use of APIs for shared device
(i.e., shared relays and shared registers), which are available in
F3RP71 BSP (R1.03 or later), as well as in F3RP61 BSP (R2.01 or
later). Calling `f3rp61SetSharedDeviceConfig` command prior to
`iocInit` in the IOC start-up script (st.cmd) allocates shared devices
for specified CPU:

```shell
f3rp61SetSharedDeviceConfig 0 512 256 64 32
f3rp61SetSharedDeviceConfig 1 512 256 64 32
```

The first line allocates 512 shared relays, 256 words of shared
registers, 64 extended shared relays, and 32 words of extended shared
registers the CPU in slot 1 (e.g., a sequence CPU). The second line
allocates the same numbers of shared relays and shared registers to
the CPU in slot 2 (e.g., F3RP71 CPU).  Note that indices starts from
0. Make sure that arguments to f3rp61ComDeviceConfigure() for the
sequence CPU shall be consistent with Inter-CPU Shared Memory Setup in
WideField3.

The former name of `f3rpSetSharedDeviceConfig`,
`f3rp61ComDeviceConfigure` has also been retained for backward
compatibility.

The `f3rp61GetSharedDeviceConfig` command will read back the
shared device configuration from all CPUs.


#### Notes on Using Shared Device with F3RP70 or F3RP71 (**not** F3RP61)<a name="SharedDeviceWithF3RP71"></a>

Make sure that, when using F3RP70 or F3RP71 (**not** F3RP61) in a
multi-CPU configuration, `Non-Simultaneous` is selected for `Shared
Refreshed Data` in Inter-CPU Shared Memory Setup.  Otherwise even if
F3RP71 writes anything to the shared memory, it looks like as if
nothing has been modified when read from the sequence CPU.  Refer to
following manuals for the detail:

- **IM 34M06M52-02E**, "e-RT3 CPU Module (SFRD␣2) BSP Common Function Manual", 5.2 Shared device
- **IM 34M06Q16-02E**, "FA-M3 Programming Tool WideField3 (Offline)", D3.1.13 Inter-CPU Shared Memory Setup.

#### Input / Output Link (INP/OUT) Fields for Shared Relays / Registers

The notation for input (`INP`) or output (`OUT`) link field for accessing shared relays and registers is as following:

```
field(INP, "@typenumber[&conversion")
```

e.g.

```
field(INP, "@R003&L")
```

- type : Device type
- number : device number
- conversion : Conversion specifier to interpret the byte sequence in the registers (or relays):
  * (no specifier) - Treat 16-bit data as signed 16-bit integer
  * &U - unsigned integer (16-bit)
  * &L - long-word (32-bit) access
  * &B - binary-coded-decimal (BCD)
  * &F - Single precision floating point (32-bit)
  * &D - Double precision floating point (64-bit)

#### Reading/Writing Shared Relays (1-bit variables)

The following example shows how to read a shared relay by using a bi
record.

```
record(bi, "f3rp61_example_19") {
    field(DTYP, "F3RP61")
    field(INP, "@E1")
}
```

The bi record can be used to read the first shared relay (E1). In this
case, the relay (E1) can be allocated to any CPU as mentioned earlier.

The following example shows how to write a shared relay by using a bo
record.

```
record(bo, "f3rp61_example_20") {
    field(DTYP, "F3RP61")
    field(OUT, "@E1")
}
```

The bo record can be used to write the first shared relay (E1). In
this case, the relay (E1) must be allocated to the Linux CPU as
mentioned earlier.


#### Reading/Writing Shared Registers

Shared registers are read-write devices. Usage of each record type
and its conversion specifier is described with the examples below.

The following example shows how to read a shared register by using a
longin record.

```
record(longin, "f3rp61_example_21") {
    field(DTYP, "F3RP61")
    field(INP, "@R1")
}
```

The longin record can be used to read the first shared register
(R1). In this case, the register (R1) can be allocated to any CPU.

The following example shows the usage of "B" conversion
specifier. Shared register value that is read from a register is
regarded as BCD format, converted to integer value, and then stored in
the VAL field as such.


```
record(longin, "f3rp61_example_22") {
    field(DTYP, "F3RP61")
    field(INP, "@R1&B")
}
```

If two registers, say, R1 and R2, are written by a Sequence CPU to
transfer a long word (32-bits) value, it can be read by using the
following record, of which INP field value has the address "@R1" with
the trailing "&L".

```
record(longin, "f3rp61_example_23") {
    field(DTYP, "F3RP61")
    field(INP, "@R1&L")
}
```

The following example shows how to write a shared register by using a
longout record.

```
record(longout, "f3rp61_example_24") {
    field(DTYP, "F3RP61")
    field(OUT, "@R1")
}
```

The longout record can be used to write the first shared register
(R1). In this case, the register (R1) must be allocated to the Linux
CPU.

The following example shows usage of "B" conversion specifier. Value
in the VAL field is converted to BCD format and written to a shared
register.

```
record(longout, "f3rp61_example_25") {
    field(DTYP, "F3RP61")
    field(OUT, "@R1&B")
}
```

In order to write a long word (32-bits) value onto two registers, say,
R1 and R2, the following record, of which INP field value has the
address "R1" with the trailing "&L", can be used.

```
record(longout, "f3rp61_example_26") {
    field(DTYP, "F3RP61")
    field(OUT, "@R1&L")
}
```

The following example shows how to read a shared register by using an
ai record.

```
record(ai, "f3rp61_example_27") {
    field(DTYP, "F3RP61")
    field(INP, "@R1")
}
```

The ai record can be used to read the first shared register (R1). In
this case, the register (R1) can be allocated to any CPU.

If two registers, say, R1 and R2, are written by a Sequence CPU to
transfer a long word (32-bits) value, it can be read by using the
following record, of which INP field value has the address "@R1" with
the trailing "&L".

```
record(ai, "f3rp61_example_28") {
    field(DTYP, "F3RP61")
    field(INP, "@R1&L")
}
```

If two registers, say, R1 and R2, are written by a Sequence CPU to
transfer a float type (32-bits) value, it can be read by using the
following record, of which INP field value has the address `@R1` with
the trailing `&F`.

```
record(ai, "f3rp61_example_29") {
    field(DTYP, "F3RP61")
    field(INP, "@R1&F")
}
```

If four registers, say, `R1`, `R2`, `R3` and `R4`, are written by a Sequence
CPU to transfer a double type (64-bits) value, it can be read by using
the following record, of which INP field value has the address `@R1`
with the trailing `&D`.

```
record(ai, "f3rp61_example_30") {
    field(DTYP, "F3RP61")
    field(INP, "@R1&D")
}
```

The following example shows how to write a shared register by using an
ao record.

```
record(ao, "f3rp61_example_31") {
    field(DTYP, "F3RP61")
    field(OUT, "@R1")
}
```

The longout record can be used to write the first shared register
(R1). In this case, the register (R1) must be allocated to the Linux
CPU.

In order to write a long word (32-bits) value onto two registers, say,
R1 and R2, the following record, of which INP field value has the
address "R1" with the trailing "&L", can be used.

```
record(ao, "f3rp61_example_32") {
    field(DTYP, "F3RP61")
    field(OUT, "@R1&L")
}
```

In order to write a float type value (32-bits) onto two registers,
say, R1 and R2, the following record, of which INP field value has the
address "R1" with the trailing "&F", can be used.

```
record(ao, "f3rp61_example_33") {
    field(DTYP, "F3RP61")
    field(OUT, "@R1&F")
}
```

In order to write a double type value (64-bits) onto four registers,
say, R1, R2, R3 and R4, the following record, of which INP field value
has the address "R1" with the trailing "&D", can be used.

```
record(ao, "f3rp61_example_34") {
    field(DTYP, "F3RP61")
    field(OUT, "@R1&D")
}
```

Waveform type records are supported to read out an array of data from
shared registers. The value of FTVL (Field Type of VaLue) must match
with the type of the data written by the Sequence CPU.

The following example shows how to read successive 256 float values
(on 512 words of registers). The address, R1, specifies the first
address of the successive registers to read.

```
record(waveform, "f3rp61_example_35") {
    field(DTYP, "F3RP61")
    field(INP, "@R1")
    field(FTVL, "FLOAT")
    field(NELM, "256")
}
```

MbbiDirect /mbboDirect and mbbi / mbbo are also supported to read /
write the shared registers.


### Communication Based on Shared Memory Using Old Interface

It is strongly recommended to use the new APIs to access shared relays
and shared registers, which is much easier to understand.  This device
and driver support, however, still supports the old shared memory APIs
for the backward compatibility.  To use the old APIs you need to know
the following points.

* While a sequence CPU can access shared relays bit by bit, an F3RP61
    CPU can access the shared relays only by word.
* The specification of the INP / OUT fields is different from that
    explained in [Using New Interface](#UsingNewInterface).
* You need to understand the address map shown below.

In order to make the story simple, we consider a case where we have
only two CPUs, a sequence CPU in slot 1 (CPU1) and an F3RP61 CPU in
slot 2 (CPU2). In addition, we assume that we allocate 512 bits (32
words) of shared relays and 256 words of shared registers to both CPU1
(a sequence CPU) and CPU2 (an F3RP61 CPU). In this case, the F3RP61
gets to see the following flat memory space, of which address starts
with zero (the first "R00000" of the left most column).

```
---------------------------------------------------------------------------------------------
R00000: E00001 – E00016 Allocated to CPU1 (sequence CPU)
R00001: E00017 – E00032
R00002: E00033 – E00048
...
R00031: E00497 – E00512
---------------------------------------------------------------------------------------------
R00032: E00513 – E00528 Allocated to CPU2 (F3RP61 CPU)
R00033: E00529 – E00544
R00034: E00545 – E00560
...
R00063: E00497 – E01024
---------------------------------------------------------------------------------------------
R00064: R00001 Allocated to CPU1 (sequence CPU)
R00065: R00002
R00066: R00003
...
R00319: R00256
---------------------------------------------------------------------------------------------
R00320: R00257 Allocated to CPU2 (F3RP61 CPU)
R00321: R00258
R00322: R00259
...
R00575: R00512
---------------------------------------------------------------------------------------------
```

The following record allows the F3RP61 CPU to read the first 16 bits
of shared relays (E00001 –E00016) all at once.

```
record(mbbiDirect, "f3rp61_example_36") {
    field(DTYP, "F3RP61")
    field(INP, "@CPU1,R0")
}
```

Note that, in this case, the address you need to specify in the INP
field is the left most number in the mapping table shown above. The bi
record is not supported to read a shared relay because CPU2 (F3RP61
CPU) can access shared relays only by word when you choose the
old-API-based access.

Similarly, the first shared register allocated to CPU1 (sequence CPU)
can be read by using the following record.

```
record(mbbiDirect, "f3rp61_example_37") {
    field(DTYP, "F3RP61")
    field(INP, "@CPU1,R64")
}
```

Both R0 and R64 in the last two examples cannot be written by the CPU2
(F3RP61 CPU) since they are allocated to CPU1 (sequence CPU).

Next, we consider CPU2 (F3RP61 CPU) writing a value into a region
allocated to it.

```
record(mbboDirect, "f3rp61_example_38") {
    field(DTYP, "F3RP61")
    field(OUT, "@CPU2,R32")
}
```

On the CPU1 (sequence CPU)-side, it gets to see a write onto 16 bits
of shared relays (E00513 – E00528) occur all at once upon the above
record processing.

Similarly, the first shared register allocated to CPU2 (F3RP61 CPU)
can be written by using the following record.

```
record(mbboDirect, "f3rp61_example_39") {
    field(DTYP, "F3RP61")
    field(OUT, "@CPU2,R320")
}
```

Needless to say, CPU2 (F3RP61 CPU) can read back the data written from
both R32 (E00513 – E00528) and R320 (R00257) by using an appropriate
input type record.

You might think that there is no reason for specifying the CPU-numbers
in the INP fields of the examples shown above since the F3RP61 CPU
sees the shared memory as a single flat memory space starting with the
first address of zero. However, we do need to specify the CPU-numbers
as shown in the examples. We do not discuss it any more since it is a
bit complicated story.

Ai / ao and longin / longout, are also supported to read / write the
shared registers.


# Accessing Internal Relays or Data/File/Cache Registes on Sequence CPU (F3RP61Seq devices)

The alternative method for a Linux CPU to communicate with a sequence
CPU is to use the message-based communicaion transaction, which
specifies `F3RP61Seq` for the `DTYP` field:

```
field(DTYP, "F3RP61")
```

It enables the Linux CPU to access internal relays and registers of
the sequence CPU. Currently supported by the device support are
internal relays `I`, internal registers `D`, file registers `B`, and
cache registers `F`.


## Input / Output Link (INP/OUT) Fields

The notation for input (`INP`) or output (`OUT`) link field for `F3RP61Seq` device is as following:

```
field(INP, "@CPUcpu,typenumber[&conversion]")
```

e.g.

```
field(INP, "@CPU1,I00003&L")
```

- cpu  : CPU number (starts from 1)
- slot : slot number (starts from 1)
- type : CPU Device type such as Input relays or Data registers
- number : device number (starts from 1)
- conversion : Conversion specifier to interpret the byte sequence in the registers (or relays):
  * (no specifier) - Treat 16-bit data as signed 16-bit integer
  * &U - unsigned integer (16-bit)
  * &L - long-word (32-bit) access
  * &B - binary-coded-decimal (BCD)
  * &F - Single precision floating point (32-bit)
  * &D - Double precision floating point (64-bit)

Combination of supported record types and PLC devive types are described in [Supported Record Types](#supported-record-types).

The following example shows how to set (1)/ reset (0) an internal
relay ("I"), say, "I4", of CPU1 (a sequence CPU in slot 1).

```
record(bo, "f3rp61_example_40") {
    field(DTYP, "F3RP61Seq")
    field(OUT, "@CPU1,I4")
}
```

Note that the device type must be `F3RP61Seq` in this case. In order
to read back the result, you can use the following record.

```
record(bi, "f3rp61_example_41") {
    field(DTYP, "F3RP61Seq")
    field(INP, "@CPU1,I4")
}
```

Record types longin/longout support a BCD (binary-coded-decimal)
conversion specifier. Usage and detailed explanation are provided with
the examples below.

In order to write a data register ("D"), say, "D7", of CPU1 (a
sequence CPU in slot 1), the following record can be used.

```
record(longout, "f3rp61_example_42") {
    field(DTYP, "F3RP61Seq")
    field(OUT, "@CPU1,D7")
}
```

Again, the result can be read back by using the following record.

```
record(longin, "f3rp61_example_43") {
    field(DTYP, "F3RP61Seq")
    field(INP, "@CPU1,D7")
}
```

The following example shows the usage of "B" conversion
specifier. Shared register value that is read from a register is
regarded as BCD format, converted to integer value, and then stored in
the VAL field as such.

```
record(longin, "f3rp61_example_44") {
    field(DTYP, "F3RP61Seq")
    field(INP, "@CPU1,D7")
}
```

The following example shows the usage of "B" conversion specifier for
output record. Value in the VAL field is converted to BCD format and
written to a shared register.

```
record(longout, "f3rp61_example_45") {
    field(DTYP, "F3RP61Seq")
    field(OUT, "@CPU1,D7")
}
```


# FL-net Support

There are two different methods in using FL-net. One is based on
message transmission and the other is based on cyclic
transmission. The device and driver support supports only the latter
with fixed link refresh period of 10 milliseconds. It does not support
FL-net in multi-CPU configuration at present. (setM3FlnSysNo() is
called without specifying sysNo in the driver support.)

In order to use FL-net with a Linux CPU, users need to put the
following IOC command in the startup script for the Linux CPU to
specify how many link relays and link registers are allocated to each
of the links.

```shell
f3rp61SetLinkDeviceConfig 0 512 256
f3rp61SetLinkDeviceConfig 1 512 256
```

The command needs to be executed prior to the call to iocInit(). The
first line implies that 512 bits of link relays and 256 words of link
registers are allocated to Link1(0 + 1). The next line means that the
same numbers of link relays and link registers are allocated to
Link2(1 + 1). The Linux CPU can handle up to two FL-net interface
modules, i.e., up to two links though the author have tested only one
link so far.

The former name of `f3rpSetLinkDeviceConfig`,
`f3rp61LinkDeviceConfigure` has also been retained for backward
compatibility.

The `f3rp61GetLinkDeviceConfig` command will read back the shared
device configuration from all nodes.

Allocation of the link relays and link registers to each of the nodes
on a link needs to be done on a sequence CPU-side by using WideField3
(or WideField2). There is nothing to be done on the Linux CPU on this
point. Only allocation of the link relays and link registers to each
of the links is needed by using the command described above.

How to make a Linux CPU communicate with other nodes on a link is
similar with how to make the F3RP71-based IOC communicate with another
CPU on the same base unit through shared memory based on the new APIs
as described in [Using New Interface](#UsingNewInterface).

The following example shows how to read a link relay.

```
record(bi, "f3rp61_example_49") {
    field(DTYP, "F3RP61")
    field(INP, "@L00001")
}
```

The bi record can be used to read the first link relay allocated to a
node on the Link1(0 + 1). The first zero of `L00001` subsequent to the
leading `L` specifies the link1(0 + 1) and the trailing `0001`
specifies the address of the link relay on the link. (The first link
relay of the Link2(1 + 1) can be addressed by `L10001`.) In this case,
since it is a read access, the link relay can be allocated to any of
the nodes on the link.

The following example shows how to write a link relay.

```
record(bo, "f3rp61_example_50") {
    field(DTYP, "F3RP61")
    field(OUT, "@L00001")
}
```

The bo record can be used to write the first link relay allocated to a
node on the Link1(0 + 1). In this case, since it is a write access,
the relay must be allocated to the Linux CPU as a node on the link.

The following example shows how to read a link register.

```
record(longin, "f3rp61_example_51") {
    field(DTYP, "F3RP61")
    field(INP, "@W00001")
}
```

The longin record can be used to read the first link register
allocated to a node on the Link1(0 + 1). The first zero of `W00001`
subsequent to the leading `W` specifies the link1(0 + 1) and the
trailing `0001` specifies the address of the link register on the
link. (The first link register of the Link2(1 + 1) can be addressed by
`W10001`.) In this case, since it is a read access, the link register
can be allocated to any of the nodes on the link.

The following example shows how to write a link register.

```
record(longout, "f3rp61_example_52") {
    field(DTYP, "F3RP61")
    field(OUT, "@W00001")
}
```

The longout record can be used to write the first link register
allocated to a node on the Link1(0 + 1). In this case, since it is a
write access, the register must be allocated to the Linux CPU as a
node on the link.

<!--
In order to read / write long word (32-bits) values by using longin /
longout records, `&L` conversion specifier can be used as explained in
6.1.1 in the case of shared registers. The rule also applies to link
registers with replacing `R` with `W`.

`ai` / `ao` record types are also supported to read / write link
registers. The conversion specifier to read / write a long word
(32-bits) value, a float type (32-bits) value, and a double type
(64-bits) value are available as in the case of reading / writing
shared registers.

Waveform type records are supported to read out an array of data from
link registers.  The `FTVL` field and conversion specifier in the
`INP`/`OUT` can be specified independently.
-->

The following example shows how to read successive 256 float values
(on 512 words of registers). The address, W00001, specifies the first
address of the successive registers to read.

```
record(waveform, "f3rp61_example_53") {
    field(DTYP, "F3RP61")
    field(INP, "@W00001")
    field(FTVL, "FLOAT")
    field(NELM, "256")
}
```

MbbiDirect /mbboDirect are also supported to read / write link
registers.


# LED / Rotary Switch / Status Register support

On the front panel of the Linux CPU there are status LEDs (namely,
**Run**, **Alarm**, **Error**, **U1**, **U2**, and **U3**) as well as
an rotary switch that is used to control boot option for the Linux
CPU. Device support provides functionality to control those LEDs, read
position of the rotary switch and read the status register (battery
status) using EPICS database records. In that case, DTYP of the record
must be set to `F3RP61SysCtl`. Additionally, there is iocsh command
available to control status LEDs.


## LED support

The following example shows the usage of iocsh command to control
status LEDs (**Run**, **Alarm**, **Error**, **U1**, **U2**, and
**U3**). To turn on Run LED, execute the following command:

```
f3rp61SetLED R 1
```

To turn off U2 LED, execute the following command:

```
f3rp61SetLED 2 0
```

Alternative usage is:

```
f3rp61SetLED Run 1
```

This command only reads first letter of the string. Thus, **R**, **A**,
and **E** are interpreted as **Run**, **Alarm** and **Error** respectively.

The usage of bo records to set status LEDs is shown in the following
example for **Run** LED:

```
record(bo, "f3rp61_example_54") {
    field(DTYP, "F3RP61SysCtl")
    field(OUT, "@SYS,LR")
}
```

To control **U1** LED change to OUT field to `@SYS,L1` as shown below:

```
record(bo, "f3rp61_example_55") {
    field(DTYP, "F3RP61SysCtl")
    field(OUT, "@SYS,L1")
}
```

Set OUT field to `@SYS,LA`, `@SYS,LE`, `@SYS,L2`, @SYS,L3` to control
**Alarm**, **Error**, **U2**, **U3** LEDs, respectively.

The following example shows how to read status of **Run** LED:

```
record(bi, "f3rp61_example_56") {
    field(DTYP, "F3RP61SysCtl")
    field(INP, "@SYS,LR")
}
```

Set INP field to `@SYS,LA`, `@SYS,LE`, `@SYS,L2`, `@SYS,L3` to read
status of **Alarm**, **Error**, **U2**, **U3** LEDs, respectively.


## Rotary switch support

The usage of mbbi record to read position of the rotary switch is shown below:

```
record(mbbi, "f3rp61_example_58") {
    field(DTYP, "F3RP61SysCtl")
    field(OUT, "@SYS,S")
}
```

It is important to note that returned position will be position of the
switch at boot time, irrelevant of the current position of the switch.


## Status register support

The usage of bi record to read status register is shown below:

```
record(bi, "f3rp61_example_57") {
    field(DTYP, "F3RP61SysCtl")
    field(OUT, "@SYS,R")
}
```
