/** @file
 *
 *  Board init for the ROC-RK3568-PC platform
 *
 *  Copyright (c) 2021-2022, Jared McNeill <jmcneill@invisible.ca>
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <Base.h>
#include <Library/ArmLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/CruLib.h>
#include <Library/GpioLib.h>
#include <Library/I2cLib.h>
#include <Library/MultiPhyLib.h>
#include <Library/OtpLib.h>
#include <Library/SocLib.h>
#include <Library/Pcie30PhyLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>

#include <IndustryStandard/Rk356x.h>
#include <IndustryStandard/Rk356xCru.h>

/*
 * PMIC registers
*/
#define PMIC_I2C_ADDR           0x20

#define PMIC_CHIP_NAME          0xed
#define PMIC_CHIP_VER           0xee
#define PMIC_POWER_EN1          0xb2
#define PMIC_POWER_EN2          0xb3
#define PMIC_POWER_EN3          0xb4
#define PMIC_LDO1_ON_VSEL       0xcc
#define PMIC_LDO2_ON_VSEL       0xce
#define PMIC_LDO3_ON_VSEL       0xd0
#define PMIC_LDO4_ON_VSEL       0xd2
#define PMIC_LDO5_ON_VSEL       0xd4
#define PMIC_LDO6_ON_VSEL       0xd6
#define PMIC_LDO7_ON_VSEL       0xd8
#define PMIC_LDO8_ON_VSEL       0xda
#define PMIC_LDO9_ON_VSEL       0xdc
#define PMIC_BUCK5_SW1_CONFIG0  0xde
#define PMIC_BUCK5_CONFIG1	0xdf

/*
 * CPU_GRF registers
*/
#define GRF_CPU_COREPVTPLL_CON0               (CPU_GRF + 0x0010)
#define  CORE_PVTPLL_RING_LENGTH_SEL_SHIFT    3
#define  CORE_PVTPLL_RING_LENGTH_SEL_MASK     (0x1FU << CORE_PVTPLL_RING_LENGTH_SEL_SHIFT)
#define  CORE_PVTPLL_OSC_EN                   BIT1
#define  CORE_PVTPLL_START                    BIT0

/*
 * SYS_GRF registers
 */
#define GRF_IOFUNC_SEL0                       (SYS_GRF + 0x0300)
#define  GMAC1_IOMUX_SEL                      BIT8
#define GRF_IOFUNC_SEL3                       (SYS_GRF + 0x030c)
#define  UART3_IOMUX_SEL                      BIT14
#define  UART4_IOMUX_SEL                      BIT12
#define GRF_IOFUNC_SEL5                       (SYS_GRF + 0x0314)
#define  PCIE20_IOMUX_SEL_MASK                (BIT3|BIT2)
#define  PCIE20_IOMUX_SEL_M1                  BIT2
#define  PCIE20_IOMUX_SEL_M2                  BIT3
#define  PCIE30X2_IOMUX_SEL_MASK              (BIT7|BIT6)
#define  PCIE30X2_IOMUX_SEL_M1                BIT6
#define  PCIE30X2_IOMUX_SEL_M2                BIT7

/*
 * PMU registers
 */
#define PMU_NOC_AUTO_CON0                     (PMU_BASE + 0x0070)
#define PMU_NOC_AUTO_CON1                     (PMU_BASE + 0x0074)

STATIC CONST GPIO_IOMUX_CONFIG mSdmmc0IomuxConfig[] = {
  { "sdmmc0_d0",        1, GPIO_PIN_PD5, 1, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
  { "sdmmc0_d1",        1, GPIO_PIN_PD6, 1, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
  { "sdmmc0_d2",        1, GPIO_PIN_PD7, 1, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
  { "sdmmc0_d3",        2, GPIO_PIN_PA0, 1, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
  { "sdmmc0_cmd",       2, GPIO_PIN_PA1, 1, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
  { "sdmmc0_clk",       2, GPIO_PIN_PA2, 1, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
  { "sdmmc0_pwr", 	2, GPIO_PIN_PB0, 0, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
  { "sdmmc0_1v8en", 	2, GPIO_PIN_PB7, 0, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
};

STATIC CONST GPIO_IOMUX_CONFIG mEmmcIomuxConfig[] = {
  { "emmc_d0",        1, GPIO_PIN_PB4, 1, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
  { "emmc_d1",        1, GPIO_PIN_PB5, 1, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
  { "emmc_d2",        1, GPIO_PIN_PB6, 1, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
  { "emmc_d3",        1, GPIO_PIN_PB7, 1, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
  { "emmc_d4",        1, GPIO_PIN_PC0, 1, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
  { "emmc_d5",        1, GPIO_PIN_PC1, 1, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
  { "emmc_d6",        1, GPIO_PIN_PC2, 1, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
  { "emmc_d7",        1, GPIO_PIN_PC3, 1, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
  { "emmc_rstn",      1, GPIO_PIN_PC7, 1, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
  { "emmc_cmd",       1, GPIO_PIN_PC4, 1, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
  { "emmc_clk",       1, GPIO_PIN_PC5, 1, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
  { "emmc_dsk",       1, GPIO_PIN_PC6, 1, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2 },
};

STATIC CONST GPIO_IOMUX_CONFIG mPcie30x2IomuxConfig[] = {
  { "pcie30x2_clkreqnm2", 4, GPIO_PIN_PC2, 4, GPIO_PIN_PULL_NONE, GPIO_PIN_DRIVE_DEFAULT },
  { "pcie30x2_perstnm2",  4, GPIO_PIN_PC4, 4, GPIO_PIN_PULL_NONE, GPIO_PIN_DRIVE_DEFAULT },
  { "pcie30x2_wakenm2",   4, GPIO_PIN_PC3, 4, GPIO_PIN_PULL_NONE, GPIO_PIN_DRIVE_DEFAULT },
  { "pcie30x2_clkreqsoc", 4, GPIO_PIN_PC6, 0, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2       }, /* GPIO */
};

STATIC CONST GPIO_IOMUX_CONFIG mPcie20IomuxConfig[] = {
  { "pcie20_clkreqnm2", 1, GPIO_PIN_PB0, 4, GPIO_PIN_PULL_NONE, GPIO_PIN_DRIVE_DEFAULT },
  { "pcie20_perstnm2",  1, GPIO_PIN_PB2, 4, GPIO_PIN_PULL_NONE, GPIO_PIN_DRIVE_DEFAULT },
  { "pcie20_wakenm2",   1, GPIO_PIN_PB1, 4, GPIO_PIN_PULL_NONE, GPIO_PIN_DRIVE_DEFAULT },
  { "pcie20_clkreqsoc", 1, GPIO_PIN_PA4, 0, GPIO_PIN_PULL_UP,   GPIO_PIN_DRIVE_2       },   /* GPIO */
};

STATIC CONST GPIO_IOMUX_CONFIG mI2C0IomuxConfig[] = {
  { "i2c0_scl", 0, GPIO_PIN_PB1, 1, GPIO_PIN_PULL_NONE, GPIO_PIN_INPUT_SCHMITT },
  { "i2c0_sda", 0, GPIO_PIN_PB2, 1, GPIO_PIN_PULL_NONE, GPIO_PIN_INPUT_SCHMITT },
};


STATIC
VOID
BoardInitPcie (
  VOID
  )
{
    UINT32 grf_iofunc_sel5;

    /* Configure MULTI-PHY 2 for PCIe mode (PCIe2x1) */
    MultiPhySetMode (2, MULTIPHY_MODE_PCIE);
    /* PCIe20 */
    GpioSetIomuxConfig (mPcie20IomuxConfig, ARRAY_SIZE(mPcie20IomuxConfig));

    /* PCIe30x2 */
    GpioSetIomuxConfig (mPcie30x2IomuxConfig, ARRAY_SIZE (mPcie30x2IomuxConfig));

    /* PCI0_CLKREQ_SOC and PCI1_CLKREQ_SOC set to high */
    GpioPinSetDirection (1, GPIO_PIN_PA4, GPIO_PIN_OUTPUT);
    GpioPinWrite (1, GPIO_PIN_PA4, TRUE);

    GpioPinSetDirection (4, GPIO_PIN_PC6, GPIO_PIN_OUTPUT);
    GpioPinWrite (4, GPIO_PIN_PC6, TRUE);

    /* PCIe30x2 and PCIe20 IO  mux selection - M2 */
    grf_iofunc_sel5 = MmioRead32(GRF_IOFUNC_SEL5);
    grf_iofunc_sel5 |= ((PCIE20_IOMUX_SEL_MASK<<16)  | \
			(PCIE30X2_IOMUX_SEL_MASK<<16)| \
			PCIE30X2_IOMUX_SEL_M2 | \
			PCIE20_IOMUX_SEL_M2);
    MmioWrite32 (GRF_IOFUNC_SEL5, grf_iofunc_sel5);
}

STATIC
EFI_STATUS
PmicRead (
  IN UINT8 Register,
  OUT UINT8 *Value
  )
{
  return I2cRead (I2C0_BASE, PMIC_I2C_ADDR,
                  &Register, sizeof (Register),
                  Value, sizeof (*Value));
}

STATIC
EFI_STATUS
PmicWrite (
  IN UINT8 Register,
  IN UINT8 Value
  )
{
  return I2cWrite (I2C0_BASE, PMIC_I2C_ADDR,
                  &Register, sizeof (Register),
                  &Value, sizeof (Value));
}

STATIC
VOID
BoardInitPmic (
  VOID
  )
{
  EFI_STATUS Status;
  UINT16 ChipName;
  UINT8 ChipVer;
  UINT8 Value;

  DEBUG ((DEBUG_INFO, "BOARD: PMIC init\n"));

  GpioSetIomuxConfig (mI2C0IomuxConfig, ARRAY_SIZE(mI2C0IomuxConfig));

  Status = PmicRead (PMIC_CHIP_NAME, &Value);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Failed to read PMIC chip name! %r\n", Status));
    ASSERT (FALSE);
  }
  ChipName = (UINT16)Value << 4;

  Status = PmicRead (PMIC_CHIP_VER, &Value);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Failed to read PMIC chip version! %r\n", Status));
    ASSERT (FALSE);
  }
  ChipName |= (Value >> 4) & 0xF;
  ChipVer = Value & 0xF;

  DEBUG ((DEBUG_INFO, "PMIC: Detected RK%03X ver 0x%X\n", ChipName, ChipVer));
  ASSERT (ChipName == 0x809);

  /* Initialize PMIC */
  PmicWrite (PMIC_LDO1_ON_VSEL, 0x0c);  /* 0.9V - vdda0v9_image */
  PmicWrite (PMIC_LDO2_ON_VSEL, 0x0c);  /* 0.9V - vdda_0v9 */
  PmicWrite (PMIC_LDO3_ON_VSEL, 0x0c);  /* 0.9V - vdd0v9_pmu */
  PmicWrite (PMIC_LDO4_ON_VSEL, 0x6c);  /* 3.3V - vccio_acodec */
  PmicWrite (PMIC_LDO5_ON_VSEL, 0x6c);  /* 3.3V - sdmmc1_vio */
  PmicWrite (PMIC_LDO6_ON_VSEL, 0x6c);  /* 3.3V - vcc3v3_pmu */
  PmicWrite (PMIC_LDO7_ON_VSEL, 0x30);  /* 1.8V - vcca_1v8 */
  PmicWrite (PMIC_LDO8_ON_VSEL, 0x30);  /* 1.8V - vcca1v8_pmu */
  PmicWrite (PMIC_LDO9_ON_VSEL, 0x30);  /* 1.8V - vcca1v8_image */

  PmicWrite (PMIC_POWER_EN1, 0xff); /* LDO1, LDO2, LDO3, LDO4 */
  PmicWrite (PMIC_POWER_EN2, 0xff); /* LDO5, LDO6, LDO7, LDO8 */
  PmicWrite (PMIC_POWER_EN3, 0xff); /* LDO9, SW1, SW2, BUCK5  */

  PmicWrite (PMIC_BUCK5_SW1_CONFIG0, 0x09); /* BUCK5 1,8V@3A SWOUT1@1A */
  PmicWrite (PMIC_BUCK5_CONFIG1, 0x31);     /* BUCK5 SLP 1,8V SWOUT2@4A */
}

EFI_STATUS
EFIAPI
BoardInitDriverEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  DEBUG ((DEBUG_INFO, "BOARD: BoardInitDriverEntryPoint() called\n"));

  /* Set CPU power domain */
/*SocSetDomainVoltage (PMUIO1, VCC_3V3); FixMe: PMUIO1 not defined */
  SocSetDomainVoltage (PMUIO2, VCC_1V8);
  SocSetDomainVoltage (VCCIO1, VCC_1V8);
  SocSetDomainVoltage (VCCIO2, VCC_1V8);
  SocSetDomainVoltage (VCCIO3, VCC_3V3);
  SocSetDomainVoltage (VCCIO4, VCC_1V8);
  SocSetDomainVoltage (VCCIO5, VCC_1V8);
  SocSetDomainVoltage (VCCIO6, VCC_1V8);
  SocSetDomainVoltage (VCCIO7, VCC_1V8);

  BoardInitPmic ();

  /* Enable automatic clock gating */
  MmioWrite32 (PMU_NOC_AUTO_CON0, 0xFFFFFFFFU);
  MmioWrite32 (PMU_NOC_AUTO_CON1, 0x000F000FU);

  /* Set core_pvtpll ring length */
  MmioWrite32 (GRF_CPU_COREPVTPLL_CON0,
               ((CORE_PVTPLL_RING_LENGTH_SEL_MASK | CORE_PVTPLL_OSC_EN | CORE_PVTPLL_START) << 16) |
               (5U << CORE_PVTPLL_RING_LENGTH_SEL_SHIFT) | CORE_PVTPLL_OSC_EN | CORE_PVTPLL_START);

  /* Configure MULTI-PHY 0 and 1 for USB3 mode */
  MultiPhySetMode (0, MULTIPHY_MODE_USB3);
  MultiPhySetMode (1, MULTIPHY_MODE_USB3);

  /* SD-card setup */
  GpioSetIomuxConfig (mSdmmc0IomuxConfig, ARRAY_SIZE (mSdmmc0IomuxConfig));

  /* eMMC setup */
  GpioSetIomuxConfig (mEmmcIomuxConfig, ARRAY_SIZE (mEmmcIomuxConfig));

  /* PCIe setup */
  BoardInitPcie ();

  return EFI_SUCCESS;
}
