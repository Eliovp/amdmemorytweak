/*
	AMD Memory Tweak by
	Elio VP			<elio@eliovp.be>
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

bool IsAmdDisplayDevice(struct pci_dev *dev)
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

bool IsRelevantDeviceID(struct pci_dev *dev)
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
		// (dev->device_id == 0x7300) || // Fiji [Radeon R9 FURY / NANO Series]
		(dev->device_id == 0x67b0) || // Hawaii XT / Grenada XT [Radeon R9 290X/390X]
		(dev->device_id == 0x67b1) || // Hawaii PRO [Radeon R9 290/390]
		(dev->device_id == 0x6798) || // Tahiti XT [Radeon HD 7970/8970 OEM / R9 280X]
		(dev->device_id == 0x679a); // Tahiti PRO [Radeon HD 7950/8950 OEM / R9 280]
}

static bool IsR9(struct pci_dev *dev)
{
	return
		(dev->device_id == 0x67b0) || // Hawaii XT / Grenada XT [Radeon R9 290X/390X]
		(dev->device_id == 0x67b1) || // Hawaii PRO [Radeon R9 290/390]
		(dev->device_id == 0x6798) || // Tahiti XT [Radeon HD 7970/8970 OEM / R9 280X]
		(dev->device_id == 0x679a); // Tahiti PRO [Radeon HD 7950/8950 OEM / R9 280]
}

typedef enum { GDDR5, HBM, HBM2 } MemoryType;

static MemoryType DetermineMemoryType(struct pci_dev *dev)
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
		{ 0x1002, 0x687f, HBM2 }, // "Radeon RX Vega", CHIP_VEGA10
		{ 0x1002, 0x687f, HBM2 }, // "Radeon RX Vega 64", CHIP_VEGA10
		{ 0x1002, 0x687f, HBM2 }, // "Radeon RX Vega 64", CHIP_VEGA10
		{ 0x1002, 0x687f, HBM2 }, // "Radeon RX Vega 56", CHIP_VEGA10
		{ 0x1002, 0x6863, HBM2 }, // "Radeon Vega Frontier Edition", CHIP_VEGA10
		/* Fury/Nano  Support will Follow later */
		/*{ 0x1002, 0x7300, HBM }, // "Radeon R9 Fury/Nano/X", CHIP_FIJI
		{ 0x1002, 0x7300, HBM }, // "Radeon R9 Fury/Nano/X", CHIP_FIJI
		{ 0x1002, 0x7300, HBM }, // "Radeon R9 Fury/Nano/X", CHIP_FIJI
		{ 0x1002, 0x7300, HBM }, // "Radeon R9 Fury/Nano/X", CHIP_FIJI
		{ 0x1002, 0x7300, HBM }, // "Radeon R9 Fury", CHIP_FIJI */
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

// Split up AmdMemoryTweak to AmdMemoryTweakHBM & AmdMemoryTweakGDDR5 because the timings are different
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
	u32 : 32;
	// TIMING8
	u32 : 32;
	// TIMING9
	u32 : 32;
	// TIMING10
	u32 WRRD : 8;
	u32 RDWR : 8;
	u32 : 16;
	// PADDING
	u32 : 32;
	// TIMING12
	u32 REF : 16;
	u32 : 16;
	// TIMING13
	u32 MRD : 8;
	u32 MOD : 8;
	u32 : 16;
	// PADDING
	u32 : 32;
	// PADDING
	u32 : 32;
	// PADDING
	u32 : 32;
	// TIMING17
	u32 PD : 4;
	u32 CKSRE : 6;
	u32 CKSRX : 6;
	u32 : 16;
} HBM2_TIMINGS;

typedef union {
	u32 value;
	struct {
		u32 RFC : 16;
		u32 : 16;
	};
} RFC_TIMING;
#define RFC_TIMING_ADDR_1 0x50260
#define RFC_TIMING_ADDR_2 0x52260
#define RFC_TIMING_ADDR_3 0x54260
#define RFC_TIMING_ADDR_4 0x56260

typedef union {
	u32 value;
	struct {
		u32 RCDW : 5;
		u32 RCDWA : 5;
		u32 RCDR : 5;
		u32 RCDRA : 5;
		u32 RRD : 4;
		u32 RC : 7;
		u32 : 1;
	};
} SEQ_RAS_TIMING;
#define MC_SEQ_RAS_TIMING 0x28A0

typedef union {
	u32 value;
	struct {
		u32 NOPW : 2;
		u32 NOPR : 2;
		u32 R2W : 5;
		u32 CCLD : 3;
		u32 CCDS : 4;
		u32 W2R : 5;
		u32 : 3;
		u32 CL : 5;
		u32 : 3;
	};
} SEQ_CAS_TIMING;
#define MC_SEQ_CAS_TIMING 0x28A4

typedef union {
	u32 value;
	struct {
		u32 RP_WRA : 7;
		u32 RP_RDA : 7;
		u32 TRP : 6;
		u32 RFC : 9;
		u32 : 3;
	} rx;
	struct {
		u32 RP_WRA : 8;
		u32 RP_RDA : 7;
		u32 TRP : 5;
		u32 RFC : 9;
		u32 : 3;
	} r9;
} SEQ_MISC_TIMING;
#define MC_SEQ_MISC_TIMING 0x28A8

typedef union {
	u32 value;
	struct {
		u32 PA2RDATA : 3;
		u32 : 1;
		u32 PA2WDATA : 3;
		u32 : 1;
		u32 FAW : 5;
		u32 CRCRL : 3;
		u32 CRCWL : 5;
		u32 T32AW : 4;
		u32 : 3;
		u32 WDATATR : 4;
	};
} SEQ_MISC_TIMING2;
#define MC_SEQ_MISC_TIMING2 0x28AC

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

static u64 ParseIndicesArg(const char *arg)
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

static bool ParseNumericArg(int argc, const char *argv[], int &i, const char* arg, u32 &value)
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

static int pci_find_instance(char *pci_string)
{
	DIR *dir = opendir("/sys/kernel/debug/dri");
	if (!dir)
	{
		perror("Couldn't open DRI under debugfs\n");
		return -1;
	}

	struct dirent *entry;
	while (entry = readdir(dir))
	{
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;

		char name[300];
		snprintf(name, sizeof(name)-1, "/sys/kernel/debug/dri/%s/name", entry->d_name);
		FILE *f = fopen(name, "r");
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
	struct pci_dev *dev;
	int mmio;

	char log[1000];
	bool modify[19];
	// HBM2
	HBM2_TIMINGS hbm2;
	RFC_TIMING rfc;
	// GDDR5
	SEQ_RAS_TIMING ras;
	SEQ_CAS_TIMING cas;
	SEQ_MISC_TIMING misc;
	SEQ_MISC_TIMING2 misc2;
	ARB_DRAM_TIMING dram1;
	ARB_DRAM_TIMING2 dram2;
} GPU;

static int gpu_compare(const void *A, const void *B)
{
	const struct pci_dev *a = ((const GPU*)A)->dev;
	const struct pci_dev *b = ((const GPU*)B)->dev;
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

static void PrintCurrentValues(GPU *gpu)
{
	fprintf(stdout, "pci:%04x:%02x:%02x.%d\n", gpu->dev->domain, gpu->dev->bus, gpu->dev->dev, gpu->dev->func);
	fflush(stdout);
	if (DetermineMemoryType(gpu->dev) == HBM2)
	{
		HBM2_TIMINGS current = gpu->hbm2;
		std::cout << "Memory state: 0x" << std::hex << current.frequency << std::dec;
		std::cout <<
			((current.frequency == 0x118) ? " (800MHz)" :
			(current.frequency == 0x11C) ? " (1000MHz)" :
			(current.frequency == 0x11E) ? " (1200MHz)" : " (unknown)") << std::endl;
		std::cout << "Timing 1\n";
		std::cout << "  CL: " << current.CL << "\t";
		std::cout << "  RAS: " << current.RAS << "\t";
		std::cout << "  RCDRD: " << current.RCDRD << "\t";
		std::cout << "  RCDWR: " << current.RCDWR << "\n";
		std::cout << "Timing 2\n";
		std::cout << "  RCAb (RC): " << current.RCAb << "\t";
		std::cout << "  RCPb (RC): " << current.RCPb << "\t";
		std::cout << "  RPAb (RP): " << current.RPAb << "\t";
		std::cout << "  RPPb (RP): " << current.RPPb << "\n";
		std::cout << "Timing 3\n";
		std::cout << "  RRDS (RRD): " << current.RRDS << "\t";
		std::cout << "  RRDL (RRD): " << current.RRDL << "\t";
		std::cout << "  RTP: " << current.RTP << "\n";
		std::cout << "Timing 4\n";
		std::cout << "  FAW: " << current.FAW << "\n";
		std::cout << "Timing 5\n";
		std::cout << "  CWL: " << current.CWL << "\t";
		std::cout << "  WTRS: " << current.WTRS << "\t";
		std::cout << "  WTRL: " << current.WTRL << "\n";
		std::cout << "Timing 6\n";
		std::cout << "  WR: " << current.WR << "\n";
		std::cout << "Timing 10\n";
                std::cout << "  WRRD: " << current.WRRD << "\t";
		std::cout << "  RDWR: " << current.RDWR << "\n";
		std::cout << "Timing 12\n";
                std::cout << "  REF: " << current.REF << "\n";
		std::cout << "Timing 13\n";
                std::cout << "  MRD: " << current.MRD << "\t";
		std::cout << "  MOD: " << current.MOD << "\n";
		std::cout << "Timing 17\n";
                std::cout << "  PD: " << current.PD << "\t";
                std::cout << "  CKSRE: " << current.CKSRE << "\t";
		std::cout << "  CKSRX: " << current.CKSRX << "\n";
		std::cout << "RFC Timing\n";
		std::cout << "  RFC: " << gpu->rfc.RFC << "\n";
		std::cout << "\n";
	}
	else // GDDR5
	{
		printf("CAS\n");
		printf("  CL: %d\t", gpu->cas.CL);
		printf("  W2R: %d\t", gpu->cas.W2R);
		printf("  CCDS: %d\t", gpu->cas.CCDS);
		printf("  CCLD: %d\t", gpu->cas.CCLD);
		printf("  R2W: %d\t", gpu->cas.R2W);
		printf("  NOPR: %d\t", gpu->cas.NOPR);
		printf("  NOPW: %d\n", gpu->cas.NOPW);
		printf("RAS\n");
		printf("  RC: %d\t", gpu->ras.RC);
		printf("  RRD: %d\t", gpu->ras.RRD);
		printf("  RCDRA: %d\t", gpu->ras.RCDRA);
		printf("  RCDR: %d\t", gpu->ras.RCDR);
		printf("  RCDWA: %d\t", gpu->ras.RCDWA);
		printf("  RCDW: %d\n", gpu->ras.RCDW);
		printf("MISC\n");
		if (IsR9(gpu->dev))
		{
			printf("  RFC: %d\t", gpu->misc.r9.RFC);
			printf("  TRP: %d\t", gpu->misc.r9.TRP);
			printf("  RP_RDA: %d\t", gpu->misc.r9.RP_RDA);
			printf("  RP_WRA: %d\n", gpu->misc.r9.RP_WRA);
		}
		else
		{
			printf("  RFC: %d\t", gpu->misc.rx.RFC);
			printf("  TRP: %d\t", gpu->misc.rx.TRP);
			printf("  RP_RDA: %d\t", gpu->misc.rx.RP_RDA);
			printf("  RP_WRA: %d\n", gpu->misc.rx.RP_WRA);
		}
		printf("MISC2\n");
		printf("  WDATATR: %d\t", gpu->misc2.WDATATR);
		printf("  T32AW: %d\t", gpu->misc2.T32AW);
		printf("  CRCWL: %d\t", gpu->misc2.CRCWL);
		printf("  CRCRL: %d\t", gpu->misc2.CRCRL);
		printf("  FAW: %d\t", gpu->misc2.FAW);
		printf("  PA2WDATA: %d\t", gpu->misc2.PA2WDATA);
		printf("  PA2RDATA: %d\n", gpu->misc2.PA2RDATA);
		printf("DRAM1\n");
		printf("  RASMACTWR: %d\t", gpu->dram1.RASMACTWR);
		printf("  RASMACTRD: %d\t", gpu->dram1.RASMACTRD);
		printf("  ACTWR: %d\t", gpu->dram1.ACTWR);
		printf("  ACTRD: %d\n", gpu->dram1.ACTRD);
		printf("DRAM2\n");
		printf("  RAS2RAS: %d\t", gpu->dram2.RAS2RAS);
		printf("  RP: %d\t", gpu->dram2.RP);
		printf("  WRPLUSRP: %d\t", gpu->dram2.WRPLUSRP);
		printf("  BUS_TURN: %d\n", gpu->dram2.BUS_TURN);
	}
}

int main(int argc, const char *argv[])
{
	GPU gpus[64] = {};

	if ((argc < 2) || (0 == strcasecmp("--help", argv[1])))
	{
		std::cout << " AMD Memory Tweak\n"
			" Read and modify memory timings on the fly\n"
			" By Eliovp & A.Solodovnikov\n\n"
			" Global command line options:\n"
			" --help \tShow this output\n"
			" --gpu|--i [comma-separated gpu indices]\tSelected device(s)\n"
			" --current \tList current timing values\n\n"
			" Command line options: (HBM2)\n"
			" --CL|--cl [value]\n"
			" --RAS|--ras [value]\n"
			" --RCDRD|--rcdrd [value]\n"
			" --RCDWR|--rcdwr [value]\n"
			" --RC|--rc [value]\n"
			" --RP|--rp [value]\n"
			" --RRD|--rrd [value]\n"
			" --RTP|--rtp [value]\n"
			" --FAW|--faw [value]\n"
			" --CWL|--cwl [value]\n"
			" --WTRS|--wtrs [value]\n"
			" --WTRL|--wtrl [value]\n"
			" --WR|--wr [value]\n"
			" --WRRD|--wrrd [value]\n"
			" --RDWR|--rdwr [value]\n"
			" --REF|--ref [value]\n"
			" --MRD|--mrd [value]\n"
			" --MOD|--mod [value]\n"
			" --PD|--pd [value]\n"
			" --CKSRE|--cksre [value]\n"
			" --CKSRX|--cksrx [value]\n"
			" --RFC|--rfc [value]\n\n"
			" Command line options: (GDDR5)\n"
			" --CL|--cl [value]\n"
			" --W2R|--w2r [value]\n"
                        " --CCDS|--ccds [value]\n"
                        " --CCLD|--ccld [value]\n"
                        " --R2W|--r2w [value]\n"
			" --NOPR|--nopr [value]\n"
                        " --NOPW|--nopw [value]\n"
                        " --RCDW|--rcdw [value]\n"
                        " --RCDWA|--rcdwa [value]\n"
                        " --RCDR|--rcdr [value]\n"
                        " --RCDRA|--rcdra [value]\n"
                        " --RRD|--rrd [value]\n"
                        " --RC|--rc [value]\n"
                        " --RFC|--rfc [value]\n"
                        " --TRP|--trp [value]\n"
			" --RP_WRA|--rp_wra [value]\n"
                        " --RP_RDA|--rp_rda [value]\n"
			" --WDATATR|--wdatatr [value]\n"
			" --T32AW|--t32aw [value]\n"
			" --CRCWL|--crcwl [value]\n"
			" --CRCRL|--crcrl [value]\n"
			" --FAW|--faw [value]\n"
			" --PA2WDATA|--pa2wdata [value]\n"
			" --PA2RDATA|--pa2rdata [value]\n"
                        " --ACTRD|--actrd [value]\n"
                        " --ACTWR|--actwr [value]\n"
                        " --RASMACTRD|--rasmactrd [value]\n"
                        " --RASMACWTR|--rasmacwtr [value]\n"
                        " --RAS2RAS|--ras2ras [value]\n"
                        " --RP|--rp [value]\n"
                        " --WRPLUSRP|--wrplusrp [value]\n"
                        " --BUS_TURN|--bus_turn [value]\n\n"
			" HBM2 Example usage: ./amdmemtool -i 0,3,5 --faw 12 --RFC 208\n"
			" GDDR5 Example usage: ./amdmemtool -i 1,2,4 --RFC 43 --ras2ras 176\n\n"
			" Make sure to run the program first with parameter --current to see what the current values are.\n"
			" Current values may change based on state of the GPU,\n"
			" in other words, make sure the GPU is under load when running --current\n"
			" HBM2 Based GPU's do not need to be under load to apply timing changes.\n"
			" Hint: Certain timings such as CL (Cas Latency) are stability timings, lowering these will lower stability.\n";
		return 0;
	}

	struct pci_access *pci = pci_alloc();
	pci_init(pci);
	pci_scan_bus(pci);
	int gpuCount = 0;
	for (struct pci_dev *dev = pci->devices; dev; dev = dev->next)
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
		GPU *gpu = &gpus[i];

		char buffer[1024];
		//char *name = pci_lookup_name(pci, buffer, sizeof(buffer), PCI_LOOKUP_DEVICE, gpu->dev->vendor_id, gpu->dev->device_id);
		//printf(" (%s)\n", name);

		snprintf(buffer, sizeof(buffer)-1, "%04x:%02x:%02x.%d", gpu->dev->domain, gpu->dev->bus, gpu->dev->dev, gpu->dev->func);
		int instance = pci_find_instance(buffer);
		if (instance == -1)
		{
			fprintf(stderr, "Cannot find DRI instance for pci:%s\n", buffer);
			return 1;
		}

		snprintf(buffer, sizeof(buffer)-1, "/sys/kernel/debug/dri/%d/amdgpu_regs", instance);
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
			//lseek(gpu->mmio, REF_TIMING_ADDR_1, SEEK_SET);
                        //read(gpu->mmio, &gpu->ref, sizeof(gpu->ref));
			lseek(gpu->mmio, RFC_TIMING_ADDR_1, SEEK_SET);
			read(gpu->mmio, &gpu->rfc, sizeof(gpu->rfc));
			break;
		case GDDR5:
			lseek(gpu->mmio, MC_SEQ_RAS_TIMING, SEEK_SET);
			read(gpu->mmio, &gpu->ras, sizeof(gpu->ras));
			lseek(gpu->mmio, MC_SEQ_CAS_TIMING, SEEK_SET);
			read(gpu->mmio, &gpu->cas, sizeof(gpu->cas));
			lseek(gpu->mmio, MC_SEQ_MISC_TIMING, SEEK_SET);
			read(gpu->mmio, &gpu->misc, sizeof(gpu->misc));
			lseek(gpu->mmio, MC_SEQ_MISC_TIMING2, SEEK_SET);
			read(gpu->mmio, &gpu->misc2, sizeof(gpu->misc2));
			lseek(gpu->mmio, MC_ARB_DRAM_TIMING, SEEK_SET);
			read(gpu->mmio, &gpu->dram1, sizeof(gpu->dram1));
			lseek(gpu->mmio, MC_ARB_DRAM_TIMING, SEEK_SET);
			read(gpu->mmio, &gpu->dram2, sizeof(gpu->dram2));
			break;
		/* case HBM:
			// maybe supported?
			lseek(gpu->mmio, 0x28, SEEK_SET);
			read(gpu->mmio, &gpu->ras, sizeof(gpu->ras));
			lseek(gpu->mmio, 0x28, SEEK_SET);
			read(gpu->mmio, &gpu->cas, sizeof(gpu->cas));
			lseek(gpu->mmio, 0x28, SEEK_SET);
			read(gpu->mmio, &gpu->misc, sizeof(gpu->misc));
			lseek(gpu->mmio, 0x28, SEEK_SET);
			read(gpu->mmio, &gpu->misc2, sizeof(gpu->misc2));
			lseek(gpu->mmio, MC_ARB_DRAM_TIMING, SEEK_SET);
			read(gpu->mmio, &gpu->dram1, sizeof(gpu->dram1));
			lseek(gpu->mmio, MC_ARB_DRAM_TIMING, SEEK_SET);
			read(gpu->mmio, &gpu->dram2, sizeof(gpu->dram2));
			break; */
		}
	}

	// Parses the command line arguments, and accumulates the changes.
	for (int index = 0; index < gpuCount; index++)
	{
		GPU *gpu = &gpus[index];
		char buffer[1024];
                char *name = pci_lookup_name(pci, buffer, sizeof(buffer), PCI_LOOKUP_DEVICE, gpu->dev->vendor_id, gpu->dev->device_id);
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
						else if (ParseNumericArg(argc, argv, i, "--RRD", value))
						{
							gpu->hbm2.RRDS = value;
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
						else if (ParseNumericArg(argc, argv, i, "--RFC", value))
						{
							gpu->rfc.RFC = value;
							gpu->modify[0] = true;
							gpu->modify[18] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RFC");
						}
					}
					else // GDDR5 & HBM
					{
						if (ParseNumericArg(argc, argv, i, "--RCDW", value))
						{
							gpu->ras.RCDW = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RCDW");
						}
						else if (ParseNumericArg(argc, argv, i, "--RCDWA", value))
						{
							gpu->ras.RCDWA = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RCDWA");
						}
						else if (ParseNumericArg(argc, argv, i, "--RCDR", value))
						{
							gpu->ras.RCDR = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RCDR");
						}
						else if (ParseNumericArg(argc, argv, i, "--RCDRA", value))
						{
							gpu->ras.RCDRA = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RCDRA");
						}
						else if (ParseNumericArg(argc, argv, i, "--RRD", value))
						{
							gpu->ras.RRD = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RRD");
						}
						else if (ParseNumericArg(argc, argv, i, "--RC", value))
						{
							gpu->ras.RC = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RC");
						}
						else if (ParseNumericArg(argc, argv, i, "--CL", value))
						{
							gpu->cas.CL = value;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CL");
						}
						else if (ParseNumericArg(argc, argv, i, "--W2R", value))
						{
							gpu->cas.W2R = value;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "W2R");
						}
						else if (ParseNumericArg(argc, argv, i, "--CCDS", value))
						{
							gpu->cas.CCDS = value;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CCDS");
						}
						else if (ParseNumericArg(argc, argv, i, "--CCLD", value))
						{
							gpu->cas.CCLD = value;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CCLD");
						}
						else if (ParseNumericArg(argc, argv, i, "--R2W", value))
						{
							gpu->cas.R2W = value;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "R2W");
						}
						else if (ParseNumericArg(argc, argv, i, "--NOPR", value))
                                                {
                                                        gpu->cas.NOPR = value;
                                                        gpu->modify[1] = true;
                                                        if (gpu->log[0]) strcat(gpu->log, ", ");
                                                        strcat(gpu->log, "NOPR");
                                                }
						else if (ParseNumericArg(argc, argv, i, "--NOPW", value))
                                                {
                                                        gpu->cas.NOPW = value;
                                                        gpu->modify[1] = true;
                                                        if (gpu->log[0]) strcat(gpu->log, ", ");
                                                        strcat(gpu->log, "NOPW");
                                                }
						else if (ParseNumericArg(argc, argv, i, "--RFC", value))
						{
							(IsR9(gpu->dev) ? gpu->misc.r9.RFC : gpu->misc.rx.RFC) = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RFC");
						}
						else if (ParseNumericArg(argc, argv, i, "--TRP", value))
						{
							(IsR9(gpu->dev) ? gpu->misc.r9.TRP : gpu->misc.rx.TRP) = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "TRP");
						}
						else if (ParseNumericArg(argc, argv, i, "--RP_RDA", value))
						{
							(IsR9(gpu->dev) ? gpu->misc.r9.RP_RDA : gpu->misc.rx.RP_RDA) = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RP_RDA");
						}
						else if (ParseNumericArg(argc, argv, i, "--RP_WRA", value))
						{
							(IsR9(gpu->dev) ? gpu->misc.r9.RP_WRA : gpu->misc.rx.RP_WRA) = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RP_WRA");
						}
						else if (ParseNumericArg(argc, argv, i, "--WDATATR", value))
						{
							gpu->misc2.WDATATR = value;
							gpu->modify[3] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WDATATR");
						}
						else if (ParseNumericArg(argc, argv, i, "--T32AW", value))
                                                {
                                                        gpu->misc2.T32AW = value;
                                                        gpu->modify[3] = true;
                                                        if (gpu->log[0]) strcat(gpu->log, ", ");
                                                        strcat(gpu->log, "T32AW");
                                                }
						else if (ParseNumericArg(argc, argv, i, "--CRCWL", value))
                                                {
                                                        gpu->misc2.CRCWL = value;
                                                        gpu->modify[3] = true;
                                                        if (gpu->log[0]) strcat(gpu->log, ", ");
                                                        strcat(gpu->log, "CRCWL");
                                                }
						else if (ParseNumericArg(argc, argv, i, "--CRCRL", value))
                                                {
                                                        gpu->misc2.CRCRL = value;
                                                        gpu->modify[3] = true;
                                                        if (gpu->log[0]) strcat(gpu->log, ", ");
                                                        strcat(gpu->log, "CRCRL");
                                                }
						else if (ParseNumericArg(argc, argv, i, "--FAW", value))
                                                {
                                                        gpu->misc2.FAW = value;
                                                        gpu->modify[3] = true;
                                                        if (gpu->log[0]) strcat(gpu->log, ", ");
                                                        strcat(gpu->log, "FAW");
                                                }
						else if (ParseNumericArg(argc, argv, i, "--PA2WDATA", value))
                                                {
                                                        gpu->misc2.PA2WDATA = value;
                                                        gpu->modify[3] = true;
                                                        if (gpu->log[0]) strcat(gpu->log, ", ");
                                                        strcat(gpu->log, "P2WDATA");
                                                }
						else if (ParseNumericArg(argc, argv, i, "--PA2RDATA", value))
                                                {
                                                        gpu->misc2.PA2RDATA = value;
                                                        gpu->modify[3] = true;
                                                        if (gpu->log[0]) strcat(gpu->log, ", ");
                                                        strcat(gpu->log, "PA2RDATA");
                                                }
						else if (ParseNumericArg(argc, argv, i, "--ACTRD", value))
						{
							gpu->dram1.ACTRD = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "ACTRD");
						}
						else if (ParseNumericArg(argc, argv, i, "--ACTWR", value))
						{
							gpu->dram1.ACTWR = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "ACTWR");
						}
						else if (ParseNumericArg(argc, argv, i, "--RASMACTRD", value))
						{
							gpu->dram1.RASMACTRD = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RASMACTRD");
						}
						else if (ParseNumericArg(argc, argv, i, "--RASMACTWR", value))
						{
							gpu->dram1.RASMACTWR = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RASMACTWR");
						}
						else if (ParseNumericArg(argc, argv, i, "--RAS2RAS", value))
						{
							gpu->dram2.RAS2RAS = value;
							gpu->modify[5] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RAS2RAS");
						}
						else if (ParseNumericArg(argc, argv, i, "--RP", value))
						{
							gpu->dram2.RP = value;
							gpu->modify[5] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RP");
						}
						else if (ParseNumericArg(argc, argv, i, "--WRPLUSRP", value))
						{
							gpu->dram2.WRPLUSRP = value;
							gpu->modify[5] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WRPLUSRP");
						}
						else if (ParseNumericArg(argc, argv, i, "--BUS_TURN", value))
						{
							gpu->dram2.BUS_TURN = value;
							gpu->modify[5] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "BUS_TURN");
						}
					}
				}
			}
		}
	}

	// Actually applies the changes, if any.
	for (int index = 0; index < gpuCount; index++)
	{
		GPU *gpu = &gpus[index];
		for (int i = 0; i < _countof(gpu->modify); i++)
		{
			if (gpu->modify[i])
			{
				switch (DetermineMemoryType(gpu->dev))
				{
				case HBM2:
					/*if (i == 7)
                                        {
                                                lseek(gpu->mmio, REF_TIMING_ADDR_1, SEEK_SET);
                                                write(gpu->mmio, &gpu->ref, sizeof(gpu->ref));
                                                lseek(gpu->mmio, REF_TIMING_ADDR_2, SEEK_SET);
                                                write(gpu->mmio, &gpu->ref, sizeof(gpu->ref));
                                                lseek(gpu->mmio, REF_TIMING_ADDR_3, SEEK_SET);
                                                write(gpu->mmio, &gpu->ref, sizeof(gpu->ref));
                                                lseek(gpu->mmio, REF_TIMING_ADDR_4, SEEK_SET);
                                                write(gpu->mmio, &gpu->ref, sizeof(gpu->ref));
                                        }*/
					if (i == 18)
					{
						lseek(gpu->mmio, RFC_TIMING_ADDR_1, SEEK_SET);
						write(gpu->mmio, &gpu->rfc, sizeof(gpu->rfc));

						lseek(gpu->mmio, RFC_TIMING_ADDR_2, SEEK_SET);
                                                write(gpu->mmio, &gpu->rfc, sizeof(gpu->rfc));

						lseek(gpu->mmio, RFC_TIMING_ADDR_3, SEEK_SET);
                                                write(gpu->mmio, &gpu->rfc, sizeof(gpu->rfc));

						lseek(gpu->mmio, RFC_TIMING_ADDR_4, SEEK_SET);
                                                write(gpu->mmio, &gpu->rfc, sizeof(gpu->rfc));
					}
					else
					{
						u32 value = ((u32*)&gpu->hbm2)[i];
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
					}
					break;
				case GDDR5:
					switch (i)
					{
					case 0:
						lseek(gpu->mmio, MC_SEQ_RAS_TIMING, SEEK_SET);
						write(gpu->mmio, &gpu->ras, sizeof(gpu->ras));
						break;
					case 1:
						lseek(gpu->mmio, MC_SEQ_CAS_TIMING, SEEK_SET);
						write(gpu->mmio, &gpu->cas, sizeof(gpu->cas));
						break;
					case 2:
						lseek(gpu->mmio, MC_SEQ_MISC_TIMING, SEEK_SET);
						write(gpu->mmio, &gpu->misc, sizeof(gpu->misc));
						break;
					case 3:
						lseek(gpu->mmio, MC_SEQ_MISC_TIMING2, SEEK_SET);
						write(gpu->mmio, &gpu->misc2, sizeof(gpu->misc2));
						break;
					case 4:
						lseek(gpu->mmio, MC_ARB_DRAM_TIMING, SEEK_SET);
						write(gpu->mmio, &gpu->dram1, sizeof(gpu->dram1));
						break;
					case 5:
						lseek(gpu->mmio, MC_ARB_DRAM_TIMING, SEEK_SET);
						write(gpu->mmio, &gpu->dram2, sizeof(gpu->dram2));
						break;
					}
					break;
				case HBM:
					perror("HBM memory type is not supported yet\n");
					return 1;
				}
			}
		}
	}

	for (int index = 0; index < gpuCount; index++)
	{
		GPU *gpu = &gpus[index];
		if (gpu->log[0])
		{
			printf("Successfully applied new %s settings to GPU %d.\n", gpu->log, index);
		}
	}

	pci_cleanup(pci);
	return 0;
}
