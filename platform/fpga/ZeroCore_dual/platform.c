/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2019 FORTH-ICS/CARV
 *				Panagiotis Peristerakis <perister@ics.forth.gr>
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_platform.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/fdt/fdt_fixup.h>
#include <sbi_utils/ipi/aclint_mswi.h>
#include <sbi_utils/serial/xlnx_uartlite.h>
#include <sbi_utils/irqchip/plic.h>
#include <sbi_utils/serial/uart8250.h>
#include <sbi_utils/timer/aclint_mtimer.h>

#define PLATFORM_UART_ADDR				0x10000000
#define PLATFORM_UART_FREQ				100000000
#define PLATFORM_UART_BAUDRATE			9600
//#define PLATFORM_UART_REG_SHIFT			2
//#define PLATFORM_UART_REG_WIDTH			4
//#define PLATFORM_UART_REG_OFFSET		0
#define PLATFORM_PLIC_ADDR				0xc000000
#define PLATFORM_PLIC_NUM_SOURCES		2
#define PLATFORM_HART_COUNT				2
#define PLATFORM_CLINT_ADDR				0x2000000
#define PLATFORM_ACLINT_MTIMER_FREQ		1000000
#define PLATFORM_CLINT_MSWI_OFFSET              0
#define PLATFORM_CLINT_MTIMER_OFFSET            0x10
#define PLATFORM_CLINT_MTIMECMP_OFFSET          0x8
#define PLATFORM_ACLINT_MSWI_ADDR		(PLATFORM_CLINT_ADDR + PLATFORM_CLINT_MSWI_OFFSET)
#define PLATFORM_ACLINT_MTIMER_ADDR		(PLATFORM_CLINT_ADDR + PLATFORM_CLINT_MTIMER_OFFSET)

static struct plic_data plic = {
	.addr = PLATFORM_PLIC_ADDR,
	.num_src = PLATFORM_PLIC_NUM_SOURCES,
};

static struct aclint_mswi_data mswi = {
	.addr = PLATFORM_ACLINT_MSWI_ADDR,
	.size = 0x10,
	.first_hartid = 0,
	.hart_count = PLATFORM_HART_COUNT,
};

static struct aclint_mtimer_data mtimer = {
	.mtime_freq = PLATFORM_ACLINT_MTIMER_FREQ,
	.mtime_addr = PLATFORM_ACLINT_MTIMER_ADDR,
	.mtime_size = ACLINT_DEFAULT_MTIME_SIZE,
	.mtimecmp_addr = PLATFORM_ACLINT_MTIMER_ADDR + PLATFORM_CLINT_MTIMECMP_OFFSET,
	.mtimecmp_size = ACLINT_DEFAULT_MTIMECMP_SIZE,
	.first_hartid = 0,
	.hart_count = PLATFORM_HART_COUNT,
	.has_64bit_mmio = FALSE,
	.has_shared_mtime = TRUE
};

/*
 * PLATFORM platform early initialization.
 */
static int platform_early_init(bool cold_boot)
{
	/* For now nothing to do. */
	return 0;
}

/*
 * PLATFORM platform final initialization.
 */
static int platform_final_init(bool cold_boot)
{
	return 0;
}

/*
 * Initialize the PLATFORM console.
 */
static int platform_console_init(void)
{
	return xlnx_uartlite_init(PLATFORM_UART_ADDR);
}

static int plic_platform_warm_irqchip_init(int m_cntx_id, int s_cntx_id)
{
	int ret;

	/* By default, enable all IRQs for M-mode of target HART */
	if (m_cntx_id > -1) {
		ret = plic_context_init(&plic, m_cntx_id, true, 0x1);
		if (ret)
			return ret;
	}

	/* Enable all IRQs for S-mode of target HART */
	if (s_cntx_id > -1) {
		ret = plic_context_init(&plic, s_cntx_id, true, 0x0);
		if (ret)
			return ret;
	}

	return 0;
}

/*
 * Initialize the PLATFORM interrupt controller for current HART.
 */
static int platform_irqchip_init(bool cold_boot)
{
	u32 hartid = current_hartid();
	int ret;

	if (cold_boot) {
		ret = plic_cold_irqchip_init(&plic);
		if (ret)
			return ret;
	}
	return plic_platform_warm_irqchip_init(2 * hartid, 2 * hartid + 1);
}

/*
 * Initialize IPI for current HART.
 */
static int platform_ipi_init(bool cold_boot)
{
	int ret;

	if (cold_boot) {
		ret = aclint_mswi_cold_init(&mswi);
		if (ret)
			return ret;
	}

	return aclint_mswi_warm_init();
}

/*
 * Initialize PLATFORM timer for current HART.
 */
static int platform_timer_init(bool cold_boot)
{
	int ret;

	if (cold_boot) {
		ret = aclint_mtimer_cold_init(&mtimer, NULL);
		if (ret)
			return ret;
	}

	return aclint_mtimer_warm_init();
}

/*
 * Platform descriptor.
 */
const struct sbi_platform_operations platform_ops = {
	.early_init = platform_early_init,
	.final_init = platform_final_init,
	.console_init = platform_console_init,
	.irqchip_init = platform_irqchip_init,
	.ipi_init = platform_ipi_init,
	.timer_init = platform_timer_init,
};

const struct sbi_platform platform = {
	.opensbi_version = OPENSBI_VERSION,
	.platform_version = SBI_PLATFORM_VERSION(0x0, 0x01),
	.name = "zerocore_dual",
	.features = SBI_PLATFORM_DEFAULT_FEATURES,
	.hart_count = PLATFORM_HART_COUNT,
	.hart_stack_size = SBI_PLATFORM_DEFAULT_HART_STACK_SIZE,
	.platform_ops_addr = (unsigned long)&platform_ops
};
