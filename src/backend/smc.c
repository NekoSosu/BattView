#include "smc_private.h"

#include <string.h>

io_connect_t BatterySMCOpen(void) {
    kern_return_t result;
    io_iterator_t iterator;
    io_object_t device;
    io_connect_t conn = 0;

    CFMutableDictionaryRef matching = IOServiceMatching("AppleSMC");
    result = IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator);
    if (result != kIOReturnSuccess) return 0;

    device = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (device == 0) return 0;

    result = IOServiceOpen(device, mach_task_self(), 0, &conn);
    IOObjectRelease(device);
    if (result != kIOReturnSuccess) return 0;
    return conn;
}

void BatterySMCClose(io_connect_t conn) {
    if (conn) IOServiceClose(conn);
}

static kern_return_t callSMC(io_connect_t conn, int index, SMCKeyData *input, SMCKeyData *output) {
    size_t inputSize = sizeof(SMCKeyData);
    size_t outputSize = sizeof(SMCKeyData);
    return IOConnectCallStructMethod(conn, index, input, inputSize, output, &outputSize);
}

kern_return_t BatterySMCReadKey(io_connect_t conn, const char *key, SMCKeyData *val) {
    if (!conn || !key || !val) return kIOReturnBadArgument;
    SMCKeyData input;
    SMCKeyData output;
    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));
    memset(val, 0, sizeof(*val));

    input.key = ((unsigned int)key[0] << 24) | ((unsigned int)key[1] << 16) | ((unsigned int)key[2] << 8) | (unsigned int)key[3];
    input.data8 = SMC_CMD_READ_KEYINFO;
    kern_return_t result = callSMC(conn, KERNEL_INDEX_SMC, &input, &output);
    if (result != kIOReturnSuccess) return result;

    val->keyInfo.dataSize = output.keyInfo.dataSize;
    val->keyInfo.dataType = output.keyInfo.dataType;
    input.keyInfo.dataSize = val->keyInfo.dataSize;
    input.data8 = SMC_CMD_READ_BYTES;
    result = callSMC(conn, KERNEL_INDEX_SMC, &input, &output);
    if (result != kIOReturnSuccess) return result;
    memcpy(val->bytes, output.bytes, sizeof(output.bytes));
    return kIOReturnSuccess;
}

double BatterySMCGetFloatValue(io_connect_t conn, const char *key) {
    SMCKeyData val;
    if (BatterySMCReadKey(conn, key, &val) != kIOReturnSuccess) return 0.0;
    if (val.keyInfo.dataType == 1718383648U) {
        float f;
        memcpy(&f, val.bytes, sizeof(float));
        return (double)f;
    }
    return 0.0;
}
