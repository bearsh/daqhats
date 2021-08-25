/*
*   author Martin Gysel (bearsh)
*   brief This file contains a wrapper over gpiod compatible to the lightweight rpi-only pin control functions.
*
*   date 25 Aug 2021
*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <glob.h>
#include "gpiod.h"
#include "gpio.h"

#define GPIOCHIP_BASE       "/dev/gpiochip"

#define ms_to_timespec(ms)  (struct timespec){ .tv_sec = ms/1000, .tv_nsec = (ms%1000)*1000000 }

struct chip_data {
    struct gpiod_chip *chip;
    pthread_t *line_threads;
    bool *line_signals;
    bool *line_active;
};

static struct chip_data *chip_data = NULL;
static int nb_of_chips = -1;

// allocate space according to available gpiochips in /dev
static void alloc_chip_data(void)
{
    glob_t g;
    if (glob(GPIOCHIP_BASE "*", GLOB_NOSORT, NULL, &g)) {
        return;
    }
    nb_of_chips = (int)g.gl_pathc;
    chip_data = calloc(sizeof(struct chip_data), nb_of_chips);
}


static struct gpiod_line* get_line(int chip, int pin)
{
    if (nb_of_chips == -1) {
        alloc_chip_data();
    }
    if (chip >= nb_of_chips) {
        // TODO: should we realloc chip_data?
        return NULL;
    }
    if (chip_data[chip].chip) {
        return gpiod_chip_get_line(chip_data[chip].chip, pin);
    }
    struct gpiod_chip* c = gpiod_chip_open_by_number(chip);
    if (!c) {
        return NULL;
    }
    chip_data[chip].chip = c;
    return gpiod_chip_get_line(c, pin);
}

static void alloc_line_data(struct chip_data *cd) {
    unsigned nb = gpiod_chip_num_lines(cd->chip);
    if (cd->line_threads == NULL) {
        cd->line_threads = calloc(sizeof(pthread_t), nb);
    }
    if (cd->line_signals == NULL) {
        cd->line_signals = calloc(sizeof(bool), nb);
    }
    if (cd->line_active == NULL) {
        cd->line_active = calloc(sizeof(bool), nb);
    }
}


void gpio_dir(int chip, int pin, int dir)
{
    struct gpiod_line* line = get_line(chip, pin);

    if (line) {
        if (dir == 0) {
            // Set pin to output.
            gpiod_line_set_direction_output(line, 0);
        } else {
            // Set pin to input.
            gpiod_line_set_direction_input(line);
        }
    }
}

void gpio_write(int chip, int pin, int val)
{
    struct gpiod_line* line = get_line(chip, pin);

    if (line) {
        gpiod_line_set_value(line, val ? 1 : 0);
    }
}

int gpio_status(int chip, int pin)
{
    struct gpiod_line* line = get_line(chip, pin);

    if (line) {
        return gpiod_line_get_value(line);
    }
    return -1;
}


struct thread_arg {
    struct chip_data* cd;
    struct gpiod_line* line;
    void (*callback)(void*);
    void* user_data;
};

static void *gpio_interrupt_thread(void* arg)
{
    const struct timespec ts = (struct timespec){ .tv_nsec = 10 * 1000000 };

    struct chip_data* cd = ((struct thread_arg*)arg)->cd;
    struct gpiod_line* line = ((struct thread_arg*)arg)->line;
    void (*callback)(void*) = ((struct thread_arg*)arg)->callback;
    void* user_data = ((struct thread_arg*)arg)->user_data;
    free(arg);

    unsigned pin = gpiod_line_offset(line);

    while (!cd->line_signals[pin]) {
        if (gpiod_line_event_wait(line, &ts) > 0) {
            // call the callback
            callback(user_data);
        }
    }

    return NULL;
}


int gpio_interrupt_callback(int chip, int pin, int mode, void (*function)(void*),
    void* data)
{
    struct gpiod_line* line = get_line(chip, pin);
    if (!line) {
        return -1;
    }
    alloc_line_data(&chip_data[chip]);

    switch (mode) {
    case 0: // falling
        gpiod_line_request_falling_edge_events(line, "daqhats");
        break;
    case 1: // rising
        gpiod_line_request_rising_edge_events(line, "daqhats");
        break;
    case 2: // both
        gpiod_line_request_both_edges_events(line, "daqhats");
        break;
    default:    // disable
        gpiod_line_release(line);
        if (chip_data[chip].line_active[pin]) {
            // there is already an interrupt thread on this pin so signal the
            // thread to end and wait for it
            chip_data[chip].line_signals[pin] = true;
            pthread_join(chip_data[chip].line_threads[pin], NULL);
            chip_data[chip].line_active[pin] = false;
        }
        return 0;
    }

    // only get here if we are configuring an edge
    // check for an existing thread
    if (chip_data[chip].line_active[pin]) {
        // there is already an interrupt thread on this pin so signal the
        // thread to end and wait for it
        chip_data[chip].line_signals[pin] = true;
        pthread_join(chip_data[chip].line_threads[pin], NULL);
        chip_data[chip].line_active[pin] = false;
    }

    struct thread_arg* thread_arg = malloc(sizeof *thread_arg);
    thread_arg->callback = function;
    thread_arg->user_data = data;
    thread_arg->line = line;
    thread_arg->cd = &chip_data[chip];

    chip_data[chip].line_signals[pin] = false;

    // start the interrupt thread
    return pthread_create(&chip_data[chip].line_threads[pin], NULL, gpio_interrupt_thread, thread_arg);
}

int gpio_wait_for_low(int chip, int pin, int timeout)
{
    struct gpiod_line* line = get_line(chip, pin);

    if (!line) {
        return -1;
    }

    if (gpiod_line_get_value(line) == 0) {
        return 1;
    }

    struct timespec ts = ms_to_timespec(timeout);;
    gpiod_line_request_falling_edge_events(line, "daqhats");
    return gpiod_line_event_wait(line, &ts);
}
