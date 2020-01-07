#include "Bits.h"

#include "Util.h"

Bits Bits_Set(Bits bits, const Trigger trigger)
{
    return bits |= (1 << (int32_t) trigger);
}

bool Bits_Get(const Bits bits, const Trigger trigger)
{
    return (bits >> trigger) & 0x1;
}

void Bits_Print(const Bits bits)
{
    for(int32_t i = 63; i >= 0; i--)
        printf("%d", (int32_t) ((bits >> i) & 0x1));
    putchar('\n');
}
