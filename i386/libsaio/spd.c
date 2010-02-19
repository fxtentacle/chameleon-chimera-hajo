/*
 * spd.c - serial presence detect memory information
 * (restored from pcefi10.5)
 */

#include "libsaio.h"
#include "pci.h"
#include "platform.h"
#include "spd.h"
#include "saio_internal.h"
#include "bootstruct.h"

#ifndef DEBUG_SPD
#define DEBUG_SPD 0
#endif

#if DEBUG_SPD
#define DBG(x...)	printf(x)
#else
#define DBG(x...)
#endif

static const char *spd_memory_types[] =
{
	"RAM",          /* 00h  Undefined */
	"FPM",          /* 01h  FPM */
	"EDO",          /* 02h  EDO */
	"",				/* 03h  PIPELINE NIBBLE */
	"SDRAM",        /* 04h  SDRAM */
	"",				/* 05h  MULTIPLEXED ROM */
	"DDR SGRAM",	/* 06h  SGRAM DDR */
	"DDR SDRAM",	/* 07h  SDRAM DDR */
	"DDR2 SDRAM",   /* 08h  SDRAM DDR 2 */
	"",				/* 09h  Undefined */
	"",				/* 0Ah  Undefined */
	"DDR3 SDRAM"   /* 0Bh  SDRAM DDR 3 */
};

#define UNKNOWN_MEM_TYPE 2
static uint8_t spd_mem_to_smbios[] =
{
	UNKNOWN_MEM_TYPE,          /* 00h  Undefined */
	UNKNOWN_MEM_TYPE,          /* 01h  FPM */
	UNKNOWN_MEM_TYPE,          /* 02h  EDO */
	UNKNOWN_MEM_TYPE,	   /* 03h  PIPELINE NIBBLE */
	SMB_MEM_TYPE_SDRAM,        /* 04h  SDRAM */
	SMB_MEM_TYPE_ROM,	   /* 05h  MULTIPLEXED ROM */
	SMB_MEM_TYPE_SGRAM,        /* 06h  SGRAM DDR */
	SMB_MEM_TYPE_DDR,          /* 07h  SDRAM DDR */
	SMB_MEM_TYPE_DDR2,         /* 08h  SDRAM DDR 2 */
	UNKNOWN_MEM_TYPE,  	   /* 09h  Undefined */
	UNKNOWN_MEM_TYPE,	   /* 0Ah  Undefined */
	SMB_MEM_TYPE_DDR3          /* 0Bh  SDRAM DDR 3 */
};
#define SPD_TO_SMBIOS_SIZE (sizeof(spd_mem_to_smbios)/sizeof(uint8_t))

typedef struct _vidTag {
    uint16_t code;
    const char*  name;
} VenIdName;

VenIdName vendorMap[] = {
    {0xCD04, "G Skill Intl"}, // id=CD Bank=5
    {0xB004, "OCZ"}, // id=B0 Bank=5
    {0x9801, "Kingston"}, // id=98 Bank=2
    {0x9E02, "Corsair"}, // id=9E Bank=3
    {0x0205, "Patriot Memory"}, // id=02 Bank=6
    {0x9B05, "Crucial Technology"}, // id=9B Bank=6
    {0xBA01, "PNY Electronics"}, // id=BA Bank=2
    {0x4F01, "Transcend Information"}, // id=4F Bank=2
    {0x1903, "Centon Electronics"}, // id=19 Bank=4
    {0x4001, "Viking Components"} // id=40 Bank=2
};
#define VEN_MAP_SIZE (sizeof(vendorMap)/sizeof(VenIdName))

#define rdtsc(low,high) \
__asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))

#define SMBHSTSTS 0
#define SMBHSTCNT 2
#define SMBHSTCMD 3
#define SMBHSTADD 4
#define SMBHSTDAT 5


const char * getVendorName(const char * spd)
{
    uint16_t code = *((uint16_t*) &spd[0x75]);
    int i;
    uint16_t bank=0;

    if (spd[2]==0x0b) { // DDR3
        for (i=0; i < VEN_MAP_SIZE; i++)
            if (code==vendorMap[i].code)
                return vendorMap[i].name;
    }
    else if (spd[2]==0x08 || spd[2]==0x07) { // DDR2 or DDR
        for (i=64; i<72 && spd[i]==0x7f;i++) bank++;
        code = bank+((uint16_t)spd[i])*256;
        for (i=0; i < VEN_MAP_SIZE; i++)
            if (code==vendorMap[i].code)
                return vendorMap[i].name;
    }

    return "No Name";
}

int getDDRspeedMhz(const char * spd)
{
    if (spd[2]==0x0b) { // DDR3
        switch(spd[12])  {
        case 0x0f:
            return 1066;
        case 0x0c:
            return 1333;
        case 0x0a:
            return 1600;
        case 0x14:
        default:
            return 800;
        }
    } 
    else if (spd[2]==0x08)  { // DDR2
        switch(spd[9]) {
        case 0x50:
            return 400;
        case 0x3d:
            return 533;
        case 0x30:
            return 667;
        case 0x25:
        default:
            return 800;
        }
    }
    return  800; // default freq for unknown types
}

#define UIS(a) ((uint32_t)spd[a])
uint32_t getDDRSerial(const char* spd)
{
    uint32_t ret=0;

    if  (spd[2]==0x0b) // DDR3
        // assume it is lsb to msb
        ret = UIS(122) | (UIS(123)<<8) | (UIS(124)<<16) | (UIS(125)<<24);
    else if  (spd[2]==0x08 || spd[2]==0x07) // DDR2 or DDR
        ret =  UIS(95) | (UIS(96)<<8) | (UIS(97)<<16) | (UIS(98)<<24);
    return ret;
}

char * getDDRPartNum(const char* spd)
{
    if (spd[2]==0x0b) // DDR3
        return &spd[128];
    else if (spd[2]==0x08 || spd[2]==0x07) // DDR2 or DDR
        return &spd[73];
    return "";
}
unsigned char smb_read_byte_intel(uint32_t base, uint8_t adr, uint8_t cmd)
{
	int l1, h1, l2, h2;
    unsigned long long t;
	
    outb(base + SMBHSTSTS, 0x1f);					// reset SMBus Controller
    outb(base + SMBHSTDAT, 0xff);
	
    while( inb(base + SMBHSTSTS) & 0x01);			// wait until ready
	
    outb(base + SMBHSTCMD, cmd);
    outb(base + SMBHSTADD, (adr << 1) | 0x01 );
    outb(base + SMBHSTCNT, 0x48 );
	
    rdtsc(l1, h1);
	
 	while (!( inb(base + SMBHSTSTS) & 0x02))		// wait til command finished
	{	
		rdtsc(l2, h2);
		t = ((h2 - h1) * 0xffffffff + (l2 - l1)) / (Platform.CPU.TSCFrequency / 40);
		if (t > 10)
			break;									// break after 10ms
    }
    return inb(base + SMBHSTDAT);
}

int mapping []={0,1,2,3,4,5}; // linear mapping for now, check me

static void read_smb_intel(pci_dt_t *smbus_dev)
{ 
    static int serialnum=0;
    int i, x, ser;
    uint8_t		spd_size, spd_type;
    uint32_t	base;
    bool            dump = false;
    RamSlotInfo_t*  slot;

    base = pci_config_read16(smbus_dev->dev.addr, 0x20) & 0xFFFE;
    DBG("Scanning smbus_dev <%04x, %04x> ...\n",smbus_dev->vendor_id, smbus_dev->device_id);

    getBoolForKey("DumpSPD", &dump, &bootInfo->bootConfig);

   // Search MAX_RAM_SLOTS slots
    for (i = 0; i < 6; i++){
        slot = &Platform.RAM.DIMM[i];
        Platform.DMI.DIMM[i]=mapping[i]; // for now no special mapping
        spd_size = smb_read_byte_intel(base, 0x50 + i, 0);
        
        // Check spd is present
        if (spd_size && spd_size != 0xff)
            {
                slot->InUse = true;
                
                slot->spd = malloc(spd_size);
                if (slot->spd)   {
                    bzero(slot->spd, spd_size);
                    
                    // Copy spd data into buffer
                    for (x = 0; x < spd_size; x++)
                        slot->spd[x] = smb_read_byte_intel(base, 0x50 + i, x);
                    
                    switch (slot->spd[SPD_MEMORY_TYPE])  {
                    case SPD_MEMORY_TYPE_SDRAM_DDR2:
                        
                        slot->ModuleSize = ((1 << (slot->spd[SPD_NUM_ROWS] & 0x0f) + (slot->spd[SPD_NUM_COLUMNS] & 0x0f) - 17) * 
                                            ((slot->spd[SPD_NUM_DIMM_BANKS] & 0x7) + 1) * slot->spd[SPD_NUM_BANKS_PER_SDRAM]);
                        break;
                        
                    case SPD_MEMORY_TYPE_SDRAM_DDR3:
                        
                        slot->ModuleSize = ((slot->spd[4] & 0x0f) + 28 ) + ((slot->spd[8] & 0x7)  + 3 );
                        slot->ModuleSize -= (slot->spd[7] & 0x7) + 25;
                        slot->ModuleSize = ((1 << slot->ModuleSize) * (((slot->spd[7] >> 3) & 0x1f) + 1));
                        
                        break;
                    }
                }
                
                spd_type = (slot->spd[SPD_MEMORY_TYPE] < ((char) 12) ? slot->spd[SPD_MEMORY_TYPE] : 0);
                slot->Type = spd_mem_to_smbios[spd_type];
                strncpy(slot->PartNo, getDDRPartNum(slot->spd), 64);
                strncpy(slot->Vendor, getVendorName(slot->spd), 64);

                ser = getDDRSerial(slot->spd);
                if (ser==0) {
                    sprintf(slot->SerialNo, "10000000%d", serialnum);
                    serialnum++;
                }
                else
                    sprintf(slot->SerialNo, "%d", ser);
                // determine speed
                slot->Frequency = getDDRspeedMhz(slot->spd);
                if(dump) {
                    printf("Slot %d Type %d %dMB (%s) %dMHz Vendor=%s, PartNo=%s SerialNo=%s\n", 
                        i, 
                        (int)slot->Type,
                        slot->ModuleSize, 
                        spd_memory_types[spd_type],
                        slot->Frequency,
                        slot->Vendor,
                        slot->PartNo,
                        slot->SerialNo); 
                    dumpPhysAddr("spd content: ",slot->spd, spd_size);
                    getc();
                }
            }
    }
#if DEBUG_SPD
    printf("Press a key to continue\n");
    getc();
#endif
}

static struct smbus_controllers_t smbus_controllers[] = {

	{0x8086, 0x269B, "ESB2",    read_smb_intel },
	{0x8086, 0x25A4, "6300ESB", read_smb_intel },
	{0x8086, 0x24C3, "ICH4",    read_smb_intel },
	{0x8086, 0x24D3, "ICH5",    read_smb_intel },
	{0x8086, 0x266A, "ICH6",    read_smb_intel },
	{0x8086, 0x27DA, "ICH7",    read_smb_intel },
	{0x8086, 0x283E, "ICH8",    read_smb_intel },
	{0x8086, 0x2930, "ICH9",    read_smb_intel },	
	{0x8086, 0x3A30, "ICH10R",  read_smb_intel },
	{0x8086, 0x3A60, "ICH10B",  read_smb_intel },
	{0x8086, 0x3B30, "P55",     read_smb_intel },
	{0x8086, 0x5032, "EP80579", read_smb_intel }

};

void scan_smbus_controller(pci_dt_t *smbus_dev)
{
	int	i;

	for( i = 1; i <  sizeof(smbus_controllers) / sizeof(smbus_controllers[0]); i++ )
		if (( smbus_controllers[i].vendor == smbus_dev->vendor_id) 
			&& ( smbus_controllers[i].device == smbus_dev->device_id))
		{
			verbose("%s%s SMBus Controller [%4x:%4x] at %02x:%02x.%x\n", 
				   (smbus_dev->vendor_id == 0x8086) ? "Intel(R) " : "",
				   smbus_controllers[i].name,
				   smbus_dev->vendor_id, smbus_dev->device_id,
				   smbus_dev->dev.bits.bus, smbus_dev->dev.bits.dev, smbus_dev->dev.bits.func);
			
			smbus_controllers[i].read_smb(smbus_dev);
			
		}
	
}

// initial call : pci_dt = root_pci_dev;
// find_and_read_smbus_controller(root_pci_dev);
bool find_and_read_smbus_controller(pci_dt_t* pci_dt)
{
    pci_dt_t	*current = pci_dt;
    int i;

    while (current) {
#if DEBUG_SPD
        printf("%02x:%02x.%x [%04x] [%04x:%04x] :: %s\n", 
               current->dev.bits.bus, current->dev.bits.dev, current->dev.bits.func, 
               current->class_id, current->vendor_id, current->device_id, 
               get_pci_dev_path(current));
#endif
	for ( i = 0; i <  sizeof(smbus_controllers) / sizeof(smbus_controllers[0]); i++ )
        {
            if (current->vendor_id == smbus_controllers[i].vendor &&
                current->device_id == smbus_controllers[i].device)
            {
                smbus_controllers[i].read_smb(current); // read smb
                return true;
            }        
        }
        find_and_read_smbus_controller(current->children);
        current = current->next;
    }
    return false; // not found
}

void scan_spd(PlatformInfo_t *p)
{
    find_and_read_smbus_controller(root_pci_dev);
}
