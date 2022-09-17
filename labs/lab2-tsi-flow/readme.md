# TSI Bootflow

## Overview

This lab will have two parts, firstly conceptual to get familiar with TileLink and TSI protocol. The other will be hands on compiling and understanding potential linking errors under the hood. 

## 1. Tethered Serial Interface (TSI) and TileLink

### 1.1 TileLink

[TileLink Spec 1.8.0](https://sifive.cdn.prismic.io/sifive%2Fcab05224-2df1-4af8-adee-8d9cba3378cd_tilelink-spec-1.8.0.pdf)

Please read and understand how Channel A and D works, this information will be useful for section 2.

### 1.2 TSI

 TSI protocol is an implementation of HTIF that is used to send commands to the RISC-V DUT. These TSI commands are simple R/W commands that are able to access the DUT’s memory space. During test, the host machine sends TSI commands through an USB adapter to DUT. The DUT then converts the TSI command into a TileLink request. This conversion is done by the SerialAdapter module (located in the `generators/testchipip` project). After the transaction is converted to TileLink, the TLSerdesser (located in `generators/testchipip`) serializes the transaction and sends it to the chip (this TLSerdesser is sometimes also referred to as a digital serial-link or SerDes). Once the serialized transaction is received on the chip, it is deserialized and masters a TileLink bus on the chip which handles the request. 
 
 TLDR: it serializes TileLink packets into at least 7 wires, with configurable width of data wires. 

[TODO][insert picture of the TSI]

## 2. Loading Program over TSI

As we're still figuring out how to have 20+ people accessing only two OsciBear on one lab bench with one power supply & clock clock generator. We'll explain how our setup works for now & plan to expand this access down the road. 

[TODO][insert setup diagram]

Please complete the following table using TSI code snippet from OsciBear, and save it for your future reference:

```verilog
module GenericSerializer(clock, reset, io_in_ready, io_in_valid,
     io_in_bits_chanId, io_in_bits_opcode, io_in_bits_param,
     io_in_bits_size, io_in_bits_source, io_in_bits_address,
     io_in_bits_data, io_in_bits_corrupt, io_in_bits_union,
     io_in_bits_last, io_out_ready, io_out_valid, io_out_bits);
  input clock, reset, io_in_valid, io_in_bits_corrupt, io_in_bits_last,
       io_out_ready;
  input [2:0] io_in_bits_chanId, io_in_bits_opcode, io_in_bits_param;
  input [3:0] io_in_bits_size, io_in_bits_source;
  input [31:0] io_in_bits_address;
  input [63:0] io_in_bits_data;
  input [7:0] io_in_bits_union;
  output io_in_ready, io_out_valid, io_out_bits;
```

### 2.1 **Exercise:** A Channel

| Signal    | Type (direction) | Width | 
| --------- | ---------------- | ----- |
| (chanid)  | TSI              |  _    |
| a_opcode  | C                |  _    |
| a_param   | C                |  _    |
| a_size    | C                | z = _ |
| a_source  | C                | o = _ |
| a_address | C                | a = _ |
| a_data    | D                | 8w = _|
| a_mask    | C                | w = _ |
| a_corrupt | D                |  _    |
| (last)    | TSI              |  _    |
| a_valid   | V                | 1     | 
| a_ready   | R (inverted)     | 1     | 

Total width: ___

### 2.2 Testing if TSI is Responding Correctly

To test if TSI is working, read out the BootROM and confirm the contents are correct. To further confirm, we constructed a TSI packet to write some code into the DTIM (scratchpad) and read it back. Let's try this process by hand, but there are two caviats we found out:

1. To send/recieve a TSI packet, we construct/deconstruct the message according to TileLink and *flip the bit order (reverse every bit)*. 

2. OsciBear is 32 bits, but the data width is 64, which means *the upper 32 bits of data are always 0* on this chip, and we need to change the `mask` field accordingly.

#### 2.2.1 Sending a TSI packet
Construct an A Channel TSI packet yourself by trying to read the BootROM at `0x0001_0000`, remember the caviat #1 above. 

#### 2.2.2 Sending a TSI packet
What would you expect to read back if BootROM is the following? Just give us the d_data field without reversing the bits, remember caviat #2. 
```
00010000 <_start>:
   10000:	020005b7          	lui	a1,0x2000
   10004:	f1402573          	csrr	a0,mhartid
   ...
```

Now that we know our code is on the DTIM, time to let Osci free! 


### 2.2 Running code at DTIM

The full BootROM dump for Osci is [here](https://github.com/ucberkeley-ee290c/chipyard-osci-sky130/blob/master/generators/chipyard/src/main/scala/ee290c/bootrom/bootrom.rv32.dump), beware that the future chips are slightly different. A BootROM is a small read only memory consisting of boot code that jumps to various PC addresses. Basically, the core with hartid=0 will be in a loop checking for CLINT interrupts at `0x0200_0000`, the msip register. After finishing transfer of our code, we set the msip register to 1, the BootROM will jump to `<boot_core_hart0>` and begin executing at `0x8000_0000`, our DTIM or scratchpad address.


## 3. Compiling & Linking

### 3.1 Remember assembly code is different

Because the C assumes the system starts in a certain status, and during the early testing stage we cannot ensure those conditions, so we have to write assembly code to do basic testings. We need to pay attention to the differences, though, when converting the C code to assembly counterpart.

Here's a snippet of the code we first wrote in assembly to test basic UART functionality. We are trying to convert the following C code to assembly, and here's the result.

```C
uint32_t *UART_TXDATA = 0x1000U;

void UART_transmit(uint8_t char) {
  *UART_TXDATA = char;
}

int main() {
  while (1) {
    UART_transmit('H');
  }
}
```

```assmebly

# UART MMIO addr
li s0, 0x1000

UART_transmit:
    sw a0, 0(s0)    # store character into UART transmit buffer
    ret             # return

main:
  li a0, 'H'          # pass character 'H' as function argument
  call UART_transmit  # invoke the function
  j main

```

Inspect the code and use [venus](https://venus.cs61c.org) to understand why our code didn't work. Now, take a careful look at the memory map of OsciBear in the spec. At which address did we load the code from last section? Now what happens when we call `ret` in UART_transmit, what is in `x1` and where will we jump to? Explain why this code will only output "H" over UART once and never actually continue to print more. 

### 3.2 Compiling Code Properly
This toolchain currently only works on linux related distros and will not work on Windows. If you don't have linux related OS locally, please create an cs199 account and log into hive to complete this section. (xPack will work on Windows/Linux, sifive's will only work on linux distros)

[TODO]


### 3.3 A couple other caviats
[TODO]


### 4. Ack
[TODO]