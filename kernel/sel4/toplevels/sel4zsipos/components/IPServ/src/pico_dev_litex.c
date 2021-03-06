#include <camkes.h>

#include <pico_device.h>
#include <pico_stack.h>

#include "pico_dev_litex.h"

#include <litex.h>

static const unsigned char mac[6] = {0x10, 0xe2, 0xd5, 0x00, 0x00, 0x00}; //todo


#define LITEX_COUNTER_RESET			100
#define LITEX_COUNTER_READER_READY	5

static volatile void *macadr;
static volatile void *phyadr;
static volatile void *rxbadr;
static volatile void *txbadr;

static int tx_slot;

static struct pico_device *litex;

#define USE_IRQ

static void pico_litex_recv()
{
	int error = 0;
	unsigned char rx_slot;
	int len;

#ifdef USE_IRQ
	error = pico_stack_lock();
#endif

	rx_slot = litex_csr_readb(macadr + LITEX_ETHMAC_SRAM_WRITER_SLOT_REG);
	len = litex_csr_readl(macadr + LITEX_ETHMAC_SRAM_WRITER_LENGTH_REG);

    pico_stack_recv(litex, (void*)rxbadr + rx_slot * LITEX_ETHMAC_SLOT_SIZE, (uint32_t)len);

#ifdef USE_IRQ
    error = pico_stack_unlock();
#endif
}

static int litex_poll_device()
{
	int ret = 0;
	unsigned char reg;

	reg = litex_csr_readb(macadr + LITEX_ETHMAC_SRAM_READER_EV_PENDING_REG);
	if (reg) {
		// packet transmitted.
		litex_csr_writeb(1, macadr + LITEX_ETHMAC_SRAM_READER_EV_PENDING_REG);
	}
	reg = litex_csr_readb(macadr + LITEX_ETHMAC_SRAM_WRITER_EV_PENDING_REG);
	if (reg) {
		ret = 1;
		pico_litex_recv();
		litex_csr_writeb(1, macadr + LITEX_ETHMAC_SRAM_WRITER_EV_PENDING_REG);
	}
	return ret;
}

void pico_litex_handle_irq()
{
#ifdef USE_IRQ
	litex_poll_device();
#endif
}

static int pico_litex_send(struct pico_device *dev, void *buf, int len)
{
	int i, timeout = 1;

    IGNORE_PARAMETER(dev);

	if (len > LITEX_ETHMAC_SLOT_SIZE) {
		dbg("WARNING: packet too big. dropped.\n");
		return 0;
	}

	memcpy((void*)txbadr + tx_slot * LITEX_ETHMAC_SLOT_SIZE, buf, len);
	litex_csr_writeb(tx_slot, macadr + LITEX_ETHMAC_SRAM_READER_SLOT_REG);
	litex_csr_writew(len, macadr + LITEX_ETHMAC_SRAM_READER_LENGTH_REG);

	for (i = 0; i < LITEX_COUNTER_READER_READY; i++) {
		unsigned char val;

		val = litex_csr_readb(macadr + LITEX_ETHMAC_SRAM_READER_READY_REG);
		if (val) {
			timeout = 0;
			break;
		}
		seL4_Yield();
	}
	if (timeout) {
		dbg("WARNING: tx slow. packet dropped.\n");
		return 0;
	}

	litex_csr_writeb(1, macadr + LITEX_ETHMAC_SRAM_READER_START_REG);

	tx_slot = (tx_slot + 1) % LITEX_ETHMAC_TX_SLOTS;

	return len;
}

#ifndef USE_IRQ
static int pico_litex_poll(struct pico_device *dev, int loop_score)
{
    if (loop_score <= 0)
        return 0;

    if (litex_poll_device())
    	loop_score--;

    return loop_score;
}
#endif

static int check_hw_config()
{
	// check that buffers are page aligned
	if (LITEX_ETHMAC_RX_SLOTS != 2 || LITEX_ETHMAC1_RX_SLOTS != 2) {
		dbg("ERROR: litex eth rx slots must be 2\n");
		return 0;
	}
	if (LITEX_ETHMAC_TX_SLOTS != 2 || LITEX_ETHMAC1_TX_SLOTS != 2) {
		dbg("ERROR: litex eth tx slots must be 2\n");
		return 0;
	}
	if (LITEX_ETHMAC_SLOT_SIZE != 2048 || LITEX_ETHMAC1_SLOT_SIZE != 2048) {
		dbg("ERROR: litex eth slot_size must be 2048\n");
		return 0;
	}
	return 1;
}

static void reset_delay()
{
	int i;

	for (i = 0; i < LITEX_COUNTER_RESET; i++)
		seL4_Yield();
}

static int pico_litex_init()
{
	if (!check_hw_config())
		return 0;

	tx_slot = 0;

	// reset hardware
	litex_csr_writeb(0, phyadr + LITEX_ETHPHY_CRG_RESET_REG);
	reset_delay();
	litex_csr_writeb(1, phyadr + LITEX_ETHPHY_CRG_RESET_REG);
	reset_delay();
	litex_csr_writeb(0, phyadr + LITEX_ETHPHY_CRG_RESET_REG);

	// clear interrupts
	litex_csr_writeb(1, macadr + LITEX_ETHMAC_SRAM_READER_EV_PENDING_REG);
	litex_csr_writeb(1, macadr + LITEX_ETHMAC_SRAM_WRITER_EV_PENDING_REG);

	// enable interrupts
	litex_csr_writeb(1, macadr + LITEX_ETHMAC_SRAM_READER_EV_ENABLE_REG);
	litex_csr_writeb(1, macadr + LITEX_ETHMAC_SRAM_WRITER_EV_ENABLE_REG);

	return 1;
}

struct pico_device *pico_litex_create()
{
    litex = PICO_ZALLOC(sizeof(struct pico_device));

    if (!litex) {
    	dbg("ERROR: can not allocate pico_device!");
        return NULL;
    }

    macadr = eth_reg0;
    phyadr = eth_reg1;
    rxbadr = eth_reg2;
    txbadr = eth_reg3;

    if (pico_device_init(litex, ifname, mac) || !pico_litex_init()) {
        dbg("litex init failed.\n");
        pico_device_destroy(litex);
        return NULL;
    }

    litex->send = pico_litex_send;
#ifndef USE_IRQ
    litex->poll = pico_litex_poll;
#endif

    return litex;
}

