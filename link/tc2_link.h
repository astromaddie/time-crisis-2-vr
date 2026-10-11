#ifndef TC2_LINK_H
#define TC2_LINK_H
#include <stdint.h>
int tc2_link_enabled(void);
int tc2_link_side(void);
uint16_t tc2_link_read(unsigned k, uint16_t *regs);
void tc2_link_write(unsigned k, uint16_t value, uint16_t *regs);
void tc2_link_poll(void);
void tc2_link_vblank(uint32_t frame);
void tc2_link_report(void);
void tc2_link_observe(uint32_t pc);
#endif
