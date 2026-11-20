MEMORY
{
    FLASH (rx) : ORIGIN = app_start, LENGTH = 0xed000 - app_start
    RAM (rwx) : ORIGIN = 0x20003400, LENGTH = 0x20040000 - 0x20003400
}

INCLUDE "nrf_common.ld"

__tbss_start__ = __tbss_start;
__tbss_end__ = __tbss_end;
__sbss_start__ = __sbss_start;
__sbss_end__ = __sbss_end;
