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
