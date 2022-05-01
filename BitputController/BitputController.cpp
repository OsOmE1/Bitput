#include <iostream>
#include <Windows.h>

#define IO_TEST CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0420, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)

int main() {
    HANDLE driver = CreateFileA("\\\\.\\Bitput", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    char* output = NULL;
    bool succesfull = DeviceIoControl(driver, IO_TEST, output, sizeof(char) * 4, output, sizeof(char) * 4, 0, 0);
    if (!succesfull) {
        std::cout << "failed";
        return 0;
    }
    std::cout << output << "\n" << succesfull;
}