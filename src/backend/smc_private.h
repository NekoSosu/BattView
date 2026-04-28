#ifndef BATTERY_MONITOR_BACKEND_SMC_PRIVATE_H
#define BATTERY_MONITOR_BACKEND_SMC_PRIVATE_H

#include <IOKit/IOKitLib.h>

#define KERNEL_INDEX_SMC 2
#define SMC_CMD_READ_BYTES 5
#define SMC_CMD_READ_KEYINFO 9

typedef struct {
    char major;
    char minor;
    char build;
    char reserved[1];
    unsigned short release;
} SMCKeyDataVers;

typedef struct {
    unsigned short version;
    unsigned short length;
    unsigned int cpuPLimit;
    unsigned int gpuPLimit;
    unsigned int memPLimit;
} SMCKeyDataLimit;

typedef struct {
    unsigned int dataSize;
    unsigned int dataType;
    char dataAttributes;
} SMCKeyInfo;

typedef char SMCBytes[32];

typedef struct {
    unsigned int key;
    SMCKeyDataVers vers;
    SMCKeyDataLimit pLimitData;
    SMCKeyInfo keyInfo;
    char result;
    char status;
    char data8;
    unsigned int data32;
    SMCBytes bytes;
} SMCKeyData;

io_connect_t BatterySMCOpen(void);
void BatterySMCClose(io_connect_t conn);
kern_return_t BatterySMCReadKey(io_connect_t conn, const char *key, SMCKeyData *val);
double BatterySMCGetFloatValue(io_connect_t conn, const char *key);

#endif
