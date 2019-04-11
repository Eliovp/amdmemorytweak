typedef struct {
	WORD field_0;
	char pad1[2];
	WORD field_4;
	char pad2[2];
	WORD device_id;
	char pad3[2];
	WORD subdevice_id;
	char pad4[2];
	WORD subvendor_id;
	char pad5[2];
	DWORD field_14;
	DWORD field_18;
	DWORD field_1C;
} VGAChip;
C_ASSERT(sizeof(VGAChip) == 0x20);

//void LoadDriver();
//void __cdecl UnloadDriver(void);
int EnumerateGPUs(DWORD vendorId, VGAChip *OutBuffer, DWORD OutBufferSize);
int GetVGAMMIOAddress(VGAChip *chip, WORD adapterIndex);
int GetVGABIOSAddress(VGAChip *chip, WORD adapterIndex);
DWORD ReadMMIODword(DWORD address, WORD adapterIndex);
void WriteMMIODword(DWORD address, DWORD value, WORD adapterIndex);

extern HANDLE g_hDevice;
