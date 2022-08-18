#include <stdio.h>
#include "pico/stdlib.h"
#include "mcp4251.hpp"
#include "pico/binary_info.h"
#include <iostream>
#include <bitset>

const auto spi_port = spi0;
constexpr uint
    spi_rx = 16,
    spi_tx = 19,
    spi_clk = 18,
    spi_cs = 17;

bool test_tcon() {
    std::cout << "Testing tcon register..." << std::endl;
    std::cout << "Reading tcon register" << std::endl;
    const auto response = mcp5251::read_register(spi0, mcp5251::registers::tcon);
    if (!response.has_value()) {
        std::cout << "Failed to read tcon register" << std::endl;
        return false;
    }
    mcp5251::tcon_register tcon = *response;
    std::cout << "    tcon: " << std::bitset<8>(tcon.bits) << std::endl;

    // tcon.pot0.pot_started = false;
    // tcon.pot0.terminal_a_connected = true;
    tcon.pot0.terminal_wiper_connected = false;
    //tcon.pot0.terminal_b_connected = false;
    
    sleep_ms(1000);
    std::cout << "Writing this to tcon register" << std::endl;
    std::cout << "    tcon: " << std::bitset<8>(tcon.bits) << std::endl;

    if (!mcp5251::write_register(spi0, mcp5251::registers::tcon, tcon.bits)) {
        std::cout << "Failed to write tcon register" << std::endl;
        return false;
    }

    sleep_ms(1000);
    const auto response_2 = mcp5251::read_register(spi0, mcp5251::registers::tcon);
    if (!response_2.has_value()) {
        std::cout << "Failed to read back tcon register" << std::endl;
        return false;
    }
    mcp5251::tcon_register tcon_written = *response_2;
    std::cout << "Reading back tcon register" << std::endl;
    std::cout << "    tcon: " << std::bitset<8>(tcon_written.bits) << std::endl;

    return true;
}

bool test_status() {
    std::cout << "Testing status register" << std::endl;

    std::cout << "Reading status register" << std::endl;
    const auto response = mcp5251::read_register(spi0, mcp5251::registers::sreg);
    if (!response.has_value()) {
        std::cout << "Failed to read status register!" << std::endl;
        return false;
    }

    const mcp5251::status_register sreg = *response;
    std::cout << "    Chip is " << (sreg.data.shutdown ? "shutdown" : "up") << std::endl;
    std::cout << "    sreg: " << std::bitset<16>(sreg.bits) << std::endl;
    return true;  
}

bool test_increment() {
    std::cout << "Testing wiper increment" << std::endl;
    if (!mcp5251::write_register(spi_port, mcp5251::registers::wiper_0, 0)) {
        std::cout << "Failed to write wiper register!" << std::endl;
        return false;
    }

    for (int i = 0; i<256; i++) {
        if (i == 100) {
            const auto response = mcp5251::read_register(spi_port, mcp5251::registers::wiper_0);
            if (!response.has_value()) {
                std::cout << "Failed to write wiper register!" << std::endl;
                return false;
            }
            std::cout << "wiper_0 intermediate value: " << std::bitset<16>(*response) << std::endl;
        }

        if (!mcp5251::wiper_increment(spi_port, false)) {
            std::cout << "Failed to increment wiper register!" << std::endl;
            return false;
        }
        sleep_ms(10);
    }
    const auto data = mcp5251::read_register(spi_port, mcp5251::registers::wiper_0);
    if (!data.has_value()) {
        std::cout << "Failed to write wiper register!" << std::endl;
        return false;
    }
    std::cout << "wiper_0 final value: " << std::bitset<16>(*data) << std::endl;
    return true;
}

bool test_short_commands() {
    std::cout << "Testing short commands..." << std::endl;
    std::cout << "Incrementing" << std::endl;
    for (int i = 0; i<256; i++) {
        if (!mcp5251::wiper_increment(spi_port, false)) {
            std::cout << "Failed to increment wiper register!" << std::endl;
            return false;
        }
        sleep_ms(10);
    }
    std::cout << "Decrementing" << std::endl;
    for (int i = 0; i<256; i++) {
        if (!mcp5251::wiper_decrement(spi_port, false)) {
            std::cout << "Failed to decrement wiper register!" << std::endl;
            return false;
        }
        sleep_ms(10);
    }
    return true;
}

int main() {
    stdio_init_all();
    
    const auto cs = spi::ChipSelect(spi_cs, true);
    gpio_set_function(spi_clk, GPIO_FUNC_SPI);
    gpio_set_function(spi_rx, GPIO_FUNC_SPI);
    gpio_set_function(spi_tx, GPIO_FUNC_SPI);
    spi_init(spi_port, 5000 * 1000);

    sleep_ms(1000);
    cs.select();

    auto reset_if_failed = [&cs] (bool success) -> void {
        if (success) {
            return;
        }
        std::cout << "Test failed, resetting SPI interface on chip with CS pin" << std::endl;
        sleep_ms(100);
        cs.deselect();
        sleep_ms(100);
        cs.select();
    };

    while (true) {
        std::cout << "Sleeping for 1s" << std::endl;
        sleep_ms(1000);
        reset_if_failed(test_tcon());
        reset_if_failed(test_status());
        reset_if_failed(test_short_commands());
        reset_if_failed(test_increment());
    }
}