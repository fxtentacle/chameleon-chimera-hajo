/*
 * Add (c) here
 *
 * Copyright .... All rights reserved.
 *
 */

#include "smbios_getters.h"

#ifndef DEBUG_SMBIOS
#define DEBUG_SMBIOS 0
#endif

#if DEBUG_SMBIOS
#define DBG(x...)	printf(x)
#else
#define DBG(x...)
#endif


bool getProcessorInformationExternalClock(returnType *value)
{
	value->word = Platform.CPU.FSBFrequency/1000000;
	return true;
}

bool getProcessorInformationMaximumClock(returnType *value)
{
	value->word = Platform.CPU.CPUFrequency/1000000;
	return true;
}

bool getSMBOemProcessorBusSpeed(returnType *value)
{
	if (Platform.CPU.Vendor == 0x756E6547) // Intel
	{		
		switch (Platform.CPU.Family) 
		{
			case 0x06:
			{
				switch (Platform.CPU.Model)
				{
					case 0x0D:					// ?
					case CPU_MODEL_YONAH:		// Yonah		0x0E
					case CPU_MODEL_MEROM:		// Merom		0x0F
					case CPU_MODEL_PENRYN:		// Penryn		0x17
					case CPU_MODEL_ATOM:		// Atom 45nm	0x1C
						return false;

					case 0x19:					// Intel Core i5 650 @3.20 Ghz
					case CPU_MODEL_NEHALEM:		// Intel Core i7 LGA1366 (45nm)
					case CPU_MODEL_FIELDS:		// Intel Core i5, i7 LGA1156 (45nm)
					case CPU_MODEL_DALES:		// Intel Core i5, i7 LGA1156 (45nm) ???
					case CPU_MODEL_DALES_32NM:	// Intel Core i3, i5, i7 LGA1156 (32nm)
					case CPU_MODEL_WESTMERE:	// Intel Core i7 LGA1366 (32nm) 6 Core
					case CPU_MODEL_NEHALEM_EX:	// Intel Core i7 LGA1366 (45nm) 6 Core ???
					case CPU_MODEL_WESTMERE_EX:	// Intel Core i7 LGA1366 (45nm) 6 Core ???
					{
						// thanks to dgobe for i3/i5/i7 bus speed detection
						int nhm_bus = 0x3F;
						static long possible_nhm_bus[] = {0xFF, 0x7F, 0x3F};
						unsigned long did, vid;
						int i;
						
						// Nehalem supports Scrubbing
						// First, locate the PCI bus where the MCH is located
						for(i = 0; i < sizeof(possible_nhm_bus); i++)
						{
							vid = pci_config_read16(PCIADDR(possible_nhm_bus[i], 3, 4), 0x00);
							did = pci_config_read16(PCIADDR(possible_nhm_bus[i], 3, 4), 0x02);
							vid &= 0xFFFF;
							did &= 0xFF00;
							
							if(vid == 0x8086 && did >= 0x2C00)
								nhm_bus = possible_nhm_bus[i]; 
						}
						
						unsigned long qpimult, qpibusspeed;
						qpimult = pci_config_read32(PCIADDR(nhm_bus, 2, 1), 0x50);
						qpimult &= 0x7F;
						DBG("qpimult %d\n", qpimult);
						qpibusspeed = (qpimult * 2 * (Platform.CPU.FSBFrequency/1000000));
						// Rek: rounding decimals to match original mac profile info
						if (qpibusspeed%100 != 0)qpibusspeed = ((qpibusspeed+50)/100)*100;
						DBG("qpibusspeed %d\n", qpibusspeed);
						value->word = qpibusspeed;
						return true;
					}
				}
			}
		}
	}
	return false;
}

uint16_t simpleGetSMBOemProcessorType(void)
{
	if (Platform.CPU.NoCores >= 4) 
	{
		return 0x0501;	// Quad-Core Xeon
	}
	else if (Platform.CPU.NoCores == 1) 
	{
		return 0x0201;	// Core Solo
	};
	
	return 0x0301;		// Core 2 Duo
}

bool getSMBOemProcessorType(returnType *value)
{
	static bool done = false;		
		
	value->word = simpleGetSMBOemProcessorType();

	if (Platform.CPU.Vendor == 0x756E6547) // Intel
	{
		if (!done)
		{
			verbose("CPU is %s, family 0x%x, model 0x%x\n", Platform.CPU.BrandString, Platform.CPU.Family, Platform.CPU.Model);
			done = true;
		}
		
		switch (Platform.CPU.Family) 
		{
			case 0x06:
			{
				switch (Platform.CPU.Model)
				{
					case 0x0D:							// ?
					case CPU_MODEL_YONAH:				// Yonah
					case CPU_MODEL_MEROM:				// Merom
					case CPU_MODEL_PENRYN:				// Penryn
					case CPU_MODEL_ATOM:				// Intel Atom (45nm)
						return true;

					case CPU_MODEL_NEHALEM:				// Intel Core i7 LGA1366 (45nm)
						if (strstr(Platform.CPU.BrandString, "Xeon(R)"))
							value->word = 0x0501;			// Xeon 
						else
							value->word = 0x0701;			// Core i7

						return true;

					case CPU_MODEL_FIELDS:				// Lynnfield, Clarksfield, Jasper
						if (strstr(Platform.CPU.BrandString, "Core(TM) i5"))
							value->word = 0x601;		// Core i5
						else
							value->word = 0x0701;		// Core i7
						return true;

					case CPU_MODEL_DALES:				// Intel Core i5, i7 LGA1156 (45nm) (Havendale, Auburndale)
						if (strstr(Platform.CPU.BrandString, "Core(TM) i5"))
							value->word = 0x601;		// Core i5
						else
							value->word = 0x0701;		// Core i7
						return true;

					case CPU_MODEL_DALES_32NM:			// Intel Core i3, i5, i7 LGA1156 (32nm) (Clarkdale, Arrandale)
						if (strstr(Platform.CPU.BrandString, "Core(TM) i3"))
							value->word = 0x901;		// Core i3
						else
							if (strstr(Platform.CPU.BrandString, "Core(TM) i5"))
								value->word = 0x601;	// Core i5
							else
								value->word = 0x0701;	// Core i7
						return true;

					case CPU_MODEL_WESTMERE:			// Intel Core i7 LGA1366 (32nm) 6 Core (Gulftown, Westmere-EP, Westmere-WS)
					case CPU_MODEL_WESTMERE_EX:			// Intel Core i7 LGA1366 (45nm) 6 Core ???
						value->word = 0x0501;			// Core i7
						return true;

					case 0x19:							// Intel Core i5 650 @3.20 Ghz
						value->word = 0x601;			// Core i5
						return true;
				}
			}
		}
	}
	
	return false;
}

bool getSMBMemoryDeviceMemoryType(returnType *value)
{
	static int idx = -1;
	int	map;

	idx++;
	if (idx < MAX_RAM_SLOTS)
	{
		map = Platform.DMI.DIMM[idx];
		if (Platform.RAM.DIMM[map].InUse && Platform.RAM.DIMM[map].Type != 0)
		{
			DBG("RAM Detected Type = %d\n", Platform.RAM.DIMM[map].Type);
			value->byte = Platform.RAM.DIMM[map].Type;
			return true;
		}
	}
	
	return false;
//	value->byte = SMB_MEM_TYPE_DDR2;
//	return true;
}

bool getSMBMemoryDeviceMemorySpeed(returnType *value)
{
	static int idx = -1;
	int	map;

	idx++;
	if (idx < MAX_RAM_SLOTS)
	{
		map = Platform.DMI.DIMM[idx];
		if (Platform.RAM.DIMM[map].InUse && Platform.RAM.DIMM[map].Frequency != 0)
		{
			DBG("RAM Detected Freq = %d Mhz\n", Platform.RAM.DIMM[map].Frequency);
			value->dword = Platform.RAM.DIMM[map].Frequency;
			return true;
		}
	}

	return false;
//	value->dword = 800;
//	return true;
}

bool getSMBMemoryDeviceManufacturer(returnType *value)
{
	static int idx = -1;
	int	map;

	idx++;
	if (idx < MAX_RAM_SLOTS)
	{
		map = Platform.DMI.DIMM[idx];
		if (Platform.RAM.DIMM[map].InUse && strlen(Platform.RAM.DIMM[map].Vendor) > 0)
		{
			DBG("RAM Detected Vendor[%d]='%s'\n", idx, Platform.RAM.DIMM[map].Vendor);
			value->string = Platform.RAM.DIMM[map].Vendor;
			return true;
		}
	}

	return false;
//	value->string = "N/A";
//	return true;
}
	
bool getSMBMemoryDeviceSerialNumber(returnType *value)
{
	static int idx = -1;
	int	map;

	idx++;
	if (idx < MAX_RAM_SLOTS)
	{
		map = Platform.DMI.DIMM[idx];
		if (Platform.RAM.DIMM[map].InUse && strlen(Platform.RAM.DIMM[map].SerialNo) > 0)
		{
			DBG("name = %s, map=%d,  RAM Detected SerialNo[%d]='%s'\n", name ? name : "", 
				map, idx, Platform.RAM.DIMM[map].SerialNo);
			value->string = Platform.RAM.DIMM[map].SerialNo;
			return true;
		}
	}

	return false;
//	value->string = "N/A";
//	return true;
}

bool getSMBMemoryDevicePartNumber(returnType *value)
{
	static int idx = -1;
	int	map;

	idx++;
	if (idx < MAX_RAM_SLOTS)
	{
		map = Platform.DMI.DIMM[idx];
		if (Platform.RAM.DIMM[map].InUse && strlen(Platform.RAM.DIMM[map].PartNo) > 0)
		{
			DBG("Ram Detected PartNo[%d]='%s'\n", idx, Platform.RAM.DIMM[map].PartNo);
			value->string = Platform.RAM.DIMM[map].PartNo;
			return true;
		}
	}

	return false;
//	value->string = "N/A";
//	return true;
}


// getting smbios addr with fast compare ops, late checksum testing ...
#define COMPARE_DWORD(a,b) ( *((uint32_t *) a) == *((uint32_t *) b) )
static const char * const SMTAG = "_SM_";
static const char* const DMITAG = "_DMI_";

SMBEntryPoint *getAddressOfSmbiosTable(void)
{
	SMBEntryPoint	*smbios;
	/* 
	 * The logic is to start at 0xf0000 and end at 0xfffff iterating 16 bytes at a time looking
	 * for the SMBIOS entry-point structure anchor (literal ASCII "_SM_").
	 */
	smbios = (SMBEntryPoint*)SMBIOS_RANGE_START;
	while (smbios <= (SMBEntryPoint *)SMBIOS_RANGE_END) {
		if (COMPARE_DWORD(smbios->anchor, SMTAG)  && 
			COMPARE_DWORD(smbios->dmi.anchor, DMITAG) &&
			smbios->dmi.anchor[4] == DMITAG[4] &&
			checksum8(smbios, sizeof(SMBEntryPoint)) == 0)
	    {
			return smbios;
	    }
		smbios = (SMBEntryPoint*)(((char*)smbios) + 16);
	}
	printf("ERROR: Unable to find SMBIOS!\n");
	pause();
	return NULL;
}

