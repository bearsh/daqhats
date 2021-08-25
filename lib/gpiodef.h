#ifndef _GPIODEF_H
#define _GPIODEF_H

#ifdef GENERIC_HAT
// dummy defines
#ifndef ADDR0_GPIO
#define ADDR0_GPIO              0,0
#endif
#ifndef ADDR1_GPIO
#define ADDR1_GPIO              0,1
#endif
#ifndef ADDR2_GPIO
#define ADDR2_GPIO              0,2
#endif
#ifndef RESET_GPIO
#define RESET_GPIO              0,3
#endif
#ifndef IRQ_GPIO
#define IRQ_GPIO                0,4
#endif
#ifndef IRQ_IOEXP_GPIO
#define IRQ_IOEXP_GPIO          0,5
#endif
#else
#ifndef USE_LIBGPIOD
#define ADDR0_GPIO              12
#define ADDR1_GPIO              13
#define ADDR2_GPIO              26
#define RESET_GPIO              16
#define IRQ_GPIO                20
#define IRQ_IOEXP_GPIO          21
#else
#define ADDR0_GPIO              0,12
#define ADDR1_GPIO              0,13
#define ADDR2_GPIO              0,26
#define RESET_GPIO              0,16
#define IRQ_GPIO                0,20
#define IRQ_IOEXP_GPIO          0,21
#endif // USE_LIBGPIOD
#endif // GENERIC_HAT

#endif // _GPIODEF_H
