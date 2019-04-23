# AMD Memory Tweak
---
#### Read and modify memory timings "on the fly"
---

### Support

  - GDDR5 Based AMD GPU's
  - ~~HBM~~(later) HBM2 Based AMD GPU's
  - Linux
  - Windows (Beta)

# Requirements

  - One or more AMD Radeon GPU's
  - amdgpu-pro | ROCM (Verified working on amdgpu-pro 18.30)
  - Adrenaline (Verified working on v19.4.1)

# Build (Linux)
Prerequisites:
  - [pciutils-dev](https://pkgs.org/download/pciutils-dev) | [libpci-dev](https://pkgs.org/download/libpci-dev)
  - [build-essential](https://pkgs.org/download/build-essential)
  - [git]()

Building:
```sh
$ git clone https://github.com/Eliovp/amdmemorytweak
$ cd amdmemorytweak/linux
$ g++ AmdMemTweak.cpp -lpci -lresolv -o amdmemtweak
```

# Build (Windows)
  - Clone repo
  - Launch VS Project
  - Build!

# Usage & Instructions

##### Global command line options
##

| Command | User Input | Extra Info |
| ------ | ------ | ------ |
| - -help |  | Show this output |
| - -gpu\|- -i | Comma-Seperated gpu indices | Selected device(s) |
| - -current |  | List current twiming values |

##### Command line options: (HBM2)
##
| Command | User Input | Extra Info |
| ------ | ------ | ------ |
| - -CL\|- -cl | [value] | Cas Latency |
| - -RAS\|- -ras | [value] | Active to PRECHARGE command period |
| - -RCDRD\|- -rcdrd | [value] | Active to READ command delay |
| - -RCDWR\|- -rcdwr | [value] | Active to WRITE command delay |
| - -RC\|- -rc | [value] | Active to Active command period |
| - -RP\|- -rp | [value] | Precharge command period |
| - -RRDS\|- -rrds | [value] | Active bank A to Active or Single bank Refresh bank B command delay different bank group |
| - -RRDL\|- -rrdl | [value] | Active bank A to Active or Single Bank Refresh bank B command delay same bank group |
| - -RTP\|- -rtp | [value] | Read to precharge delay |
| - -FAW\|- -faw | [value] | Four Active Window |
| - -CWL\|- -cwl | [value] | |
| - -WTRS\|- -wtrs | [value] | Write to read delay |
| - -WTRL\|- -wtrl | [value] | tWTR = tWTRL when bank groups is enabled and both WRITE and READ |
| - -WR\|- -wr | [value] | Write Recovery Time |
| - -RREFD\|- -rrefd | [value] | |
| - -RDRDDD\|- -rdrddd | [value] | |
| - -RDRDSD\|- -rdrdsd | [value] | |
| - -RDRDSC\|- -rdrdsc | [value] | |
| - -RDRDSCL\|- -rdrdscl | [value] | |
| - -WRWRDD\|- -wrwrdd | [value] | |
| - -WRWRSD\|- -wrwrsd | [value] | |
| - -WRWRSC\|- -wrwrsc | [value] | |
| - -WRWRSCL\|- -wrwrscl | [value] | |
| - -WRRD\|- -wrrd | [value] | |
| - -RDWR\|- -rdwr | [value] | |
| - -REF\|- -ref | [value] | Average Periodic Refresh Interval |
| - -MRD\|- -mrd | [value] | Mode Register Set command cycle time |
| - -MOD\|- -mod | [value] | Mode Register Set command update delay |
| - -XS\|- -xs | [value] | Self refresh exit period |
| - -XSMRS\|- -xsmrs | [value] | |
| - -PD\|- -pd | [value] | Power down entry to exit time |
| - -CKSRE\|- -cksre | [value] | Valid CK Clock required after self refresh or power-down entry |
| - -CKSRX\|- -cksrx | [value] | Valid CK Clock required before self refresh power down exit |
| - -RFCPB\|- -rfcpb | [value] | |
| - -STAG\|- -stag | [value] | |
| - -XP\|- -xp | [value] | |
| - -CPDED\|- -cpded | [value] | |
| - -CKE\|- -cke | [value] | |
| - -RDDATA\|- -rddata | [value] | |
| - -WRLAT\|- -wrlat | [value] | |
| - -RDLAT\|- -rdlat | [value] | |
| - -WRDATA\|- -wrdata | [value] | |
| - -CKESTAG\|- -ckestag | [value] | |
| - -RFC\|- -rfc | [value] | Auto Refresh Row Cycle Time |

##### Command line options: (GDDR5)
##
| Command | User Input | Extra Info |
| ------ | ------ | ------ |
| - -CKSRE\|- -cksre | [value] |  |
| - -CKSRX\|- -cksrx | [value] |  |
| - -CKE_PULSE\|- -cke_pulse | [value] |  |
| - -CKE\|- -cke | [value] |  |
| - -SEQ_IDLE\|- -seq_idle | [value] |  |
| - -CL\|- -cl | [value] | CAS to data return latency |
| - -W2R\|- -w2r | [value] | Write to read turn |
| - -R2R\|- -r2r | [value] | Read to read time |
| - -CCLD\|- -ccld | [value] | Cycles between r/w from bank A to r/w bank B |
| - -R2W\|- -r2w | [value] | Read to write turn |
| - -NOPR\|- -nopr | [value] | Extra cycle(s) between successive read bursts |
| - -NOPW\|- -nopw | [value] | Extra cycle(s) between successive write bursts |
| - -RCDW\|- -rcdw | [value] | # of cycles from active to write |
| - -RCDWA\|- -rcdwa | [value] | # of cycles from active to write with auto-precharge |
| - -RCDR\|- -rcdr | [value] | # of cycles from active to read |
| - -RCDRA\|- -rcdra | [value] | # of cycles from active to read with auto-precharge |
| - -RRD\|- -rrd | [value] | # of cycles from active bank a to active bank b |
| - -RC\|- -rc | [value] | # of cycles from active to active/auto refresh |
| - -RFC\|- -rfc | [value] | Auto-refresh command period |
| - -TRP\|- -trp | [value] | Precharge command period |
| - -RP_WRA\|- -rp_wra | [value] | From write with auto-precharge to active |
| - -RP_RDA\|- -rp_rda | [value] | From read with auto-precharge to active |
| - -WDATATR\|- -wdatatr | [value] |  |
| - -T32AW\|- -t32aw | [value] |  |
| - -CRCWL\|- -crcwl | [value] |  |
| - -CRCRL\|- -crcrl | [value] |  |
| - -FAW\|- -faw | [value] |  |
| - -PA2WDATA\|- -pa2wdata | [value] |  |
| - -PA2RDATA\|- -pa2rdata | [value] |  |
| - -RAS\|- -ras | [value] |  |
| - -ACTRD\|- -actrd | [value] |  |
| - -ACTWR\|- -actwr | [value] |  |
| - -RASMACTRD\|- -rasmactrd | [value] |  |
| - -RASMACWTR\|- -rasmacwtr | [value] |  |
| - -RAS2RAS\|- -ras2ras | [value] |  |
| - -RP\|- -rp | [value] |  |
| - -WRPLUSRP\|- -wrplusrp | [value] |  |
| - -BUS_TURN\|- -bus_turn | [value] |  |

##### Example Usage (Linux):
##
```sh
$ sudo ./amdmemtool --i 0,3,5 --faw 100 --RFC 100
```

##### Example Usage (Windows):
##
    C:\Users\You\Desktop\WinAMDTweak.exe --i 1,2,4 --rfc 100 --RC 100

(These are just examples! Don't try these at home! :p)


## Some extra info
Not all possible timings have been exposed.
However, it's not such a big deal to add more of them in the tool.
The ones available are more or like the most important ones.

Some users have reported very nice results already, please continue to contribute to these results.
[Example](https://bitcointalk.org/index.php?topic=5123724.msg50562384#msg50562384)

Have fun!

Cheers


## Tips
- 3GBgapb49BZ7fBPXnbetqbnMn2KiGNzUXf
- 0x8C77C212da3e12cad1AfB8824CF74b1CC04d2F7C
  
> In the unlikely event of not owning either BTC or ETH and you do want to be an amazing person and tip,
> shapeshift, changelly, simpleswap, ... are great ways to solve that "issue" ;-)

### Todos

 - Fix HBM gen1
 - ...

License
----

##### GPL
