
/*
 * Copyright 2007 Hajo Nils Krabbenhöft.	 All rights reserved.
 */

#include "libsaio.h"
#include "boot.h"
#include "bootstruct.h"
#include "efi.h"
#include "acpi.h"
#include "fake_efi.h"
#include "efi_tables.h"
#include "platform.h"
#include "acpi_patcher.h"
#include "smbios.h"
#include "device_inject.h"
#include "convert.h"
#include "pci.h"
#include "sl.h"

void addHajoKey(const char* key) {
    if(key == 0 || strlen(key)<3) return;
    
    verbose("addHajoKey: \"%s\" \n", key);
    //sleep(5);
    
    const char *data = getStringForKey(key, &bootInfo->chameleonConfig);
    
    const char* colon = strchr(data, ':');
    const char* equal = strchr(colon, '=');
    
    char nodePath[128];
    strncpy(nodePath, data, colon-data);
    nodePath[colon-data] = 0;
    
    char attributeName[128];
    strncpy(attributeName, colon+1, equal-colon-1);
    nodePath[equal-colon-1] = 0;

    int len = 0;
    const char* attributeValue = convertHexStr2Binary(equal+1, &len);
    
    verbose("in node \"%s\" set property \"%s\" to \"%s\" \n", nodePath, attributeName, attributeValue);
    
 	Node* node = DT__FindNode(nodePath, true);
	if (node == 0) stop("Couldn't find/add node");

    DT__AddProperty(node, attributeName, len + 1, (char*)attributeValue);
}

void addMiscToDeviceTree(void)
{
    const char *hajokeys = getStringForKey("hajokeys", &bootInfo->chameleonConfig);
    if(!hajokeys) return;
    
    verbose("hajokeys: \"%s\" \n", hajokeys);
  
    const char* curpos = hajokeys;
    const char* end = curpos + strlen(hajokeys);
    
    while(curpos < end) {
        const char* space = strchr(curpos, ' ');
        const char* space2 = strchr(curpos, '\r');
        const char* space3 = strchr(curpos, '\n');
        if(space2 != 0 && space2 < space) space = space2;
        if(space3 != 0 && space3 < space) space = space3;
        if(!space) space = end;
        int len = space - curpos;
        
        char buffer[128];
        strncpy(buffer, curpos, len);
        buffer[len] = 0;
        
        addHajoKey(buffer);
        curpos = space+1;
    }
    
    //sleep(5);
}
