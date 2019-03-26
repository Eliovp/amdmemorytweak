# AMD Memory Tweak
Read and modify memory timings on the fly
﻿
---
## Overview
AMD Memory Tweak lets you read and change memory timings at all times.

## Support
* Supports GDDR5 based GPU's
* Supports HBM & HBM2 based GPU's
* Linux only

## System Requirements
* AMD Radeon GDDR5|HBM|HBM2-based GPU(s).
* amdgpu-pro | rocm

## Building
Prerequisites
  * pciutils-dev | libpci-dev
  * build-essential
  
Clone the repository
  * git clone https://github.com/Eliovp/amdmemorytweak.git
  
cd amdmemorytweak

Build
  * g++ amdmemorytweak.cpp -lpci -lresolv -o amdmemtweak

## Usage

Global command line options:
 --help         Show this output
 --gpu|--i [comma-separated gpu indices]        Selected device(s)
 --current      List current timing values

 Command line options: (HBM2)
 --CL|--cl [value]
 --RAS|--ras [value]
 --RCDRD|--rcdrd [value]
 --RCDWR|--rcdwr [value]
 --RC|--rc [value]
 --RP|--rp [value]
 --RRD|--rrd [value]
 --RTP|--rtp [value]
 --FAW|--faw [value]
 --CWL|--cwl [value]
 --WTRS|--wtrs [value]
 --WTRL|--wtrl [value]
 --WR|--wr [value]
 --WRRD|--wrrd [value]
 --RDWR|--rdwr [value]
 --REF|--ref [value]
 --MRD|--mrd [value]
 --MOD|--mod [value]
 --PD|--pd [value]
 --CKSRE|--cksre [value]
 --CKSRX|--cksrx [value]
 --RFC|--rfc [value]

 Command line options: (GDDR5)
 --CL|--cl [value]
 --W2R|--w2r [value]
 --CCDS|--ccds [value]
 --CCLD|--ccld [value]
 --R2W|--r2w [value]
 --NOPR|--nopr [value]
 --NOPW|--nopw [value]
 --RCDW|--rcdw [value]
 --RCDWA|--rcdwa [value]
 --RCDR|--rcdr [value]
 --RCDRA|--rcdra [value]
 --RRD|--rrd [value]
 --RC|--rc [value]
 --RFC|--rfc [value]
 --TRP|--trp [value]
 --RP_WRA|--rp_wra [value]
 --RP_RDA|--rp_rda [value]
 --WDATATR|--wdatatr [value]
 --T32AW|--t32aw [value]
 --CRCWL|--crcwl [value]
 --CRCRL|--crcrl [value]
 --FAW|--faw [value]
 --PA2WDATA|--pa2wdata [value]
 --PA2RDATA|--pa2rdata [value]
 --ACTRD|--actrd [value]
 --ACTWR|--actwr [value]
 --RASMACTRD|--rasmactrd [value]
 --RASMACWTR|--rasmacwtr [value]
 --RAS2RAS|--ras2ras [value]
 --RP|--rp [value]
 --WRPLUSRP|--wrplusrp [value]
 --BUS_TURN|--bus_turn [value]

 HBM2 Example usage: ./amdmemtool -i 0,3,5 --faw 12 --RFC 208
 GDDR5 Example usage: ./amdmemtool -i 1,2,4 --RFC 43 --ras2ras 176

## Some extra info
Not all possible timings have been exposed.
However, it's not such a big deal to add more of them in the tool.
The ones available are more or like the most important ones.

Have fun!

Cheers


## Tips (Although i'm not expecting too much..)
BTC
  * 3GBgapb49BZ7fBPXnbetqbnMn2KiGNzUXf
  
ETH
  * 0x8C77C212da3e12cad1AfB8824CF74b1CC04d2F7C
  
In the unlikely event of not owning either BTC or ETH and you do want to be an amazing person and tip,
shapeshift, changelly, simpleswap, ... are great ways to solve that "issue" ;-)
