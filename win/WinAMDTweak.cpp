#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "IOMap.h"

typedef WORD u16;
typedef DWORD u32;
typedef unsigned __int64 u64;
#define strcasecmp _strcmpi

#define AMD_TIMING_REGS_BASE_1 0x50200
#define AMD_TIMING_REGS_BASE_2 0x52200
#define AMD_TIMING_REGS_BASE_3 0x54200
#define AMD_TIMING_REGS_BASE_4 0x56200

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
	u32 REF : 16;		// Determines at what rate refreshes will be executed.
	u32 : 16;
	// TIMING13
	u32 MRD : 8;
	u32 MOD : 8;
	u32 : 16;
	// TIMING14
	u32 XS : 16;		// self refresh exit period
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

typedef union {
	u32 value;
	struct {
		u32 CKSRE : 4;
		u32 CKSRX : 4;
		u32 CKE_PULSE : 4;
		u32 CKE : 6;
		u32 SEQ_IDLE : 3;
		u32 : 11;
	};
} SEQ_PMG_TIMING;
#define MC_SEQ_PMG_TIMING 0x28B0

typedef union {
	u32 value;
	struct {
		u32 RCDW : 5;	// # of cycles from active to write
		u32 RCDWA : 5;	// # of cycles from active to write with auto-precharge
		u32 RCDR : 5;	// # of cycles from active to read
		u32 RCDRA : 5;	// # of cycles from active to read with auto-precharge
		u32 RRD : 4;	// # of cycles from active bank a to active bank b
		u32 RC : 7;	// # of cycles from active to active/auto refresh
		u32 : 1;
	};
} SEQ_RAS_TIMING;
#define MC_SEQ_RAS_TIMING 0x28A0

typedef union {
	u32 value;
	struct {
		u32 NOPW : 2;	// Extra cycle(s) between successive write bursts
		u32 NOPR : 2;	// Extra cycle(s) between successive read bursts
		u32 R2W : 5;	// Read to write turn
		u32 CCLD : 3;	// Cycles between r/w from bank A to r/w bank B
		u32 R2R : 4;	// Read to read time
		u32 W2R : 5;	// Write to read turn
		u32 : 3;
			  u32 CL : 5;	// CAS to data return latency
			  u32 : 3;
	};
} SEQ_CAS_TIMING;
#define MC_SEQ_CAS_TIMING 0x28A4

typedef union {
	u32 value;
	struct {
		u32 RP_WRA : 7;	// From write with auto-precharge to active
		u32 RP_RDA : 7;	// From read with auto-precharge to active
		u32 TRP : 6;	// Precharge command period
		u32 RFC : 9;	// Auto-refresh command period
		u32 : 3;
	} rx;
	struct {
		u32 RP_WRA : 8;	// From write with auto-precharge to active
		u32 RP_RDA : 7;	// From read with auto-precharge to active
		u32 TRP : 5;	// Precharge command period
		u32 RFC : 9;	// Auto-refresh command period
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
		u32 FAW : 5;	// The time window in wich four activates are allowed in the same rank
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
		u32 : 22;
		u32 RAS : 6;
		u32 : 4;
	};
} SEQ_MISC3;
#define MC_SEQ_MISC3 0x2a2c

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


static const char* LookupDeviceName(u16 device_id)
{
	struct {
		u16 vendor_id;
		u16 device_id;
		const char* name;
	} KnownGPUs[] = {
		{ 0x1002, 0x66af, "Radeon VII" },
		{ 0x1002, 0x687f, "Vega 10 XL/XT [Radeon RX Vega 56/64]" },
		{ 0x1002, 0x6867, "Vega 10 XL [Radeon Pro Vega 56]" },
		{ 0x1002, 0x6863, "Vega 10 XTX [Radeon Vega Frontier Edition]" },
		{ 0x1002, 0x67df, "Ellesmere [Radeon RX 470/480/570/570X/580/580X/590]" },
		{ 0x1002, 0x67c4, "Ellesmere [Radeon Pro WX 7100]" },
		{ 0x1002, 0x67c7, "Ellesmere [Radeon Pro WX 5100]" },
		{ 0x1002, 0x67ef, "Baffin [Radeon RX 460/560D / Pro 450/455/460/555/555X/560/560X]" },
		{ 0x1002, 0x67ff, "Baffin [Radeon RX 550 640SP / RX 560/560X]" },
		{ 0x1002, 0x7300, "Fiji [Radeon R9 FURY / NANO Series]" },
		{ 0x1002, 0x67b0, "Hawaii XT / Grenada XT [Radeon R9 290X/390X]" },
		{ 0x1002, 0x67b1, "Hawaii PRO [Radeon R9 290/390]" },
		{ 0x1002, 0x6798, "Tahiti XT [Radeon HD 7970/8970 OEM / R9 280X]" },
		{ 0x1002, 0x679a, "Tahiti PRO [Radeon HD 7950/8950 OEM / R9 280]" },
	};
	for (int i = 0; i < _countof(KnownGPUs); i++)
	{
		if (/*(KnownGPUs[i].vendor_id == dev->vendor_id) && */(KnownGPUs[i].device_id == device_id))
		{
			return KnownGPUs[i].name;
		}
	}
	return "";
}

static bool IsRelevantDeviceID(u16 device_id)
{
	return
		(device_id == 0x66af) || // Radeon VII
		(device_id == 0x687f) || // Vega 10 XL/XT [Radeon RX Vega 56/64]
		(device_id == 0x6867) || // Vega 10 XL [Radeon Pro Vega 56]
		(device_id == 0x6863) || // Vega 10 XTX [Radeon Vega Frontier Edition]
		(device_id == 0x67df) || // Ellesmere [Radeon RX 470/480/570/570X/580/580X/590]
		(device_id == 0x67c4) || // Ellesmere [Radeon Pro WX 7100]
		(device_id == 0x67c7) || // Ellesmere [Radeon Pro WX 5100]
		(device_id == 0x67ef) || // Baffin [Radeon RX 460/560D / Pro 450/455/460/555/555X/560/560X]
		(device_id == 0x67ff) || // Baffin [Radeon RX 550 640SP / RX 560/560X]
		//(device_id == 0x7300) || // Fiji [Radeon R9 FURY / NANO Series]
		(device_id == 0x67b0) || // Hawaii XT / Grenada XT [Radeon R9 290X/390X]
		(device_id == 0x67b1) || // Hawaii PRO [Radeon R9 290/390]
		(device_id == 0x6798) || // Tahiti XT [Radeon HD 7970/8970 OEM / R9 280X]
		(device_id == 0x679a); // Tahiti PRO [Radeon HD 7950/8950 OEM / R9 280]
}

static bool IsR9(u16 device_id)
{
	return
		(device_id == 0x67b0) || // Hawaii XT / Grenada XT [Radeon R9 290X/390X]
		(device_id == 0x67b1) || // Hawaii PRO [Radeon R9 290/390]
		(device_id == 0x6798) || // Tahiti XT [Radeon HD 7970/8970 OEM / R9 280X]
		(device_id == 0x679a); // Tahiti PRO [Radeon HD 7950/8950 OEM / R9 280]
}

typedef enum { GDDR5, HBM, HBM2 } MemoryType;

static MemoryType DetermineMemoryType(u16 device_id)
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
		if (/*(KnownGPUs[i].vendor_id == dev->vendor_id) && */(KnownGPUs[i].device_id == device_id))
		{
			return KnownGPUs[i].memory_type;
		}
	}
	return GDDR5;
}

typedef struct {
	char log[1000];
	bool modify[25];
	// HBM2
	HBM2_TIMINGS hbm2;
	// GDDR5
	SEQ_PMG_TIMING pmg;
	SEQ_RAS_TIMING ras;
	SEQ_CAS_TIMING cas;
	SEQ_MISC_TIMING misc;
	SEQ_MISC_TIMING2 misc2;
	SEQ_MISC3 misc3;
	ARB_DRAM_TIMING dram1;
	ARB_DRAM_TIMING2 dram2;
} GPU;

static u64 ParseIndicesArg(const char* arg)
{
	char buffer[1000];
	strcpy(buffer, arg);
	u64 mask = 0;
	char* token = strtok(buffer, ",");
	while (token)
	{
		while (*token && isspace(*token)) token++;
		char* endptr;
		unsigned long index = strtoul(token, &endptr, 10);
		while (*endptr && isspace(*endptr)) endptr++;
		int err = errno;
		if ((endptr == token) || *endptr || (errno == EINVAL) || (errno == ERANGE) || (index >= 64))
		{
			printf("Invalid GPU index specified.\n");
			exit(EXIT_FAILURE);
		}
		mask |= ((u64)1 << index);
		token = strtok(NULL, ",");
	}
	return mask;
}

static bool ParseNumericArg(int argc, const char* argv[], int& i, const char* arg, u32 & value)
{
	if (!strcasecmp(arg, argv[i]))
	{
		if (i == (argc - 1))
		{
			printf("Argument \"%s\" requires a parameter.\n", argv[i]);
			exit(EXIT_FAILURE);
		}
		i++;
		value = strtoul(argv[i], 0, 10);
		if ((errno == EINVAL) || (errno == ERANGE))
		{
			printf("Failed to parse parameter %s %s\n", argv[i - 1], argv[i]);
			exit(EXIT_FAILURE);
		}
		return true;
	}
	return false;
}

static void PrintCurrentValues(VGAChip * chip, GPU * gpu)
{
	if (DetermineMemoryType(chip->device_id) == HBM2)
	{
		HBM2_TIMINGS current = gpu->hbm2;
		printf("Memory state: 0x%X (%s)\n", current.frequency,
			(current.frequency == 0x118) ? "800MHz" :
			(current.frequency == 0x11C) ? "1000MHz" :
			(current.frequency == 0x11E) ? "1200MHz" : "unknown");
		printf("Timing 1\t");
		printf("  CL: %d\t", current.CL);
		printf("  RAS: %d\t", current.RAS);
		printf("  RCDRD: %d\t", current.RCDRD);
		printf("  RCDWR: %d\n", current.RCDWR);
		printf("Timing 2\t");
		printf("  RCAb (RC): %d\t", current.RCAb);
		printf("  RCPb (RC): %d\t", current.RCPb);
		printf("  RPAb (RP): %d\t", current.RPAb);
		printf("  RPPb (RP): %d\n", current.RPPb);
		printf("Timing 3\t");
		printf("  RRDS: %d\t", current.RRDS);
		printf("  RRDL: %d\t", current.RRDL);
		printf("  RTP: %d\n", current.RTP);
		printf("Timing 4\t");
		printf("  FAW: %d\n", current.FAW);
		printf("Timing 5\t");
		printf("  CWL: %d\t", current.CWL);
		printf("  WTRS: %d\t", current.WTRS);
		printf("  WTRL: %d\n", current.WTRL);
		printf("Timing 6\t");
		printf("  WR: %d\n", current.WR);
		printf("Timing 7\t");
		printf("  RREFD: %d\n", current.RREFD);
		printf("Timing 8\t");
		printf("  RDRDDD: %d\t", current.RDRDDD);
		printf("  RDRDSD: %d\t", current.RDRDSD);
		printf("  RDRDSC: %d\t", current.RDRDSC);
		printf("  RDRDSCL: %d\n", current.RDRDSCL);
		printf("Timing 9\t");
		printf("  WRWRDD: %d\t", current.WRWRDD);
		printf("  WRWRSD: %d\t", current.WRWRSD);
		printf("  WRWRSC: %d\t", current.WRWRSC);
		printf("  WRWRSCL: %d\n", current.WRWRSCL);
		printf("Timing 10\t");
		printf("  WRRD: %d\t", current.WRRD);
		printf("  RDWR: %d\n", current.RDWR);
		printf("Timing 12\t");
		printf("  REF: %d\n", current.REF);
		printf("Timing 13\t");
		printf("  MRD: %d\t", current.MRD);
		printf("  MOD: %d\n", current.MOD);
		printf("Timing 14\t");
		printf("  XS: %d\n", current.XS);
		printf("Timing 16\t");
		printf("  XSMRS: %d\n", current.XSMRS);
		printf("Timing 17\t");
		printf("  PD: %d\t", current.PD);
		printf("  CKSRE: %d\t", current.CKSRE);
		printf("  CKSRX: %d\n", current.CKSRX);
		printf("Timing 20\t");
		printf("  RFCPB: %d\t", current.RFCPB);
		printf("  STAG: %d\n", current.STAG);
		printf("Timing 21\t");
		printf("  XP: %d\t", current.XP);
		printf("  CPDED: %d\t", current.CPDED);
		printf("  CKE: %d\n", current.CKE);
		printf("Timing 22\t");
		printf("  RDDATA: %d\t", current.RDDATA);
		printf("  WRLAT: %d\t", current.WRLAT);
		printf("  RDLAT: %d\t", current.RDLAT);
		printf("  WRDATA: %d\n", current.WRDATA);
		printf("Timing 23\t");
		printf("  CKESTAG: %d\n", current.CKESTAG);
		printf("RFC Timing\t");
		printf("  RFC: %d\n", current.RFC);
		printf("\n");
	}
	else // GDDR5
	{
		printf("\nPMG:\t");
		printf("  CKSRE: %d\t", gpu->pmg.CKSRE);
		printf("  CKSRX: %d\t", gpu->pmg.CKSRX);
		printf("  CKE_PULSE: %d\t", gpu->pmg.CKE_PULSE);
		printf("  CKE: %d\t", gpu->pmg.CKE);
		printf("  SEQ_IDLE: %d\n", gpu->pmg.SEQ_IDLE);
		printf("CAS:\t");
		printf("  CL: %d\t", gpu->cas.CL);
		printf("  W2R: %d\t", gpu->cas.W2R);
		printf("  R2R: %d\t", gpu->cas.R2R);
		printf("  CCLD: %d\t", gpu->cas.CCLD);
		printf("  R2W: %d\t", gpu->cas.R2W);
		printf("  NOPR: %d\t", gpu->cas.NOPR);
		printf("  NOPW: %d\n", gpu->cas.NOPW);
		printf("RAS:\t");
		printf("  RC: %d\t", gpu->ras.RC);
		printf("  RRD: %d\t", gpu->ras.RRD);
		printf("  RCDRA: %d\t", gpu->ras.RCDRA);
		printf("  RCDR: %d\t", gpu->ras.RCDR);
		printf("  RCDWA: %d\t", gpu->ras.RCDWA);
		printf("  RCDW: %d\n", gpu->ras.RCDW);
		printf("MISC:\t");
		if (IsR9(chip->device_id))
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
		printf("MISC2:\t");
		printf("  WDATATR: %d\t", gpu->misc2.WDATATR);
		printf("  T32AW: %d\t", gpu->misc2.T32AW);
		printf("  CRCWL: %d\t", gpu->misc2.CRCWL);
		printf("  CRCRL: %d\t", gpu->misc2.CRCRL);
		printf("  FAW: %d\t", gpu->misc2.FAW);
		printf("  PA2WDATA: %d\t", gpu->misc2.PA2WDATA);
		printf("  PA2RDATA: %d\n", gpu->misc2.PA2RDATA);
		printf("M3(MR4):\t");
		printf("  RAS: %d\n", gpu->misc3.RAS);
		printf("DRAM1:\t");
		printf("  RASMACTWR: %d\t", gpu->dram1.RASMACTWR);
		printf("  RASMACTRD: %d\t", gpu->dram1.RASMACTRD);
		printf("  ACTWR: %d\t", gpu->dram1.ACTWR);
		printf("  ACTRD: %d\n", gpu->dram1.ACTRD);
		printf("DRAM2:\t");
		printf("  RAS2RAS: %d\t", gpu->dram2.RAS2RAS);
		printf("  RP: %d\t", gpu->dram2.RP);
		printf("  WRPLUSRP: %d\t", gpu->dram2.WRPLUSRP);
		printf("  BUS_TURN: %d\n", gpu->dram2.BUS_TURN);
	}
}

HMODULE hEIO = NULL;
void LoadDriver()
{
	hEIO = LoadLibraryA("EIO.dll");
	if (!hEIO)
	{
		printf("Failed to load EIO.dll\n");
		exit(EXIT_FAILURE);
	}
	g_hDevice = CreateFileA("\\\\.\\IOMap", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (g_hDevice == INVALID_HANDLE_VALUE)
	{
		printf("Failed to open IOMap device\n");
		exit(EXIT_FAILURE);
	}
}
void __cdecl UnloadDriver(void)
{
	if (hEIO)
	{
		FreeLibrary(hEIO);
	}
}

int main(int argc, const char* argv[])
{
	if ((argc < 2) || (0 == strcasecmp("--help", argv[1])))
	{
		printf(" AMD Memory Tweak\n"
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
			" Command line options: (GDDR5)\n"
			" --CKSRE|--cksre [value]\n"
			" --CKSRX|--cksrx [value]\n"
			" --CKE_PULSE|--cke_pulse [value]\n"
			" --CKE|--cke [value]\n"
			" --SEQ_IDLE|--seq_idle [value]\n"
			" --CL|--cl [value]\n"
			" --W2R|--w2r [value]\n"
			" --R2R|--r2r [value]\n"
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
			" --RAS|--ras [value]\n"
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
			" Hint: Certain timings such as CL (Cas Latency) are stability timings, lowering these will lower stability.\n");
		return EXIT_SUCCESS;
	}

	atexit(UnloadDriver);
	LoadDriver();

	const int PCI_VENDOR_ID_ATI = 0x1002;
	VGAChip chips[64];
	int gpuCount = EnumerateGPUs(PCI_VENDOR_ID_ATI, chips, sizeof(chips));
	if (gpuCount <= 0)
	{
		printf("No AMD display devices have been found!\n");
		return EXIT_SUCCESS;
	}

	GPU gpus[64];
	memset(gpus, 0, sizeof(gpus));
	for (int i = 0; i < gpuCount; i++)
	{
		GetVGAMMIOAddress(&chips[i], i);
		GetVGABIOSAddress(&chips[i], i);

		GPU* gpu = &gpus[i];
		switch (DetermineMemoryType(chips[i].device_id))
		{
		case HBM2:
			for (int j = 0; j < sizeof(HBM2_TIMINGS) / 4; j++)
			{
				*((u32*)& gpu->hbm2 + j) = ReadMMIODword(AMD_TIMING_REGS_BASE_1 + j * sizeof(u32), i);
			}
			break;
		case GDDR5:
			gpu->pmg.value = ReadMMIODword(MC_SEQ_PMG_TIMING, i);
			gpu->ras.value = ReadMMIODword(MC_SEQ_RAS_TIMING, i);
			gpu->cas.value = ReadMMIODword(MC_SEQ_CAS_TIMING, i);
			gpu->misc.value = ReadMMIODword(MC_SEQ_MISC_TIMING, i);
			gpu->misc2.value = ReadMMIODword(MC_SEQ_MISC_TIMING2, i);
			gpu->misc3.value = ReadMMIODword(MC_SEQ_MISC3, i);
			gpu->dram1.value = ReadMMIODword(MC_ARB_DRAM_TIMING, i);
			gpu->dram2.value = ReadMMIODword(MC_ARB_DRAM_TIMING2, i);
			break;
		case HBM:
			// only RAS gives correct values
			//gpu->ras.value = ReadMMIODword(0x28, i);
			/*
			gpu->cas = ReadMMIODword(0x28, i);
			gpu->misc = ReadMMIODword(0x28, i);
			gpu->misc2 = ReadMMIODword(0x28, i);
			gpu->dram1 = ReadMMIODword(MC_ARB_DRAM_TIMING, i);
			gpu->dram2 = ReadMMIODword(MC_ARB_DRAM_TIMING2, i);
			*/
			break;
		}
	}

	// Parses the command line arguments, and accumulates the changes.
	for (int index = 0; index < gpuCount; index++)
	{
		GPU* gpu = &gpus[index];
		if (IsRelevantDeviceID(chips[index].device_id))
		{
			u64 affectedGPUs = 0xFFFFFFFFFFFFFFFF; // apply to all GPUs by default
			for (int i = 1; i < argc; i++)
			{
				if (!strcasecmp("--gpu", argv[i]) || !strcasecmp("--i", argv[i]))
				{
					if (i == (argc - 1))
					{
						printf("Argument \"%s\" requires a parameter.\n", argv[i]);
						return EXIT_FAILURE;
					}
					i++;
					affectedGPUs = ParseIndicesArg(argv[i]);
				}
				else if (!strcasecmp("--current", argv[i]))
				{
					if (affectedGPUs & ((u64)1 << index))
					{
						printf("GPU %d:  %s\t", index, LookupDeviceName(chips[index].device_id));
						PrintCurrentValues(&chips[index], gpu);
					}
				}
				else if (affectedGPUs & ((u64)1 << index))
				{
					u32 value = 0;
					if (DetermineMemoryType(chips[index].device_id) == HBM2)
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
					else // GDDR5 & HBM
					{
						if (ParseNumericArg(argc, argv, i, "--CKSRE", value))
						{
							gpu->pmg.CKSRE = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CKSRE");
						}
						else if (ParseNumericArg(argc, argv, i, "--CKSRX", value))
						{
							gpu->pmg.CKSRX = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CKSRX");
						}
						else if (ParseNumericArg(argc, argv, i, "--CKE_PULSE", value))
						{
							gpu->pmg.CKE_PULSE = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CKE_PULSE");
						}
						else if (ParseNumericArg(argc, argv, i, "--CKE", value))
						{
							gpu->pmg.CKE = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CKE");
						}
						else if (ParseNumericArg(argc, argv, i, "--SEQ_IDLE", value))
						{
							gpu->pmg.SEQ_IDLE = value;
							gpu->modify[0] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "SEQ_IDLE");
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
							gpu->cas.CL = value;
							gpu->modify[1] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CL");
						}
						else if (ParseNumericArg(argc, argv, i, "--W2R", value))
						{
							gpu->cas.W2R = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "W2R");
						}
						else if (ParseNumericArg(argc, argv, i, "--R2R", value))
						{
							gpu->cas.R2R = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "R2R");
						}
						else if (ParseNumericArg(argc, argv, i, "--CCLD", value))
						{
							gpu->cas.CCLD = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CCLD");
						}
						else if (ParseNumericArg(argc, argv, i, "--R2W", value))
						{
							gpu->cas.R2W = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "R2W");
						}
						else if (ParseNumericArg(argc, argv, i, "--NOPR", value))
						{
							gpu->cas.NOPR = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "NOPR");
						}
						else if (ParseNumericArg(argc, argv, i, "--NOPW", value))
						{
							gpu->cas.NOPW = value;
							gpu->modify[2] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "NOPW");
						}
						else if (ParseNumericArg(argc, argv, i, "--RFC", value))
						{
							if (IsR9(chips[index].device_id))
								gpu->misc.r9.RFC = value;
							else
								gpu->misc.rx.RFC = value;
							gpu->modify[3] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RFC");
						}
						else if (ParseNumericArg(argc, argv, i, "--TRP", value))
						{
							if (IsR9(chips[index].device_id))
								gpu->misc.r9.TRP = value;
							else
								gpu->misc.rx.TRP = value;
							gpu->modify[3] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "TRP");
						}
						else if (ParseNumericArg(argc, argv, i, "--RP_RDA", value))
						{
							if (IsR9(chips[index].device_id))
								gpu->misc.r9.RP_RDA = value;
							else
								gpu->misc.rx.RP_RDA = value;
							gpu->modify[3] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RP_RDA");
						}
						else if (ParseNumericArg(argc, argv, i, "--RP_WRA", value))
						{
							if (IsR9(chips[index].device_id))
								gpu->misc.r9.RP_WRA = value;
							else
								gpu->misc.rx.RP_WRA = value;
							gpu->modify[3] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RP_WRA");
						}
						else if (ParseNumericArg(argc, argv, i, "--WDATATR", value))
						{
							gpu->misc2.WDATATR = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WDATATR");
						}
						else if (ParseNumericArg(argc, argv, i, "--T32AW", value))
						{
							gpu->misc2.T32AW = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "T32AW");
						}
						else if (ParseNumericArg(argc, argv, i, "--CRCWL", value))
						{
							gpu->misc2.CRCWL = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CRCWL");
						}
						else if (ParseNumericArg(argc, argv, i, "--CRCRL", value))
						{
							gpu->misc2.CRCRL = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "CRCRL");
						}
						else if (ParseNumericArg(argc, argv, i, "--FAW", value))
						{
							gpu->misc2.FAW = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "FAW");
						}
						else if (ParseNumericArg(argc, argv, i, "--PA2WDATA", value))
						{
							gpu->misc2.PA2WDATA = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "P2WDATA");
						}
						else if (ParseNumericArg(argc, argv, i, "--PA2RDATA", value))
						{
							gpu->misc2.PA2RDATA = value;
							gpu->modify[4] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "PA2RDATA");
						}
						else if (ParseNumericArg(argc, argv, i, "--RAS", value))
						{
							gpu->misc3.RAS = value;
							gpu->modify[5] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "PA2RDATA");
						}
						else if (ParseNumericArg(argc, argv, i, "--ACTRD", value))
						{
							gpu->dram1.ACTRD = value;
							gpu->modify[6] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "ACTRD");
						}
						else if (ParseNumericArg(argc, argv, i, "--ACTWR", value))
						{
							gpu->dram1.ACTWR = value;
							gpu->modify[6] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "ACTWR");
						}
						else if (ParseNumericArg(argc, argv, i, "--RASMACTRD", value))
						{
							gpu->dram1.RASMACTRD = value;
							gpu->modify[6] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RASMACTRD");
						}
						else if (ParseNumericArg(argc, argv, i, "--RASMACTWR", value))
						{
							gpu->dram1.RASMACTWR = value;
							gpu->modify[6] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RASMACTWR");
						}
						else if (ParseNumericArg(argc, argv, i, "--RAS2RAS", value))
						{
							gpu->dram2.RAS2RAS = value;
							gpu->modify[7] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RAS2RAS");
						}
						else if (ParseNumericArg(argc, argv, i, "--RP", value))
						{
							gpu->dram2.RP = value;
							gpu->modify[7] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "RP");
						}
						else if (ParseNumericArg(argc, argv, i, "--WRPLUSRP", value))
						{
							gpu->dram2.WRPLUSRP = value;
							gpu->modify[7] = true;
							if (gpu->log[0]) strcat(gpu->log, ", ");
							strcat(gpu->log, "WRPLUSRP");
						}
						else if (ParseNumericArg(argc, argv, i, "--BUS_TURN", value))
						{
							gpu->dram2.BUS_TURN = value;
							gpu->modify[7] = true;
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
		GPU* gpu = &gpus[index];
		for (int i = 0; i < _countof(gpu->modify); i++)
		{
			if (gpu->modify[i])
			{
				switch (DetermineMemoryType(chips[index].device_id))
				{
				case HBM2:
				{
					u32 value = ((u32*)& gpu->hbm2)[i];
					if (i == 0) // special logic for frequency
					{
						value = (gpu->hbm2.frequency == 0x118) ? 0x118 : 0x11C;
					}
					WriteMMIODword(AMD_TIMING_REGS_BASE_1 + (i * sizeof(u32)), value, index);
					WriteMMIODword(AMD_TIMING_REGS_BASE_2 + (i * sizeof(u32)), value, index);
					WriteMMIODword(AMD_TIMING_REGS_BASE_3 + (i * sizeof(u32)), value, index);
					WriteMMIODword(AMD_TIMING_REGS_BASE_4 + (i * sizeof(u32)), value, index);
					break;
				}
				case GDDR5:
					switch (i)
					{
					case 0:
						WriteMMIODword(MC_SEQ_PMG_TIMING, gpu->pmg.value, index);
						break;
					case 1:
						WriteMMIODword(MC_SEQ_RAS_TIMING, gpu->ras.value, index);
						break;
					case 2:
						WriteMMIODword(MC_SEQ_CAS_TIMING, gpu->cas.value, index);
						break;
					case 3:
						WriteMMIODword(MC_SEQ_MISC_TIMING, gpu->misc.value, index);
						break;
					case 4:
						WriteMMIODword(MC_SEQ_MISC_TIMING2, gpu->misc2.value, index);
						break;
					case 5:
						WriteMMIODword(MC_SEQ_MISC3, gpu->misc3.value, index);
						break;
					case 6:
						WriteMMIODword(MC_ARB_DRAM_TIMING, gpu->dram1.value, index);
						break;
					case 7:
						WriteMMIODword(MC_ARB_DRAM_TIMING2, gpu->dram2.value, index);
						break;
					}
					break;
				case HBM:
					printf("HBM memory type is not supported yet\n");
					return EXIT_FAILURE;
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

	return EXIT_SUCCESS;
}
