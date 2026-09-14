/*
** Mapper 104: Camerica / BIC-48
** 用于超级战魂等游戏
*/

#include <noftypes.h>
#include <nes_mmc.h>
#include <nes.h>
#include <libsnss.h>
#include <string.h>

static uint8 regs[8];

static void map104_write(uint32 address, uint8 value)
{
    switch (address & 0xF000)
    {
    case 0x8000:
        regs[0] = value;
        mmc_bankvrom(1, 0x0000, value & 0x07);
        break;

    case 0x9000:
        regs[1] = value;
        /* 镜像控制: bit0=0 垂直, bit0=1 水平 */
        if (value & 0x01)
            ppu_mirror(0, 0, 1, 1);
        else
            ppu_mirror(0, 1, 0, 1);
        break;

    case 0xA000:
        regs[2] = value;
        mmc_bankvrom(1, 0x0400, value & 0x07);
        break;

    case 0xB000:
        regs[3] = value;
        mmc_bankvrom(1, 0x0800, value & 0x07);
        break;

    case 0xC000:
        regs[4] = value;
        mmc_bankvrom(1, 0x0C00, value & 0x07);
        break;

    case 0xD000:
        regs[5] = value;
        /* 16KB ROM bank at $8000 */
        mmc_bankrom(16, 0x8000, value & (mmc_getinfo()->rom_banks * 2 - 1));
        break;

    case 0xE000:
        regs[6] = value;
        /* 8KB ROM bank at $C000 */
        mmc_bankrom(8, 0xC000, value & (mmc_getinfo()->rom_banks * 2 - 1));
        break;

    case 0xF000:
        regs[7] = value;
        break;
    }
}

static void map104_init(void)
{
    memset(regs, 0, sizeof(regs));

    /* 默认映射 */
    mmc_bankrom(16, 0x8000, 0);
    mmc_bankrom(8, 0xC000, mmc_getinfo()->rom_banks * 2 - 1);
    mmc_bankvrom(8, 0x0000, 0);
}

static map_memwrite map104_memwrite[] =
{
    { 0x8000, 0xFFFF, map104_write },
    {     -1,     -1, NULL }
};

mapintf_t map104_intf =
{
    104,
    "Camerica/BIC-48",
    map104_init,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    map104_memwrite,
    NULL
};