#ifndef INCLUDE_PICO_DEV_LITEX_H
#define INCLUDE_PICO_DEV_LITEX_H

#include <pico_config.h>
#include <pico_device.h>

void pico_litex_destroy(struct pico_device *loop);
struct pico_device *pico_litex_create();

void pico_litex_handle_irq(void);

#endif

