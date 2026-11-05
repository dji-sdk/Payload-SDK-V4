#include "rndis_core.h"

#include <assert.h>

int main(void)
{
    USB_TO_ENET_DATA rx = {0};

    assert(sizeof(rx.USB_enet_data_buffer) >= 1514U);
    return 0;
}
