/*
	AMD Memory Tweak by
	Elio VP                 <elio@eliovp.be>
	A. Solodovnikov
	Copyright (c) 2019 Eliovp, BVBA. All rights reserved.
	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute and/or sublicense
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:
	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
*/

#include <sstream>
#include <iostream>
#include <string.h> // strcasecmp
#include <stdlib.h> // strtoul
#include <stdio.h> // printf
#include <errno.h>
#include <time.h> // nanosleep
#include <sys/mman.h> // mmap
#include <unistd.h> // close lseek read write
#include <fcntl.h> // open
#include <dirent.h> // opendir readdir closedir

extern "C" {
#include "pci/pci.h" // full path /usr/local/include/pci/pci.h
}

#ifndef _countof
#define _countof(a) (sizeof(a)/sizeof(*(a)))
#endif

// from /include/linux/pci_ids.h
#define PCI_VENDOR_ID_AMD 0x1022
#define PCI_VENDOR_ID_ATI 0x1002

#define VERSION "AMD Memory Tweak Linux CLI version 0.1.8\n"

bool IsAmdDisplayDevice(struct pci_dev* dev)
{
	if ((dev->device_class >> 8) == PCI_BASE_CLASS_DISPLAY)
	{
		u8 headerType = pci_read_byte(dev, PCI_HEADER_TYPE);
		if ((headerType & 0x7F) == PCI_HEADER_TYPE_NORMAL)
		{
			if ((dev->vendor_id == PCI_VENDOR_ID_ATI) ||
				(dev->vendor_id == PCI_VENDOR_ID_AMD))
			{
				return true;
			}
		}
	}
	return false;
}

bool IsRelevantDeviceID(struct pci_dev* dev)
{
	return
		(dev->device_id == 0x66af) || // Radeon VII
		(dev->device_id == 0x687f) || // Vega 10 XL/XT [Radeon RX Vega 56/64]
		(dev->device_id == 0x6867) || // Vega 10 XL [Radeon Pro Vega 56]
		(dev->device_id == 0x6863) || // Vega 10 XTX [Radeon Vega Frontier Edition]
		(dev->device_id == 0x67df) || // Ellesmere [Radeon RX 470/480/570/570X/580/580X/590]
		(dev->device_id == 0x67c4) || // Ellesmere [Radeon Pro WX 7100]
		(dev->device_id == 0x67c7) || // Ellesmere [Radeon Pro WX 5100]
		(dev->device_id == 0x67ef) || // Baffin [Radeon RX 460/560D / Pro 450/455/460/555/555X/560/560X]
		(dev->device_id == 0x67ff) || // Baffin [Radeon RX 550 640SP / RX 560/560X]
		(dev->device_id == 0x7300) || // Fiji [Radeon R9 FURY / NANO Series]
		(dev->device_id == 0x67b0) || // Hawaii XT / Grenada XT [Radeon R9 290X/390X]
		(dev->device_id == 0x67b1) || // Hawaii PRO [Radeon R9 290/390]
		(dev->device_id == 0x6798) || // Tahiti XT [Radeon HD 7970/8970 OEM / R9 280X]
		(dev->device_id == 0x679a); // Tahiti PRO [Radeon HD 7950/8950 OEM / R9 280]
}

static bool IsR9(struct pci_dev* dev)
{
	return
		(dev->device_id == 0x67b0) || // Hawaii XT / Grenada XT [Radeon R9 290X/390X]
		(dev->device_id == 0x67b1) || // Hawaii PRO [Radeon R9 290/390]
		(dev->device_id == 0x6798) || // Tahiti XT [Radeon HD 7970/8970 OEM / R9 280X]
		(dev->device_id == 0x679a); // Tahiti PRO [Radeon HD 7950/8950 OEM / R9 280]
}

typedef enum { GDDR5, HBM, HBM2 } MemoryType;

static MemoryType DetermineMemoryType(struct pci_dev* dev)
{
	struct {
		u16 vendor_id;
		u16 device_id;
		MemoryType memory_type;
	} KnownGPUs[] = {
		/* Vega20 - Radeon VII */
		{ 0x1002, 0x66a0, HBM2 }, // "Radeon Instinct", CHIP_VEGA20
		{ 0x1002, 0x66a1, HBM2 }, // "Radeon Vega20", CHIP_VEGA20
		{ 0x1002, 0x66a2, HBM2 }, // "Radeon Vega20", CHIP_VEGA20
		{ 0x1002, 0x66a3, HBM2 }, // "Radeon Vega20", CHIP_VEGA20
		{ 0x1002, 0x66a4, HBM2 }, // "Radeon Vega20", CHIP_VEGA20
		{ 0x1002, 0x66a7, HBM2 }, // "Radeon Pro Vega20", CHIP_VEGA20
		{ 0x1002, 0x66af, HBM2 }, // "Radeon Vega20", CHIP_VEGA20
		{ 0x1002, 0x66af, HBM2 }, // "Radeon VII", CHIP_VEGA20
		/* Vega */
		{ 0x1002, 0x687f, HBM2 }, // "Radeon RX Vega 56/64", CHIP_VEGA10
		{ 0x1002, 0x6863, HBM2 }, // "Radeon Vega Frontier Edition", CHIP_VEGA10
		/* Fury/Nano */
		{ 0x1002, 0x7300, HBM }, // "Radeon R9 Fury/Nano/X", CHIP_FIJI
	};
	for (int i = 0; i < _countof(KnownGPUs); i++)
	{
		if ((KnownGPUs[i].vendor_id == dev->vendor_id) && (KnownGPUs[i].device_id == dev->device_id))
		{
			return KnownGPUs[i].memory_type;
		}
	}
	return GDDR5;
}

#define AMD_TIMING_REGS_BASE_1 0x50200
#define AMD_TIMING_REGS_BASE_2 0x52200
#define AMD_TIMING_REGS_BASE_3 0x54200
#define AMD_TIMING_REGS_BASE_4 0x56200

typedef union {
	u32 value;
	struct {
		u32 : 8;
			  u32 MAN : 4;
			  u32 VEN : 4;
			  u32 : 16;
	} rx;
	struct {
		u32 : 24;
			  u32 MAN : 8;
	} hbm;
} MANUFACTURER;
#define MANUFACTURER_ID 0x2A00
#define MANUFACTURER_ID_HBM 0x29C4
#define MANUFACTURER_ID_HBM2 0x5713C

// Split up GDDR5, HBM & HBM2 because the structures and registers are different
typedef struct {
	u32 frequency;
	// TIMING1
	u32 CL : 8;
	u32 RAS : 8;
	u32 RCDRD : 8;
	u32 RCDWR : 8;
	// TIMING2
	u32 RCAb : 8;
	u32 RCPb : 8;
	u32 RPAb : 8;
	u32 RPPb : 8;
	// TIMING3
	u32 RRDS : 8;
	u32 RRDL : 8;
	u32 : 8;
	u32 RTP : 8;
	// TIMING4
	u32 FAW : 8;
	u32 : 24;
	// TIMING5
	u32 CWL : 8;
	u32 WTRS : 8;
	u32 WTRL : 8;
	u32 : 8;
	// TIMING6
	u32 WR : 8;
	u32 : 24;
	// TIMING7
	u32 : 8;
	u32 RREFD : 8;
	u32 : 8;
	u32 : 8;
	// TIMING8
	u32 RDRDDD : 8;
	u32 RDRDSD : 8;
	u32 RDRDSC : 8;
	u32 RDRDSCL : 6;
	u32 : 2;
	// TIMING9
	u32 WRWRDD : 8;
	u32 WRWRSD : 8;
	u32 WRWRSC : 8;
	u32 WRWRSCL : 6;
	u32 : 2;
	// TIMING10
	u32 WRRD : 8;
	u32 RDWR : 8;
	u32 : 16;
	// PADDING
	u32 : 32;
	// TIMING12
	u32 REF : 16;           // Determines at what rate refreshes will be executed.
	u32 : 16;
	// TIMING13
	u32 MRD : 8;
	u32 MOD : 8;
	u32 : 16;
	// TIMING14
	u32 XS : 16;            // self refresh exit period
	u32 : 16;
	// PADDING
	u32 : 32;
	// TIMING16
	u32 XSMRS : 16;
	u32 : 16;
	// TIMING17
	u32 PD : 4;
	u32 CKSRE : 6;
	u32 CKSRX : 6;
	u32 : 16;
	// PADDING
	u32 : 32;
	// PADDING
	u32 : 32;
	// TIMING20
	u32 RFCPB : 16;
	u32 STAG : 8;
	u32 : 8;
	// TIMING21
	u32 XP : 8;
	u32 : 8;
	u32 CPDED : 8;
	u32 CKE : 8;
	// TIMING22
	u32 RDDATA : 8;
	u32 WRLAT : 8;
	u32 RDLAT : 8;
	u32 WRDATA : 4;
	u32 : 4;
	// TIMING23
	u32 : 16;
	u32 CKESTAG : 8;
	u32 : 8;
	// RFC
	u32 RFC : 16;
	u32 : 16;
} HBM2_TIMINGS;

/*
typedef union {
	u32 value;
	struct {
		u32 RCV_DLY : 3;
		u32 RCV_EXT : 5;
		u32 RST_SEL : 2;
		u32 RXDPWRON_DLY : 2;
		u32 RST_HLD : 4;
		u32 STR_PRE : 1;
		u32 STR_PST : 1;
		u32 : 2;
		u32 RBS_DLY : 5;
		u32 RBS_WEDC_DLY : 5;
		u32 : 2;
	};
} SEQ_RD_CTL_D0;
#define MC_SEQ_RD_CTL_D0 0x28b4
*/

typedef union {
	u32 value;
	struct {
		u32 CKSRE : 3; // Valid clock requirement after CKSRE
		u32 : 1;
		u32 CKSRX : 3; // Valid clock requirement before CKSRX
		u32 : 1;
		u32 CKE_PULSE : 4; // Minimum CKE pulse
		u32 CKE : 6;
		u32 SEQ_IDLE : 3; // idle before deassert rdy to arb
		u32 : 2;
		u32 CKE_PULSE_MSB : 1; // Minimum CKE pulse msb
		u32 SEQ_IDLE_SS : 8; // idle before deassert rdy to arb at ss
	} rx;
	struct {
		u32 CKSRE : 3; // Valid clock requirement after CKSRE
		u32 CKSRX : 3; // Valid clock requirement before CKSRX
		u32 CKE_PULSE : 5; // Minimum CKE pulse
		u32 CKE : 8;
		u32 SEQ_IDLE : 3; // idle before deassert rdy to arb
		u32 SEQ_IDLE_SS : 8; // idle before deassert rdy to arb at ss
		u32 : 2;
	} hbm;
} SEQ_PMG_TIMING;
#define MC_SEQ_PMG_TIMING 0x28B0 // Power Management
#define MC_SEQ_PMG_TIMING_LP 0x2B4C // Power Management S0 Power State
#define MC_SEQ_PMG_TIMING_HBM 0x28C4 // Power Management

typedef union {
	u32 value;
	struct {
		u32 RCDW : 5;   // # of cycles from active to write
		u32 RCDWA : 5;  // # of cycles from active to write with auto-precharge
		u32 RCDR : 5;   // # of cycles from active to read
		u32 RCDRA : 5;  // # of cycles from active to read with auto-precharge
		u32 RRD : 4;    // # of cycles from active bank a to active bank b
		u32 RC : 7;     // # of cycles from active to active/auto refresh
		u32 : 1;
	};
} SEQ_RAS_TIMING;
#define MC_SEQ_RAS_TIMING 0x28A0
#define MC_SEQ_RAS_TIMING_LP 0x2A6C
#define MC_SEQ_RAS_TIMING_HBM 0x28A4

typedef union {
	u32 value;
	struct {
		u32 NOPW : 2;   // Extra cycle(s) between successive write bursts
		u32 NOPR : 2;   // Extra cycle(s) between successive read bursts
		u32 R2W : 5;    // Read to write turn around time
		u32 CCDL : 3;   // Cycles between r/w from bank A to r/w bank B
		u32 R2R : 4;    // Read to read time
		u32 W2R : 5;    // Write to read turn around time
		u32 : 3;
		u32 CL : 5;     // CAS to data return latency (0 - 20)
		u32 : 3;
	} rx;
	struct {
		u32 NOPW : 2;   // Extra cycle(s) between successive write bursts
		u32 NOPR : 2;   // Extra cycle(s) between successive read bursts
		u32 R2W : 5;    // Read to write turn
		u32 CCDL : 3;   // Cycles between r/w from bank A to r/w bank B
		u32 R2R : 4;    // Read to read time
		u32 W2R : 5;    // Write to read turn
		u32 CL : 5;     // CAS to data return latency
		u32 : 6;
	} hbm;
} SEQ_CAS_TIMING;
#define MC_SEQ_CAS_TIMING 0x28A4
#define MC_SEQ_CAS_TIMING_LP 0x2A70
#define MC_SEQ_CAS_TIMING_HBM 0x28AC

typedef union {
	u32 value;
	struct {
		u32 RP_WRA : 6; // From write with auto-precharge to active
		u32 : 2;
		u32 RP_RDA : 6; // From read with auto-precharge to active
		u32 : 1;
		u32 TRP : 5;    // Precharge command period
		u32 RFC : 9;    // Auto-refresh command period
		u32 : 3;
	} rx;
	struct {
		u32 RP_WRA : 8; // From write with auto-precharge to active
		u32 RP_RDA : 7; // From read with auto-precharge to active
		u32 TRP : 5;    // Precharge command period
		u32 RFC : 9;    // Auto-refresh command period
		u32 : 3;
	} r9;
	struct {
		u32 RP_WRA : 6; // From write with auto-precharge to active
		u32 RP_RDA : 6; // From read with auto-precharge to active
		u32 TRP : 5;    // Precharge command period
		u32 RFC : 7;    // Auto-refresh command period
		u32 RRDL : 4;
		u32 MRD : 4;
	} hbm;
} SEQ_MISC_TIMING;
#define MC_SEQ_MISC_TIMING 0x28A8
#define MC_SEQ_MISC_TIMING_LP 0x2A74
#define MC_SEQ_MISC_TIMING_HBM 0x28B4

typedef union {
	u32 value;
	struct {
		u32 PA2RDATA : 3;       // DDR4
		u32 : 1;
		u32 PA2WDATA : 3;       // DDR4
		u32 : 1;
		u32 FAW : 5;            // The time window in wich four activates are allowed in the same rank
		u32 REDC : 3;           // Min 0, Max 7
		u32 WEDC : 5;           // Min 0, Max 7
		u32 T32AW : 4;          // Max 12
		u32 : 3;
		u32 WDATATR : 4;        // WCMD timing for write training
	} rx;
	struct {
		u32 PA2RDATA : 3;
		u32 PA2WDATA : 3;
		u32 FAW : 5;            // The time window in wich four activates are allowed in the same rank
		u32 WPAR : 3;
		u32 RPAR : 3;
		u32 T32AW : 4;
		u32 WDATATR : 4;
		u32 : 7;
	} hbm;
} SEQ_MISC_TIMING2;
#define MC_SEQ_MISC_TIMING2 0x28AC
#define MC_SEQ_MISC_TIMING2_LP 0x2A78
#define MC_SEQ_MISC_TIMING2_HBM 0x28BC

/* typedef union {
	u32 value;
	struct {
		u32 : 22;
		u32 RAS : 6;
		u32 : 4;
	};
} SEQ_MISC3;
#define MC_SEQ_MISC3 0x2A2C */

typedef union {
	u32 value;
	struct {
		u32 ACTRD : 8;
		u32 ACTWR : 8;
		u32 RASMACTRD : 8;
		u32 RASMACTWR : 8;
	};
} ARB_DRAM_TIMING;
#define MC_ARB_DRAM_TIMING 0x2774

typedef union {
	u32 value;
	struct {
		u32 RAS2RAS : 8;
		u32 RP : 8;
		u32 WRPLUSRP : 8;
		u32 BUS_TURN : 8;
	};
} ARB_DRAM_TIMING2;
#define MC_ARB_DRAM_TIMING2 0x2778

typedef union {
	u32 value;
	struct {
		u32 REF : 16;
		u32 : 16;
	};
} ARB_RFSH_RATE;
#define MC_ARB_RFSH_RATE 0x27B0

typedef union {
	u32 value;
	struct {
		u32 TWT2RT : 5;	 // number of tck cycles from write train to read train commands.Actual number is max(2, TWT2RT + 1).
		u32 TARF2T : 5;	// number of tck cycles from aurto-refresh command to a train command 
		u32 TT2ROW : 5;	
		u32 TLD2LD : 5;	// number of mclk cycles between LDFF commands
		u32 : 12; // woops :p Thx Doktor83  ;-) (Community effort for the win!)
	};
} SEQ_TRAIN_TIMING;
#define MC_SEQ_TRAIN_TIMING 0x2900

typedef union {
	u32 value;
	struct {
		u32 ENB : 1;
		u32 CNT : 5;
		u32 TRC : 16;
		u32 : 10;
	};
} SEQ_ROW_HAMMER;
#define MC_SEQ_ROW_HAMMER 0x27B0


static u64 ParseIndicesArg(const char* arg)
{
	u64 mask = 0;
	std::istringstream ss(arg);
	while (!ss.eof())
	{
		std::string token;
		std::getline(ss, token, ',');
		unsigned long index = strtoul(token.c_str(), 0, 10);
		if ((errno == EINVAL) || (errno == ERANGE) || (index >= 64))
		{
			std::cout << "Invalid GPU index specified.\n";
			return 1;
		}
		mask |= ((u64)1 << index);
	}
	return mask;
}

static bool ParseNumericArg(int argc, const char* argv[], int& i, const char* arg, u32 & value)
{
	if (!strcasecmp(arg, argv[i]))
	{
		if (i == (argc - 1))
		{
			std::cout << "Argument \"" << argv[i] << "\" requires a parameter.\n";
			exit(1);
		}
		i++;
		value = strtoul(argv[i], 0, 10);
		if ((errno == EINVAL) || (errno == ERANGE))
		{
			std::cout << "Failed to parse parameter " << argv[i - 1] << " " << argv[i] << "\n";
			exit(1);
		}
		return true;
	}
	return false;
}

static int pci_find_instance(char* pci_string)
{
	DIR* dir = opendir("/sys/kernel/debug/dri");
	if (!dir)
	{
		perror("Couldn't open DRI under debugfs\n");
		return -1;
	}

	struct dirent* entry;
	while (entry = readdir(dir))
	{
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;

		char name[300];
		snprintf(name, sizeof(name) - 1, "/sys/kernel/debug/dri/%s/name", entry->d_name);
		FILE * f = fopen(name, "r");
		if (!f) continue;

		char device[256];
		device[sizeof(device) - 1] = 0;
		int parsed_device = fscanf(f, "%*s %255s", device);
		fclose(f);

		if (parsed_device != 1) continue;

		// strip off dev= for kernels > 4.7
		if (strstr(device, "dev="))
		{
			memmove(device, device + 4, strlen(device) - 3);
		}

		if (!strcmp(pci_string, device))
		{
			closedir(dir);
			return atoi(entry->d_name);
		}
	}
	closedir(dir);
	return -1;
}

typedef struct {
	struct pci_dev* dev;
	int mmio;

	char log[1000];
	bool modify[25];
	// HBM2
	HBM2_TIMINGS hbm2;
	// GDDR5
	//SEQ_RD_CTL_D0 d0;
	SEQ_PMG_TIMING pmg;
	SEQ_RAS_TIMING ras;
	SEQ_CAS_TIMING cas;
	SEQ_MISC_TIMING misc;
	SEQ_MISC_TIMING2 misc2;
	/* SEQ_MISC3 misc3; */
	ARB_DRAM_TIMING dram1;
	ARB_DRAM_TIMING2 dram2;
	ARB_RFSH_RATE ref;
	SEQ_TRAIN_TIMING train;
	MANUFACTURER man;
	// HBM1 specific
	SEQ_ROW_HAMMER ham;
} GPU;

static int gpu_compare(const void* A, const void* B)
{
	const struct pci_dev* a = ((const GPU*)A)->dev;
	const struct pci_dev* b = ((const GPU*)B)->dev;
	if (a->domain < b->domain) return -1;
	if (a->domain > b->domain) return 1;
	if (a->bus < b->bus) return -1;
	if (a->bus > b->bus) return 1;
	if (a->dev < b->dev) return -1;
	if (a->dev > b->dev) return 1;
	if (a->func < b->func) return -1;
	if (a->func > b->func) return 1;
	return 0;
}

static void PrintCurrentValues(GPU * gpu)
{
	fprintf(stdout, "pci:%04x:%02x:%02x.%d\n", gpu->dev->domain, gpu->dev->bus, gpu->dev->dev, gpu->dev->func);
	fflush(stdout);
	if (DetermineMemoryType(gpu->dev) == HBM2)
	{
		HBM2_TIMINGS current = gpu->hbm2;
		std::cout << "Memory state: " << std::dec;
		std::cout <<
			((current.frequency == 0x118) ? "800MHz" :
			(current.frequency == 0x11C) ? "1000MHz" :
				(current.frequency == 0x11E) ? "1200MHz" : "Unknown") << std::endl;
		MANUFACTURER man = gpu->man;
		std::cout << "Memory: " << std::dec;
		std::cout <<
			((man.hbm.MAN == 0x63) ? "Hynix HBM2" :
			(man.hbm.MAN == 0x61) ? "Samsung HBM2" : "Unknown") << std::endl;
		std::cout << "Timing 1:\t";
		std::cout << "  CL: " << current.CL << "\t";
		std::cout << "  RAS: " << current.RAS << "\t";
		std::cout << "  RCDRD: " << current.RCDRD << "\t";
		std::cout << "  RCDWR: " << current.RCDWR << "\n";
		std::cout << "Timing 2:\t";
		std::cout << "  RCAb (RC): " << current.RCAb << "\t";
		std::cout << "  RCPb (RC): " << current.RCPb << "\t";
		std::cout << "  RPAb (RP): " << current.RPAb << "\t";
		std::cout << "  RPPb (RP): " << current.RPPb << "\n";
		std::cout << "Timing 3:\t";
		std::cout << "  RRDS: " << current.RRDS << "\t";
		std::cout << "  RRDL: " << current.RRDL << "\t";
		std::cout << "  RTP: " << current.RTP << "\n";
		std::cout << "Timing 4:\t";
		std::cout << "  FAW: " << current.FAW << "\n";
		std::cout << "Timing 5:\t";
		std::cout << "  CWL: " << current.CWL << "\t";
		std::cout << "  WTRS: " << current.WTRS << "\t";
		std::cout << "  WTRL: " << current.WTRL << "\n";
		std::cout << "Timing 6:\t";
		std::cout << "  WR: " << current.WR << "\n";
		std::cout << "Timing 7:\t";
		std::cout << "  RREFD: " << current.RREFD << "\n";
		std::cout << "Timing 8:\t";
		std::cout << "  RDRDDD: " << current.RDRDDD << "\t";
		std::cout << "  RDRDSD: " << current.RDRDSD << "\t";
		std::cout << "  RDRDSC: " << current.RDRDSC << "\t";
		std::cout << "  RDRDSCL: " << current.RDRDSCL << "\n";
		std::cout << "Timing 9:\t";
		std::cout << "  WRWRDD: " << current.WRWRDD << "\t";
		std::cout << "  WRWRSD: " << current.WRWRSD << "\t";
		std::cout << "  WRWRSC: " << current.WRWRSC << "\t";
		std::cout << "  WRWRSCL: " << current.WRWRSCL << "\n";
		std::cout << "Timing 10:\t";
		std::cout << "  WRRD: " << current.WRRD << "\t";
		std::cout << "  RDWR: " << current.RDWR << "\n";
		std::cout << "Timing 12:\t";
		std::cout << "  REF: " << current.REF << "\n";
		std::cout << "Timing 13:\t";
		std::cout << "  MRD: " << current.MRD << "\t";
		std::cout << "  MOD: " << current.MOD << "\n";
		std::cout << "Timing 14:\t";
		std::cout << "  XS: " << current.XS << "\n";
		std::cout << "Timing 16:\t";
		std::cout << "  XSMRS: " << current.XSMRS << "\n";
		std::cout << "Timing 17:\t";
		std::cout << "  PD: " << current.PD << "  \t";
		std::cout << "  CKSRE: " << current.CKSRE << "\t";
		std::cout << "  CKSRX: " << current.CKSRX << "\n";
		std::cout << "Timing 20:\t";
		std::cout << "  RFCPB: " << current.RFCPB << "\t";
		std::cout << "  STAG: " << current.STAG << "\n";
		std::cout << "Timing 21:\t";
		std::cout << "  XP: " << current.XP << "  \t";
		std::cout << "  CPDED: " << current.CPDED << "\t";
		std::cout << "  CKE: " << current.CKE << "\n";
		std::cout << "Timing 22:\t";
		std::cout << "  RDDATA: " << current.RDDATA << "\t";
		std::cout << "  WRLAT: " << current.WRLAT << "\t";
		std::cout << "  RDLAT: " << current.RDLAT << "\t";
		std::cout << "  WRDATA: " << current.WRDATA << "\n";
		std::cout << "Timing 23:\t";
		std::cout << "  CKESTAG: " << current.CKESTAG << "\n";
		std::cout << "RFC Timing:\t";
		std::cout << "  RFC: " << current.RFC << "\n";
		std::cout << "\n";
	}
	else if (DetermineMemoryType(gpu->dev) == HBM) {
		MANUFACTURER man = gpu->man;
		std::cout << "Memory: " << std::dec;
		std::cout <<
			((man.hbm.MAN == 0x63) ? "Hynix HBM" :
			(man.hbm.MAN == 0x61) ? "Samsung HBM" : "Unknown") << std::endl;
		printf("Timings\n");
		printf("PMG:\t");
		printf("  CKSRE: %d\t", gpu->pmg.hbm.CKSRE);
		printf("  CKSRX: %d\t", gpu->pmg.hbm.CKSRX);
		printf("  CKE_PULSE: %d\t", gpu->pmg.hbm.CKE_PULSE);
		printf("  CKE: %d\t", gpu->pmg.hbm.CKE);
		printf("  SEQ_IDLE: %d\t", gpu->pmg.hbm.SEQ_IDLE);
		printf("  SEQ_IDLE_SS: %d\n", gpu->pmg.hbm.SEQ_IDLE_SS);
		printf("RAS:\t");
		printf("  RCDW: %d\t", gpu->ras.RCDW);
		printf("  RCDWA: %d\t", gpu->ras.RCDWA);
		printf("  RCDR: %d\t", gpu->ras.RCDR);
		printf("  RCDRA: %d\t", gpu->ras.RCDRA);
		printf("  RRD: %d\t", gpu->ras.RRD);
		printf("  RC: %d\n", gpu->ras.RC);
		printf("CAS:\t");
		printf("  NOPW: %d\t", gpu->cas.hbm.NOPW);
		printf("  NOPR: %d\t", gpu->cas.hbm.NOPR);
		printf("  R2W: %d\t", gpu->cas.hbm.R2W);
		printf("  CCDL: %d\t", gpu->cas.hbm.CCDL);
		printf("  R2R: %d\t", gpu->cas.hbm.R2R);
		printf("  W2R: %d\t", gpu->cas.hbm.W2R);
		printf("  CL: %d\n", gpu->cas.hbm.CL);
		printf("MISC:\t");
		printf("  RP_WRA: %d\t", gpu->misc.hbm.RP_WRA);
		printf("  RP_RDA: %d\t", gpu->misc.hbm.RP_RDA);
		printf("  TRP: %d\t", gpu->misc.hbm.TRP);
		printf("  RFC: %d\t", gpu->misc.hbm.RFC);
		printf("  RRDL: %d\t", gpu->misc.hbm.RRDL);
		printf("  MRD: %d\n", gpu->misc.hbm.MRD);
		printf("MISC2:\t");
		printf("  PA2RDATA: %d\t", gpu->misc2.hbm.PA2RDATA);
		printf("  PA2WDATA: %d\t", gpu->misc2.hbm.PA2WDATA);
		printf("  FAW: %d\t", gpu->misc2.hbm.FAW);
		printf("  WPAR: %d\t", gpu->misc2.hbm.WPAR);
		printf("  RPAR: %d\t", gpu->misc2.hbm.RPAR);
		printf("  T32AW: %d\t", gpu->misc2.hbm.T32AW);
		printf("  WDATATR: %d\n", gpu->misc2.hbm.WDATATR);
		printf("REF:\t");
		printf("  REF: %d\n", gpu->ref.REF);
		printf("Hammer:\t");
		printf("  ENB: %d\t", gpu->ham.ENB);
		printf("  CNT: %d\t", gpu->ham.CNT);
		printf("  TRC: %d\n\n", gpu->ham.TRC);
	}
	else // GDDR5
	{
		MANUFACTURER man = gpu->man;
		std::cout << "Memory: " << std::dec;
		std::cout <<
			((man.rx.MAN == 0x1) ? "Samsung GDDR5" :
			(man.rx.MAN == 0x3) ? "Elpida GDDR5" :
			(man.rx.MAN == 0x6) ? "Hynix GDDR5" :
			(man.rx.MAN == 0xf) ? "Micron GDDR5" : "Unknown") << std::endl;
		/*printf("D0:\t");
		printf("  RCV_DLY: %d\t", gpu->d0.RCV_DLY);
		printf("  RCV_EXT: %d\t", gpu->d0.RCV_EXT);
		printf("  RST_SEL: %d\t", gpu->d0.RST_SEL);
		printf("  RXDPWRON_DLY: %d\t", gpu->d0.RXDPWRON_DLY);
		printf("  RST_HLD: %d\t", gpu->d0.RST_HLD);
		printf("  STR_PRE: %d\t", gpu->d0.STR_PRE);
		printf("  STR_PST: %d\t", gpu->d0.STR_PST);
		printf("  RBS_DLY: %d\t", gpu->d0.RBS_DLY);
		printf("  RBS_WEDC_DLY: %d\n", gpu->d0.RBS_WEDC_DLY);
		*/
		printf("Timings\n");
		printf("PMG:\t");
		printf("  CKSRE: %d\t", gpu->pmg.rx.CKSRE);
		printf("  CKSRX: %d\t", gpu->pmg.rx.CKSRX);
		printf("  CKE_PULSE: %d\t", gpu->pmg.rx.CKE_PULSE);
		printf("  CKE: %d\t", gpu->pmg.rx.CKE);
		printf("  SEQ_IDLE: %d\t", gpu->pmg.rx.SEQ_IDLE);
		printf("  CKE_PULSE_MSB: %d\t", gpu->pmg.rx.CKE_PULSE_MSB);
		printf("  SEQ_IDLE_SS: %d\n", gpu->pmg.rx.SEQ_IDLE_SS);
		printf("RAS:\t");
		printf("  RCDW: %d\t", gpu->ras.RCDW);
		printf("  RCDWA: %d\t", gpu->ras.RCDWA);
		printf("  RCDR: %d\t", gpu->ras.RCDR);
		printf("  RCDRA: %d\t", gpu->ras.RCDRA);
		printf("  RRD: %d\t", gpu->ras.RRD);
		printf("  RC: %d\n", gpu->ras.RC);
		printf("CAS:\t");
		printf("  NOPW: %d\t", gpu->cas.rx.NOPW);
		printf("  NOPR: %d\t", gpu->cas.rx.NOPR);
		printf("  R2W: %d\t", gpu->cas.rx.R2W);
		printf("  CCDL: %d\t", gpu->cas.rx.CCDL);
		printf("  R2R: %d\t", gpu->cas.rx.R2R);
		printf("  W2R: %d\t", gpu->cas.rx.W2R);
		printf("  CL: %d\n", gpu->cas.rx.CL);
		printf("MISC:\t");
		if (IsR9(gpu->dev))
		{
			printf("  RP_WRA: %d\t", gpu->misc.r9.RP_WRA);
			printf("  RP_RDA: %d\t", gpu->misc.r9.RP_RDA);
			printf("  TRP: %d\t", gpu->misc.r9.TRP);
			printf("  RFC: %d\n", gpu->misc.r9.RFC);
		}
		else
		{
			printf("  RP_WRA: %d\t", gpu->misc.rx.RP_WRA);
			printf("  RP_RDA: %d\t", gpu->misc.rx.RP_RDA);
			printf("  TRP: %d\t", gpu->misc.rx.TRP);
			printf("  RFC: %d\n", gpu->misc.rx.RFC);
		}
		printf("MISC2:\t");
		printf("  PA2RDATA: %d\t", gpu->misc2.rx.PA2RDATA);
		printf("  PA2WDATA: %d\t", gpu->misc2.rx.PA2WDATA);
		printf("  FAW: %d\t", gpu->misc2.rx.FAW);
		printf("  REDC: %d\t", gpu->misc2.rx.REDC);
		printf("  WEDC: %d\t", gpu->misc2.rx.WEDC);
		printf("  T32AW: %d\t", gpu->misc2.rx.T32AW);
		printf("  WDATATR: %d\n", gpu->misc2.rx.WDATATR);
		/*printf("M3(MR4):\t");         // todo later
		printf("  RAS: %d\n", gpu->misc3.RAS); */
		printf("DRAM1:\t");
		printf("  RASMACTWR: %d\t", gpu->dram1.RASMACTWR);
		printf("  RASMACTRD: %d\t", gpu->dram1.RASMACTRD);
		printf("  ACTWR: %d\t", gpu->dram1.ACTWR);
		printf("  ACTRD: %d\n", gpu->dram1.ACTRD);
		printf("DRAM2:\t");
		printf("  RAS2RAS: %d\t", gpu->dram2.RAS2RAS);
		printf("  RP: %d  \t", gpu->dram2.RP);
		printf("  WRPLUSRP: %d\t", gpu->dram2.WRPLUSRP);
		printf("  BUS_TURN: %d\n", gpu->dram2.BUS_TURN);
		printf("REF:\t");
		printf("  REF: %d\n", gpu->ref.REF);
		printf("TRAIN:\t");
		printf("  TWT2RT: %d\t", gpu->train.TWT2RT);
		printf("  TARF2T: %d\t", gpu->train.TARF2T);
		printf("  TT2ROW: %d\t", gpu->train.TT2ROW);
		printf("  TLD2LD: %d\n\n", gpu->train.TLD2LD);
	}
}

int main(int argc, const char* argv[])
{
	GPU gpus[64] = {};

	if ((argc < 2) || (0 == strcasecmp("--help", argv[1])))
	{
		std::cout << " AMD Memory Tweak\n"
			" Read and modify memory timings on the fly\n"
			" By Eliovp & A.Solodovnikov\n\n"
			" Global command line options:\n"
			" --help \tShow this output\n"
			" --version|--v\tShow version info\n"
			" --gpu|--i [comma-separated gpu indices]\tSelected device(s)\n"
			" --current \tList current timing values\n\n"
			" Command line options: (HBM2)\n"
			" --CL|--cl [value]\n"
			" --RAS|--ras [value]\n"
			" --RCDRD|--rcdrd [value]\n"
			" --RCDWR|--rcdwr [value]\n"
			" --RC|--rc [value]\n"
			" --RP|--rp [value]\n"
			" --RRDS|--rrds [value]\n"
			" --RRDL|--rrdl [value]\n"
			" --RTP|--rtp [value]\n"
			" --FAW|--faw [value]\n"
			" --CWL|--cwl [value]\n"
			" --WTRS|--wtrs [value]\n"
			" --WTRL|--wtrl [value]\n"
			" --WR|--wr [value]\n"
			" --RREFD|--rrefd [value]\n"
			" --RDRDDD|--rdrddd [value]\n"
			" --RDRDSD|--rdrdsd [value]\n"
			" --RDRDSC|--rdrdsc [value]\n"
			" --RDRDSCL|--rdrdscl [value]\n"
			" --WRWRDD|--wrwrdd [value]\n"
			" --WRWRSD|--wrwrsd [value]\n"
			" --WRWRSC|--wrwrsc [value]\n"
			" --WRWRSCL|--wrwrscl [value]\n"
			" --WRRD|--wrrd [value]\n"
			" --RDWR|--rdwr [value]\n"
			" --REF|--ref [value]\n"
			" --MRD|--mrd [value]\n"
			" --MOD|--mod [value]\n"
			" --XS|--xs [value]\n"
			" --XSMRS|--xsmrs [value]\n"
			" --PD|--pd [value]\n"
			" --CKSRE|--cksre [value]\n"
			" --CKSRX|--cksrx [value]\n"
			" --RFCPB|--rfcpb [value]\n"
			" --STAG|--stag [value]\n"
			" --XP|--xp [value]\n"
			" --CPDED|--cpded [value]\n"
			" --CKE|--cke [value]\n"
			" --RDDATA|--rddata [value]\n"
			" --WRLAT|--wrlat [value]\n"
			" --RDLAT|--rdlat [value]\n"
			" --WRDATA|--wrdata [value]\n"
			" --CKESTAG|--ckestag [value]\n"
			" --RFC|--rfc [value]\n\n"
			" Command line options: (HBM)\n"
			" --CKSRE|--cksre [value]\n"
			" --CKSRX|--cksrx [value]\n"
			" --CKE_PULSE|--cke_pulse [value]\n"
			" --CKE|--cke [value]\n"
			" --SEQ_IDLE|--seq_idle [value]\n"
			" --SEQ_IDLE_SS|--seq_idle_ss [value]\n"
			" --RCDW|--rcdw [value]\n"
			" --RCDWA|--rcdwa [value]\n"
			" --RCDR|--rcdr [value]\n"
			" --RCDRA|--rcdra [value]\n"
			" --RRD|--rrd [value]\n"
			" --RC|--rc [value]\n"
			" --NOPW|--nopw [value]\n"
			" --NOPR|--nopr [value]\n"
			" --R2W|--r2w [value]\n"
			" --CCDL|--ccdl [value]\n"
			" --R2R|--r2r [value]\n"
			" --W2R|--w2r [value]\n"
			" --CL|--cl [value]\n"
			" --RP_RDA|--rp_rda [value]\n"
			" --RP_WRA|--rp_wra [value]\n"
			" --TRP|--trp [value]\n"
			" --RFC|--rfc [value]\n"
			" --RRDL|--rrdl [value]\n"
			" --MRD|--mrd [value]\n"
			" --PA2RDATA|--pa2rdata [value]\n"
			" --PA2WDATA|--pa2wdata [value]\n"
			" --FAW|--faw [value]\n"
			" --WPAR|--wpar [value]\n"
			" --RPAR|--rpar [value]\n"
			" --T32AW|--t32aw [value]\n"
			" --WDATATR|--wdatatr [value]\n"
			" --REF|--ref [value]\n"
			" --ENB|--enb [value]\n"
			" --CNT|--cnt [value]\n"
			" --TRC|--trc [value]\n\n"
			" Command line options: (GDDR5)\n"
			" --CKSRE|--cksre [value]\n"
			" --CKSRX|--cksrx [value]\n"
			" --CKE_PULSE|--cke_pulse [value]\n"
			" --CKE|--cke [value]\n"
			" --SEQ_IDLE|--seq_idle [value]\n"
			" --CKE_PULSE_MSB|--cke_pulse_msb [value]\n"
			" --SEQ_IDLE_SS|--seq_idle_ss [value]\n"
			" --RCDW|--rcdw [value]\n"
			" --RCDWA|--rcdwa [value]\n"
			" --RCDR|--rcdr [value]\n"
			" --RCDRA|--rcdra [value]\n"
			" --RRD|--rrd [value]\n"
			" --RC|--rc [value]\n"
			" --NOPW|--nopw [value]\n"
			" --NOPR|--nopr [value]\n"
			" --R2W|--r2w [value]\n"
			" --CCDL|--ccdl [value]\n"
			" --R2R|--r2r [value]\n"
			" --W2R|--w2r [value]\n"
			" --CL|--cl [value]\n"
			" --RP_WRA|--rp_wra [value]\n"
			" --RP_RDA|--rp_rda [value]\n"
			" --TRP|--trp [value]\n"
			" --RFC|--rfc [value]\n"
			" --PA2RDATA|--pa2rdata [value]\n"
			" --PA2WDATA|--pa2wdata [value]\n"
			" --FAW|--faw [value]\n"
			" --REDC|--redc [value]\n"
			" --WEDC|--wedc [value]\n"
			" --T32AW|--t32aw [value]\n"
			" --WDATATR|--wdatatr [value]\n"
			//" --RAS|--ras [value]\n"
			" --ACTRD|--actrd [value]\n"
			" --ACTWR|--actwr [value]\n"
			" --RASMACTRD|--rasmactrd [value]\n"
			" --RASMACWTR|--rasmacwtr [value]\n"
			" --RAS2RAS|--ras2ras [value]\n"
			" --RP|--rp [value]\n"
			" --WRPLUSRP|--wrplusrp [value]\n"
			" --BUS_TURN|--bus_turn [value]\n"
			" --REF|--ref [value]\n\n"
			" HBM2 Example usage: ./amdmemtool --i 0,3,5 --faw 12 --RFC 208\n"
			" HBM Example usage: ./amdmemtool --i 6 --ref 13\n"
			" GDDR5 Example usage: ./amdmemtool --i 1,2,4 --RFC 43 --ras2ras 176\n\n"
			" Make sure to run the program first with parameter --current to see what the current values are.\n"
			" Current values may change based on state of the GPU,\n"
			" in other words, make sure the GPU is under load when running --current\n"
			" HBM2 Based GPU's do not need to be under load to apply timing changes.\n"
			" Hint: Certain timings such as CL (Cas Latency) are stability timings, lowering these will lower stability.\n";
		return 0;
	}

	if (!strcasecmp("--version", argv[1]) || !strcasecmp("--v", argv[1]))
	{
		printf(VERSION);
		return EXIT_SUCCESS;
	}

	struct pci_access* pci = pci_alloc();
	pci_init(pci);
	pci_scan_bus(pci);
	int gpuCount = 0;
	for (struct pci_dev* dev = pci->devices; dev; dev = dev->next)
	{
		pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_ROM_BASE | PCI_FILL_SIZES | PCI_FILL_CLASS);
		if (IsAmdDisplayDevice(dev))
		{
			gpus[gpuCount++].dev = dev;
		}
	}

	if (gpuCount == 0)
	{
		printf("No AMD display devices have been found!\n");
		return 0;
	}

	qsort(gpus, gpuCount, sizeof(GPU), gpu_compare);

	//printf("Detected GPU's\n");
	for (int i = 0; i < gpuCount; i++)
	{
		GPU* gpu = &gpus[i];

		char buffer[1024];
		//char *name = pci_lookup_name(pci, buffer, sizeof(buffer), PCI_LOOKUP_DEVICE, gpu->dev->vendor_id, gpu->dev->device_id);
		//printf(" (%s)\n", name);

		snprintf(buffer, sizeof(buffer) - 1, "%04x:%02x:%02x.%d", gpu->dev->domain, gpu->dev->bus, gpu->dev->dev, gpu->dev->func);
		int instance = pci_find_instance(buffer);
		if (instance == -1)
		{
			fprintf(stderr, "Cannot find DRI instance for pci:%s\n", buffer);
			return 1;
		}

		snprintf(buffer, sizeof(buffer) - 1, "/sys/kernel/debug/dri/%d/amdgpu_regs", instance);
		gpu->mmio = open(buffer, O_RDWR);
		if (gpu->mmio == -1)
		{
			fprintf(stderr, "Failed to open %s\n", buffer);
			return 1;
		}

		switch (DetermineMemoryType(gpu->dev))
		{
		case HBM2:
			lseek(gpu->mmio, AMD_TIMING_REGS_BASE_1, SEEK_SET);
			read(gpu->mmio, &gpu->hbm2, sizeof(gpu->hbm2));
			lseek(gpu->mmio, MANUFACTURER_ID_HBM2, SEEK_SET);
			read(gpu->mmio, &gpu->man, sizeof(gpu->man));
			break;
		case GDDR5:
			/*lseek(gpu->mmio, MC_SEQ_RD_CTL_D0, SEEK_SET);
			read(gpu->mmio, &gpu->d0, sizeof(gpu->d0));*/
			lseek(gpu->mmio, MC_SEQ_PMG_TIMING, SEEK_SET);
			read(gpu->mmio, &gpu->pmg, sizeof(gpu->pmg));
			lseek(gpu->mmio, MC_SEQ_RAS_TIMING, SEEK_SET);
			read(gpu->mmio, &gpu->ras, sizeof(gpu->ras));
			lseek(gpu->mmio, MC_SEQ_CAS_TIMING, SEEK_SET);
			read(gpu->mmio, &gpu->cas, sizeof(gpu->cas));
			lseek(gpu->mmio, MC_SEQ_MISC_TIMING, SEEK_SET);
			read(gpu->mmio, &gpu->misc, sizeof(gpu->misc));
			lseek(gpu->mmio, MC_SEQ_MISC_TIMING2, SEEK_SET);
			read(gpu->mmio, &gpu->misc2, sizeof(gpu->misc2));
			/*lseek(gpu->mmio, MC_SEQ_MISC3, SEEK_SET);
			read(gpu->mmio, &gpu->misc3, sizeof(gpu->misc3));*/
			lseek(gpu->mmio, MC_ARB_DRAM_TIMING, SEEK_SET);
			read(gpu->mmio, &gpu->dram1, sizeof(gpu->dram1));
			lseek(gpu->mmio, MC_ARB_DRAM_TIMING2, SEEK_SET);
			read(gpu->mmio, &gpu->dram2, sizeof(gpu->dram2));
			lseek(gpu->mmio, MC_ARB_RFSH_RATE, SEEK_SET);
			read(gpu->mmio, &gpu->ref, sizeof(gpu->ref));
			lseek(gpu->mmio, MC_SEQ_TRAIN_TIMING, SEEK_SET);
			read(gpu->mmio, &gpu->train, sizeof(gpu->train));
			lseek(gpu->mmio, MANUFACTURER_ID, SEEK_SET);
			read(gpu->mmio, &gpu->man, sizeof(gpu->man));
			break;
		case HBM:
			lseek(gpu->mmio, MC_SEQ_PMG_TIMING_HBM, SEEK_SET);
			read(gpu->mmio, &gpu->pmg, sizeof(gpu->pmg));
			lseek(gpu->mmio, MC_SEQ_RAS_TIMING_HBM, SEEK_SET);
			read(gpu->mmio, &gpu->ras, sizeof(gpu->ras));
			lseek(gpu->mmio, MC_SEQ_CAS_TIMING_HBM, SEEK_SET);
			read(gpu->mmio, &gpu->cas, sizeof(gpu->cas));
			lseek(gpu->mmio, MC_SEQ_MISC_TIMING_HBM, SEEK_SET);
			read(gpu->mmio, &gpu->misc, sizeof(gpu->misc));
			lseek(gpu->mmio, MC_SEQ_MISC_TIMING2_HBM, SEEK_SET);
			read(gpu->mmio, &gpu->misc2, sizeof(gpu->misc2));
			lseek(gpu->mmio, MC_ARB_RFSH_RATE, SEEK_SET);
			read(gpu->mmio, &gpu->ref, sizeof(gpu->ref));
			lseek(gpu->mmio, MC_SEQ_ROW_HAMMER, SEEK_SET);
			read(gpu->mmio, &gpu->ham, sizeof(gpu->ham));
			lseek(gpu->mmio, MANUFACTURER_ID_HBM, SEEK_SET);
			read(gpu->mmio, &gpu->man, sizeof(gpu->man));
			break;
		}
	}

	// Parses the command line arguments, and accumulates the changes.
	for (int index = 0; index < gpuCount; index++)
	{
		GPU* gpu = &gpus[index];
		char buffer[1024];
		char* name = pci_lookup_name(pci, buffer, sizeof(buffer), PCI_LOOKUP_DEVICE, gpu->dev->vendor_id, gpu->dev->device_id);
		if (gpu->dev && IsRelevantDeviceID(gpu->dev))
		{
			u64 affectedGPUs = 0xFFFFFFFFFFFFFFFF; // apply to all GPUs by default
			for (int i = 1; i < argc; i++)
			{
				if (!strcasecmp("--gpu", argv[i]) || !strcasecmp("--i", argv[i]))
				{
					if (i == (argc - 1))
					{
						std::cout << "Argument \"" << argv[i] << "\" requires a parameter.\n";
						return 1;
					}
					i++;
					affectedGPUs = ParseIndicesArg(argv[i]);
				}
				else if (!strcasecmp("--current", argv[i]))
				{
					if (affectedGPUs & ((u64)1 << index))
					{
						std::cout << "GPU " << index << ": ";
						printf(" %s\t", name);
						PrintCurrentValues(gpu);
					}
				}
				else if (affectedGPUs & ((u64)1 << index))
				{
					u32 value = 0;
					if (DetermineMemoryType(gpu->dev) == HBM2)
					{
						if (ParseNumericArg(argc, argv, i, "--CL", value))
						{
							gpu->hbm2.CL = value;
							gpu->modify[0] = true;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CL");
						}
						else if (ParseNumericArg(argc, argv, i, "--RAS", value))
						{
							gpu->hbm2.RAS = value;
							gpu->modify[0] = true;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RAS");
						}
						else if (ParseNumericArg(argc, argv, i, "--RCDRD", value))
						{
							gpu->hbm2.RCDRD = value;
							gpu->modify[0] = true;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RCDRD");
						}
						else if (ParseNumericArg(argc, argv, i, "--RCDWR", value))
						{
							gpu->hbm2.RCDWR = value;
							gpu->modify[0] = true;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RCDWR");
						}
						else if (ParseNumericArg(argc, argv, i, "--RC", value))
						{
							gpu->hbm2.RCAb = value;
							gpu->hbm2.RCPb = value;
							gpu->modify[0] = true;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RC");
						}
						else if (ParseNumericArg(argc, argv, i, "--RP", value))
						{
							gpu->hbm2.RPAb = value;
							gpu->hbm2.RPPb = value;
							gpu->modify[0] = true;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RP");
						}
						else if (ParseNumericArg(argc, argv, i, "--RRDS", value))
						{
							gpu->hbm2.RRDS = value;
							gpu->modify[0] = true;
							gpu->modify[3] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RRD");
						}
						else if (ParseNumericArg(argc, argv, i, "--RRDL", value))
						{
							gpu->hbm2.RRDL = value;
							gpu->modify[0] = true;
							gpu->modify[3] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RRD");
						}
						else if (ParseNumericArg(argc, argv, i, "--RTP", value))
						{
							gpu->hbm2.RTP = value;
							gpu->modify[0] = true;
							gpu->modify[3] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RTP");
						}
						else if (ParseNumericArg(argc, argv, i, "--FAW", value))
						{
							gpu->hbm2.FAW = value;
							gpu->modify[0] = true;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "FAW");
						}
						else if (ParseNumericArg(argc, argv, i, "--CWL", value))
						{
							gpu->hbm2.CWL = value;
							gpu->modify[0] = true;
							gpu->modify[5] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CWL");
						}
						else if (ParseNumericArg(argc, argv, i, "--WTRS", value))
						{
							gpu->hbm2.WTRS = value;
							gpu->modify[0] = true;
							gpu->modify[5] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WTRS");
						}
						else if (ParseNumericArg(argc, argv, i, "--WTRL", value))
						{
							gpu->hbm2.WTRL = value;
							gpu->modify[0] = true;
							gpu->modify[5] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WTRL");
						}
						else if (ParseNumericArg(argc, argv, i, "--WR", value))
						{
							gpu->hbm2.WR = value;
							gpu->modify[0] = true;
							gpu->modify[6] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WR");
						}
						else if (ParseNumericArg(argc, argv, i, "--RREFD", value))
						{
							gpu->hbm2.RREFD = value;
							gpu->modify[0] = true;
							gpu->modify[7] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RREFD");
						}
						else if (ParseNumericArg(argc, argv, i, "--RDRDDD", value))
						{
							gpu->hbm2.RDRDDD = value;
							gpu->modify[0] = true;
							gpu->modify[8] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RDRDDD");
						}
						else if (ParseNumericArg(argc, argv, i, "--RDRDSD", value))
						{
							gpu->hbm2.RDRDSD = value;
							gpu->modify[0] = true;
							gpu->modify[8] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RDRDSD");
						}
						else if (ParseNumericArg(argc, argv, i, "--RDRDSC", value))
						{
							gpu->hbm2.RDRDSC = value;
							gpu->modify[0] = true;
							gpu->modify[8] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RDRDSC");
						}
						else if (ParseNumericArg(argc, argv, i, "--RDRDSCL", value))
						{
							gpu->hbm2.RDRDSCL = value;
							gpu->modify[0] = true;
							gpu->modify[8] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RDRDSCL");
						}
						else if (ParseNumericArg(argc, argv, i, "--WRWRDD", value))
						{
							gpu->hbm2.WRWRDD = value;
							gpu->modify[0] = true;
							gpu->modify[9] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WRWRDD");
						}
						else if (ParseNumericArg(argc, argv, i, "--WRWRSD", value))
						{
							gpu->hbm2.WRWRSD = value;
							gpu->modify[0] = true;
							gpu->modify[9] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WRWRSD");
						}
						else if (ParseNumericArg(argc, argv, i, "--WRWRSC", value))
						{
							gpu->hbm2.WRWRSC = value;
							gpu->modify[0] = true;
							gpu->modify[9] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WRWRSC");
						}
						else if (ParseNumericArg(argc, argv, i, "--WRWRSCL", value))
						{
							gpu->hbm2.WRWRSCL = value;
							gpu->modify[0] = true;
							gpu->modify[9] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WRWRSCL");
						}
						else if (ParseNumericArg(argc, argv, i, "--WRRD", value))
						{
							gpu->hbm2.WRRD = value;
							gpu->modify[0] = true;
							gpu->modify[10] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WRRD");
						}
						else if (ParseNumericArg(argc, argv, i, "--RDWR", value))
						{
							gpu->hbm2.RDWR = value;
							gpu->modify[0] = true;
							gpu->modify[10] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RDWR");
						}
						else if (ParseNumericArg(argc, argv, i, "--REF", value))
						{
							gpu->hbm2.REF = value;
							gpu->modify[0] = true;
							gpu->modify[12] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "REF");
						}
						else if (ParseNumericArg(argc, argv, i, "--MRD", value))
						{
							gpu->hbm2.MRD = value;
							gpu->modify[0] = true;
							gpu->modify[13] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "MRD");
						}
						else if (ParseNumericArg(argc, argv, i, "--MOD", value))
						{
							gpu->hbm2.MOD = value;
							gpu->modify[0] = true;
							gpu->modify[13] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "MOD");
						}
						else if (ParseNumericArg(argc, argv, i, "--XS", value))
						{
							gpu->hbm2.XS = value;
							gpu->modify[0] = true;
							gpu->modify[14] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "XS");
						}
						else if (ParseNumericArg(argc, argv, i, "--XSMRS", value))
						{
							gpu->hbm2.XSMRS = value;
							gpu->modify[0] = true;
							gpu->modify[16] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "XSMRS");
						}
						else if (ParseNumericArg(argc, argv, i, "--PD", value))
						{
							gpu->hbm2.PD = value;
							gpu->modify[0] = true;
							gpu->modify[17] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "PD");
						}
						else if (ParseNumericArg(argc, argv, i, "--CKSRE", value))
						{
							gpu->hbm2.CKSRE = value;
							gpu->modify[0] = true;
							gpu->modify[17] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CKSRE");
						}
						else if (ParseNumericArg(argc, argv, i, "--CKSRX", value))
						{
							gpu->hbm2.CKSRX = value;
							gpu->modify[0] = true;
							gpu->modify[17] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CKSRX");
						}
						else if (ParseNumericArg(argc, argv, i, "--RFCPB", value))
						{
							gpu->hbm2.RFCPB = value;
							gpu->modify[0] = true;
							gpu->modify[20] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RFCPB");
						}
						else if (ParseNumericArg(argc, argv, i, "--STAG", value))
						{
							gpu->hbm2.STAG = value;
							gpu->modify[0] = true;
							gpu->modify[20] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "STAG");
						}
						else if (ParseNumericArg(argc, argv, i, "--XP", value))
						{
							gpu->hbm2.XP = value;
							gpu->modify[0] = true;
							gpu->modify[21] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "XP");
						}
						else if (ParseNumericArg(argc, argv, i, "--CPDED", value))
						{
							gpu->hbm2.CPDED = value;
							gpu->modify[0] = true;
							gpu->modify[21] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CPDED");
						}
						else if (ParseNumericArg(argc, argv, i, "--CKE", value))
						{
							gpu->hbm2.CKE = value;
							gpu->modify[0] = true;
							gpu->modify[21] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CKE");
						}
						else if (ParseNumericArg(argc, argv, i, "--RDDATA", value))
						{
							gpu->hbm2.RDDATA = value;
							gpu->modify[0] = true;
							gpu->modify[22] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RDDATA");
						}
						else if (ParseNumericArg(argc, argv, i, "--WRLAT", value))
						{
							gpu->hbm2.WRLAT = value;
							gpu->modify[0] = true;
							gpu->modify[22] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WRLAT");
						}
						else if (ParseNumericArg(argc, argv, i, "--RDLAT", value))
						{
							gpu->hbm2.RDLAT = value;
							gpu->modify[0] = true;
							gpu->modify[22] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RDLAT");
						}
						else if (ParseNumericArg(argc, argv, i, "--WRDATA", value))
						{
							gpu->hbm2.WRDATA = value;
							gpu->modify[0] = true;
							gpu->modify[22] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WRDATA");
						}
						else if (ParseNumericArg(argc, argv, i, "--CKESTAG", value))
						{
							gpu->hbm2.CKESTAG = value;
							gpu->modify[0] = true;
							gpu->modify[23] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CKESTAG");
						}
						else if (ParseNumericArg(argc, argv, i, "--RFC", value))
						{
							gpu->hbm2.RFC = value;
							gpu->modify[0] = true;
							gpu->modify[24] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RFC");
						}
					}
					else if (DetermineMemoryType(gpu->dev) == HBM) {
						if (ParseNumericArg(argc, argv, i, "--CKSRE", value))
						{
							gpu->pmg.hbm.CKSRE = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CKSRE");
						}
						else if (ParseNumericArg(argc, argv, i, "--CKSRX", value))
						{
							gpu->pmg.hbm.CKSRX = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CKSRX");
						}
						else if (ParseNumericArg(argc, argv, i, "--CKE_PULSE", value))
						{
							gpu->pmg.hbm.CKE_PULSE = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CKE_PULSE");
						}
						else if (ParseNumericArg(argc, argv, i, "--CKE", value))
						{
							gpu->pmg.hbm.CKE = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CKE");
						}
						else if (ParseNumericArg(argc, argv, i, "--SEQ_IDLE", value))
						{
							gpu->pmg.hbm.SEQ_IDLE = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "SEQ_IDLE");
						}
						else if (ParseNumericArg(argc, argv, i, "--SEQ_IDLE_SS", value))
						{
							gpu->pmg.hbm.SEQ_IDLE = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "SEQ_IDLE_SS");
						}
						else if (ParseNumericArg(argc, argv, i, "--RCDW", value))
						{
							gpu->ras.RCDW = value;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RCDW");
						}
						else if (ParseNumericArg(argc, argv, i, "--RCDWA", value))
						{
							gpu->ras.RCDWA = value;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RCDWA");
						}
						else if (ParseNumericArg(argc, argv, i, "--RCDR", value))
						{
							gpu->ras.RCDR = value;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RCDR");
						}
						else if (ParseNumericArg(argc, argv, i, "--RCDRA", value))
						{
							gpu->ras.RCDRA = value;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RCDRA");
						}
						else if (ParseNumericArg(argc, argv, i, "--RRD", value))
						{
							gpu->ras.RRD = value;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RRD");
						}
						else if (ParseNumericArg(argc, argv, i, "--RC", value))
						{
							gpu->ras.RC = value;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RC");
						}
						else if (ParseNumericArg(argc, argv, i, "--CL", value))
						{
							gpu->cas.hbm.CL = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CL");
						}
						else if (ParseNumericArg(argc, argv, i, "--W2R", value))
						{
							gpu->cas.hbm.W2R = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "W2R");
						}
						else if (ParseNumericArg(argc, argv, i, "--R2R", value))
						{
							gpu->cas.hbm.R2R = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "R2R");
						}
						else if (ParseNumericArg(argc, argv, i, "--CCDL", value))
						{
							gpu->cas.hbm.CCDL = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CCDL");
						}
						else if (ParseNumericArg(argc, argv, i, "--R2W", value))
						{
							gpu->cas.hbm.R2W = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "R2W");
						}
						else if (ParseNumericArg(argc, argv, i, "--NOPR", value))
						{
							gpu->cas.hbm.NOPR = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "NOPR");
						}
						else if (ParseNumericArg(argc, argv, i, "--NOPW", value))
						{
							gpu->cas.hbm.NOPW = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "NOPW");
						}
						else if (ParseNumericArg(argc, argv, i, "--MRD", value))
						{
							gpu->misc.hbm.MRD = value;
							gpu->modify[3] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "MRD");
						}
						else if (ParseNumericArg(argc, argv, i, "--RRDL", value))
						{
							gpu->misc.hbm.RRDL = value;
							gpu->modify[3] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RRDL");
						}
						else if (ParseNumericArg(argc, argv, i, "--RFC", value))
						{
							gpu->misc.hbm.RFC = value;
							gpu->modify[3] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RFC");
						}
						else if (ParseNumericArg(argc, argv, i, "--TRP", value))
						{
							gpu->misc.hbm.TRP = value;
							gpu->modify[3] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "TRP");
						}
						else if (ParseNumericArg(argc, argv, i, "--RP_RDA", value))
						{
							gpu->misc.hbm.RP_RDA = value;
							gpu->modify[3] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RP_RDA");
						}
						else if (ParseNumericArg(argc, argv, i, "--RP_WRA", value))
						{
							gpu->misc.hbm.RP_WRA = value;
							gpu->modify[3] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RP_WRA");
						}
						else if (ParseNumericArg(argc, argv, i, "--WDATATR", value))
						{
							gpu->misc2.hbm.WDATATR = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WDATATR");
						}
						else if (ParseNumericArg(argc, argv, i, "--T32AW", value))
						{
							gpu->misc2.hbm.T32AW = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "T32AW");
						}
						else if (ParseNumericArg(argc, argv, i, "--RPAR", value))
						{
							gpu->misc2.hbm.RPAR = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RPAR");
						}
						else if (ParseNumericArg(argc, argv, i, "--WPAR", value))
						{
							gpu->misc2.hbm.WPAR = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WPAR");
						}
						else if (ParseNumericArg(argc, argv, i, "--FAW", value))
						{
							gpu->misc2.hbm.FAW = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "FAW");
						}
						else if (ParseNumericArg(argc, argv, i, "--PA2WDATA", value))
						{
							gpu->misc2.hbm.PA2WDATA = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "P2WDATA");
						}
						else if (ParseNumericArg(argc, argv, i, "--PA2RDATA", value))
						{
							gpu->misc2.hbm.PA2RDATA = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "PA2RDATA");
						}
						else if (ParseNumericArg(argc, argv, i, "--REF", value))
						{
							gpu->ref.REF = value;
							gpu->modify[5] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "REF");
						}
						else if (ParseNumericArg(argc, argv, i, "--ENB", value))
						{
							gpu->ham.ENB = value;
							gpu->modify[6] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "ENB");
						}
						else if (ParseNumericArg(argc, argv, i, "--CNT", value))
						{
							gpu->ham.CNT = value;
							gpu->modify[6] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CNT");
						}
						else if (ParseNumericArg(argc, argv, i, "--TRC", value))
						{
							gpu->ham.TRC = value;
							gpu->modify[6] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "TRC");
						}
					}
					else // GDDR5
					{
						if (ParseNumericArg(argc, argv, i, "--CKSRE", value))
						{
							gpu->pmg.rx.CKSRE = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CKSRE");
						}
						else if (ParseNumericArg(argc, argv, i, "--CKSRX", value))
						{
							gpu->pmg.rx.CKSRX = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CKSRX");
						}
						else if (ParseNumericArg(argc, argv, i, "--CKE_PULSE", value))
						{
							gpu->pmg.rx.CKE_PULSE = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CKE_PULSE");
						}
						else if (ParseNumericArg(argc, argv, i, "--CKE", value))
						{
							gpu->pmg.rx.CKE = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CKE");
						}
						else if (ParseNumericArg(argc, argv, i, "--SEQ_IDLE", value))
						{
							gpu->pmg.rx.SEQ_IDLE = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "SEQ_IDLE");
						}
						else if (ParseNumericArg(argc, argv, i, "--CKE_PULSE_MSB", value))
						{
							gpu->pmg.rx.CKE_PULSE_MSB = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CKE_PULSE_MSB");
						}
						else if (ParseNumericArg(argc, argv, i, "--SEQ_IDLE_SS", value))
						{
							gpu->pmg.rx.SEQ_IDLE_SS = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "SEQ_IDLE_SS");
						}
						else if (ParseNumericArg(argc, argv, i, "--RCDW", value))
						{
							gpu->ras.RCDW = value;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RCDW");
						}
						else if (ParseNumericArg(argc, argv, i, "--RCDWA", value))
						{
							gpu->ras.RCDWA = value;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RCDWA");
						}
						else if (ParseNumericArg(argc, argv, i, "--RCDR", value))
						{
							gpu->ras.RCDR = value;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RCDR");
						}
						else if (ParseNumericArg(argc, argv, i, "--RCDRA", value))
						{
							gpu->ras.RCDRA = value;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RCDRA");
						}
						else if (ParseNumericArg(argc, argv, i, "--RRD", value))
						{
							gpu->ras.RRD = value;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RRD");
						}
						else if (ParseNumericArg(argc, argv, i, "--RC", value))
						{
							gpu->ras.RC = value;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RC");
						}
						else if (ParseNumericArg(argc, argv, i, "--CL", value))
						{
							gpu->cas.rx.CL = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CL");
						}
						else if (ParseNumericArg(argc, argv, i, "--W2R", value))
						{
							gpu->cas.rx.W2R = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "W2R");
						}
						else if (ParseNumericArg(argc, argv, i, "--R2R", value))
						{
							gpu->cas.rx.R2R = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "R2R");
						}
						else if (ParseNumericArg(argc, argv, i, "--CCDL", value))
						{
							gpu->cas.rx.CCDL = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CCDL");
						}
						else if (ParseNumericArg(argc, argv, i, "--R2W", value))
						{
							gpu->cas.rx.R2W = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "R2W");
						}
						else if (ParseNumericArg(argc, argv, i, "--NOPR", value))
						{
							gpu->cas.rx.NOPR = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "NOPR");
						}
						else if (ParseNumericArg(argc, argv, i, "--NOPW", value))
						{
							gpu->cas.rx.NOPW = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "NOPW");
						}
						else if (ParseNumericArg(argc, argv, i, "--RFC", value))
						{
							(IsR9(gpu->dev) ? gpu->misc.r9.RFC : gpu->misc.rx.RFC) = value;
							gpu->modify[3] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RFC");
						}
						else if (ParseNumericArg(argc, argv, i, "--TRP", value))
						{
							(IsR9(gpu->dev) ? gpu->misc.r9.TRP : gpu->misc.rx.TRP) = value;
							gpu->modify[3] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "TRP");
						}
						else if (ParseNumericArg(argc, argv, i, "--RP_RDA", value))
						{
							(IsR9(gpu->dev) ? gpu->misc.r9.RP_RDA : gpu->misc.rx.RP_RDA) = value;
							gpu->modify[3] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RP_RDA");
						}
						else if (ParseNumericArg(argc, argv, i, "--RP_WRA", value))
						{
							(IsR9(gpu->dev) ? gpu->misc.r9.RP_WRA : gpu->misc.rx.RP_WRA) = value;
							gpu->modify[3] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RP_WRA");
						}
						else if (ParseNumericArg(argc, argv, i, "--WDATATR", value))
						{
							gpu->misc2.rx.WDATATR = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WDATATR");
						}
						else if (ParseNumericArg(argc, argv, i, "--T32AW", value))
						{
							gpu->misc2.rx.T32AW = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "T32AW");
						}
						else if (ParseNumericArg(argc, argv, i, "--REDC", value))
						{
							gpu->misc2.rx.REDC = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "REDC");
						}
						else if (ParseNumericArg(argc, argv, i, "--WEDC", value))
						{
							gpu->misc2.rx.WEDC = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WEDC");
						}
						else if (ParseNumericArg(argc, argv, i, "--FAW", value))
						{
							gpu->misc2.rx.FAW = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "FAW");
						}
						else if (ParseNumericArg(argc, argv, i, "--PA2WDATA", value))
						{
							gpu->misc2.rx.PA2WDATA = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "P2WDATA");
						}
						else if (ParseNumericArg(argc, argv, i, "--PA2RDATA", value))
						{
							gpu->misc2.rx.PA2RDATA = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "PA2RDATA");
						}
						/*else if (ParseNumericArg(argc, argv, i, "--RAS", value))
						{
								gpu->misc3.RAS = value;
								gpu->modify[5] = true;
								if (gpu->log[0]) strcat(gpu->log, ", ");
								strcat(gpu->log, "PA2RDATA");
						}*/
						else if (ParseNumericArg(argc, argv, i, "--ACTRD", value))
						{
							gpu->dram1.ACTRD = value;
							gpu->modify[5] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "ACTRD");
						}
						else if (ParseNumericArg(argc, argv, i, "--ACTWR", value))
						{
							gpu->dram1.ACTWR = value;
							gpu->modify[5] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "ACTWR");
						}
						else if (ParseNumericArg(argc, argv, i, "--RASMACTRD", value))
						{
							gpu->dram1.RASMACTRD = value;
							gpu->modify[5] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RASMACTRD");
						}
						else if (ParseNumericArg(argc, argv, i, "--RASMACTWR", value))
						{
							gpu->dram1.RASMACTWR = value;
							gpu->modify[5] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RASMACTWR");
						}
						else if (ParseNumericArg(argc, argv, i, "--RAS2RAS", value))
						{
							gpu->dram2.RAS2RAS = value;
							gpu->modify[6] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RAS2RAS");
						}
						else if (ParseNumericArg(argc, argv, i, "--RP", value))
						{
							gpu->dram2.RP = value;
							gpu->modify[6] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RP");
						}
						else if (ParseNumericArg(argc, argv, i, "--WRPLUSRP", value))
						{
							gpu->dram2.WRPLUSRP = value;
							gpu->modify[6] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WRPLUSRP");
						}
						else if (ParseNumericArg(argc, argv, i, "--BUS_TURN", value))
						{
							gpu->dram2.BUS_TURN = value;
							gpu->modify[6] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "BUS_TURN");
						}
						else if (ParseNumericArg(argc, argv, i, "--REF", value))
						{
							gpu->ref.REF = value;
							gpu->modify[7] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "REF");
						}

					}
				}
			}
		}
	}

	// Actually applies the changes, if any.
	for (int index = 0; index < gpuCount; index++)
	{
		GPU* gpu = &gpus[index];
		for (int i = 0; i < _countof(gpu->modify); i++)
		{
			if (gpu->modify[i])
			{
				switch (DetermineMemoryType(gpu->dev))
				{
				case HBM2:
				{
					u32 value = ((u32*)& gpu->hbm2)[i];
					if (i == 0) // special logic for frequency
					{
						value = (gpu->hbm2.frequency == 0x118) ? 0x118 : 0x11C;
					}

					lseek(gpu->mmio, AMD_TIMING_REGS_BASE_1 + (i * sizeof(u32)), SEEK_SET);
					write(gpu->mmio, &value, sizeof(u32));

					lseek(gpu->mmio, AMD_TIMING_REGS_BASE_2 + (i * sizeof(u32)), SEEK_SET);
					write(gpu->mmio, &value, sizeof(u32));

					lseek(gpu->mmio, AMD_TIMING_REGS_BASE_3 + (i * sizeof(u32)), SEEK_SET);
					write(gpu->mmio, &value, sizeof(u32));

					lseek(gpu->mmio, AMD_TIMING_REGS_BASE_4 + (i * sizeof(u32)), SEEK_SET);
					write(gpu->mmio, &value, sizeof(u32));
					break;
				}
				case GDDR5:
					switch (i)
					{
					case 0:
						lseek(gpu->mmio, MC_SEQ_PMG_TIMING, SEEK_SET);
						write(gpu->mmio, &gpu->pmg, sizeof(gpu->pmg.rx));
						break;
					case 1:
						lseek(gpu->mmio, MC_SEQ_RAS_TIMING, SEEK_SET);
						write(gpu->mmio, &gpu->ras, sizeof(gpu->ras));
						break;
					case 2:
						lseek(gpu->mmio, MC_SEQ_CAS_TIMING, SEEK_SET);
						write(gpu->mmio, &gpu->cas, sizeof(gpu->cas.rx));
						break;
					case 3:
						lseek(gpu->mmio, MC_SEQ_MISC_TIMING, SEEK_SET);
						write(gpu->mmio, &gpu->misc, sizeof(gpu->misc.rx));
						break;
					case 4:
						lseek(gpu->mmio, MC_SEQ_MISC_TIMING2, SEEK_SET);
						write(gpu->mmio, &gpu->misc2, sizeof(gpu->misc2.rx));
						break;
						/*case 5:
								lseek(gpu->mmio, MC_SEQ_MISC3, SEEK_SET);
								write(gpu->mmio, &gpu->misc3, sizeof(gpu->misc3));
								break;*/
					case 5:
						lseek(gpu->mmio, MC_ARB_DRAM_TIMING, SEEK_SET);
						write(gpu->mmio, &gpu->dram1, sizeof(gpu->dram1));
						break;
					case 6:
						lseek(gpu->mmio, MC_ARB_DRAM_TIMING2, SEEK_SET);
						write(gpu->mmio, &gpu->dram2, sizeof(gpu->dram2));
						break;
					case 7:
						lseek(gpu->mmio, MC_ARB_RFSH_RATE, SEEK_SET);
						write(gpu->mmio, &gpu->ref, sizeof(gpu->ref));
						break;
						/*case 9:
								lseek(gpu->mmio, MC_SEQ_RD_CTL_D0, SEEK_SET);
								write(gpu->mmio, &gpu->d0, sizeof(gpu->d0));
								break;*/
					}
					break;
				case HBM:
					switch (i)
					{
					case 0:
						lseek(gpu->mmio, MC_SEQ_PMG_TIMING_HBM, SEEK_SET);
						write(gpu->mmio, &gpu->pmg, sizeof(gpu->pmg.hbm));
						break;
					case 1:
						lseek(gpu->mmio, MC_SEQ_RAS_TIMING_HBM, SEEK_SET);
						write(gpu->mmio, &gpu->ras, sizeof(gpu->ras));
						break;
					case 2:
						lseek(gpu->mmio, MC_SEQ_CAS_TIMING_HBM, SEEK_SET);
						write(gpu->mmio, &gpu->cas, sizeof(gpu->cas.hbm));
						break;
					case 3:
						lseek(gpu->mmio, MC_SEQ_MISC_TIMING_HBM, SEEK_SET);
						write(gpu->mmio, &gpu->misc, sizeof(gpu->misc.hbm));
						break;
					case 4:
						lseek(gpu->mmio, MC_SEQ_MISC_TIMING2_HBM, SEEK_SET);
						write(gpu->mmio, &gpu->misc2, sizeof(gpu->misc2.hbm));
						break;
					case 5:
						lseek(gpu->mmio, MC_ARB_RFSH_RATE, SEEK_SET);
						write(gpu->mmio, &gpu->ref, sizeof(gpu->ref));
						break;
					case 6:
						lseek(gpu->mmio, MC_SEQ_ROW_HAMMER, SEEK_SET);
						write(gpu->mmio, &gpu->ham, sizeof(gpu->ham));
						break;
					}
					break;
				}
			}
		}
	}

	for (int index = 0; index < gpuCount; index++)
	{
		GPU* gpu = &gpus[index];
		if (gpu->log[0])
		{
			printf("Successfully applied new %s settings to GPU %d.\n", gpu->log, index);
		}
	}

	pci_cleanup(pci);
	return 0;
}
