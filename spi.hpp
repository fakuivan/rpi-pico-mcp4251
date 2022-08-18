#pragma once
#include <stdint.h>
#include "hardware/spi.h"
#include <tuple>

namespace spi {

struct ChipSelectGuard {
    const uint8_t pin;
    const bool active_low;
    ChipSelectGuard(const uint8_t pin, const bool active_low)
    : pin(pin), active_low(active_low) { 
        gpio_put(pin, !active_low);
    }
    ~ChipSelectGuard() {
        gpio_put(pin, this->active_low);
    }
};

struct ChipSelect {
    const uint8_t pin;
    const bool active_low;
    ChipSelect(const uint8_t pin, const bool active_low)
    : pin(pin), active_low(active_low) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, active_low);
    }
    [[nodiscard]] auto get_guard() const {
        return ChipSelectGuard(pin, active_low);
    }
    void select() const {
        gpio_put(this->pin, !this->active_low);
    }
    void deselect() const {
        gpio_put(this->pin, this->active_low);
    }
    template<typename Func>
    inline void with_active(Func f) const {
        this->select();
        f();
        this->deselect();
    }
};

template<size_t length>
int write_read_blocking(spi_inst_t *inst,
    const uint8_t (&read_buffer)[length],
    uint8_t (&write_buffer)[length])
{
    return spi_write_read_blocking(inst, &read_buffer[0], &write_buffer[0], length);
}

uint8_t write_read_blocking(spi_inst_t *inst, const uint8_t msg) {
    uint8_t response[1];
    write_read_blocking(inst, {msg}, response);
    return response[0];
}

}