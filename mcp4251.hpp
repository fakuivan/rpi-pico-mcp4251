#pragma once
#include <stdint.h>
#include "./spi.hpp"
#include <optional>

// These macros allow us to use enum classes as regular enums,
// except they're scoped and we can specify the underlying type

// BEGIN ugly macros

#define ARITH_BIN_OPERATOR(TYPE,ARITH_TYPE,OP) \
  template<typename T> \
  constexpr inline ARITH_TYPE operator OP(const TYPE a, const T b) { \
    return static_cast<ARITH_TYPE>(static_cast<ARITH_TYPE>(a) OP static_cast<ARITH_TYPE>(b)); \
  } \
  constexpr inline TYPE operator OP(const TYPE a, const TYPE b) { \
    return static_cast<TYPE>(static_cast<ARITH_TYPE>(a) OP static_cast<ARITH_TYPE>(b)); \
  }

#define ARITH_IBIN_OPERATOR(TYPE,OP) \
  constexpr inline TYPE& operator OP##=(TYPE& a, const TYPE& b) { \
    return a = a OP b; \
  }

#define FLAG_ENUM_CLS(NAME,ARITH_TYPE) \
  enum class NAME : ARITH_TYPE; \
  ARITH_BIN_OPERATOR(NAME,ARITH_TYPE,|) \
  ARITH_BIN_OPERATOR(NAME,ARITH_TYPE,&) \
  ARITH_IBIN_OPERATOR(NAME,|) \
  enum class NAME : ARITH_TYPE
  // Followed by body

// END ugly macros

namespace mcp5251 {

constexpr uint8_t dummy_data = 0xff;
constexpr uint16_t data_mask = 0x1ff;

FLAG_ENUM_CLS(registers, uint8_t) {
    wiper_0 = 0b00000000,
    wiper_1 = 0b00010000,
    tcon    = 0b01000000,
    sreg    = 0b01010000
};

FLAG_ENUM_CLS(commands, uint8_t) {
    write     = 0b00000000,
    increment = 0b00000100,
    decrement = 0b00001000,
    read      = 0b00001100
};

// bit fields are kind of garbage, but imho using shifts and masks
// for this would be far more tedious

// TODO: Add some sort of assertion to check for bitfield order,
//       first bitfield members should be lsb

union tcon_register {
    uint8_t bits;
    struct {
        bool terminal_b_connected : 1;
        bool terminal_wiper_connected : 1;
        bool terminal_a_connected : 1;
        bool pot_started : 1;
        uint8_t : 4;
    } pot0;
    struct {
        uint8_t : 4;
        bool terminal_b_connected : 1;
        bool terminal_wiper_connected : 1;
        bool terminal_a_connected : 1;
        bool pot_started : 1;
    } pot1;
    tcon_register(uint16_t reg) : bits((uint8_t)(reg & data_mask)) {}
};

static_assert((sizeof(tcon_register) == 1U));

union status_register {
    uint16_t bits;
    struct {
        bool : 1;
        bool shutdown : 1;
        uint8_t : 7;
    } data;
    status_register(uint16_t reg) : bits(reg) {}
};

union long_command_response {
    uint16_t bits;
    struct {
        uint16_t data : 9;
        bool success : 1;
        uint8_t : 6;
    } data;
    long_command_response(uint16_t res) : bits(res) {}
};

union short_command_response {
    uint8_t bits;
    struct {
        uint8_t : 1;
        bool success : 1;
        uint8_t : 6;
    } data;
    short_command_response(uint8_t res) : bits(res) {}
};

uint16_t concat_bits(const uint8_t msb, const uint8_t lsb) {
    return ((uint16_t)lsb) | (((uint16_t)msb) << 8);
}

std::tuple<uint8_t, uint8_t> split_bits(const uint16_t number) {
    return {(number >> 8) & 0xff, number & 0xff};
}

bool send_short_command(spi_inst *inst, uint8_t command) {
    const short_command_response res = 
        spi::write_read_blocking(inst, command);
    return res.data.success;
}

bool wiper_increment(spi_inst_t *inst, bool pot_1) {
    const uint8_t command = 
        commands::increment | (pot_1 ? registers::wiper_1 : registers::wiper_0);
    return send_short_command(inst, command);
}

bool wiper_decrement(spi_inst_t *inst, bool pot_1) {
    const uint8_t command = 
        commands::decrement | (pot_1 ? registers::wiper_1 : registers::wiper_0);
    return send_short_command(inst, command);
}

std::optional<uint16_t> send_long_command(
    spi_inst_t *inst,
    const uint8_t msb,
    const uint8_t lsb)
{
    const auto msb_response = spi::write_read_blocking(inst, msb);
    const auto lsb_response = spi::write_read_blocking(inst, lsb);
    volatile uint16_t response = concat_bits(msb_response, lsb_response);
    const long_command_response result = response;
    volatile uint16_t data = result.data.data;
    if (!result.data.success) {
        return {};
    }
    return {result.data.data};
}

std::optional<uint16_t> send_long_command(
    spi_inst_t *inst,
    const uint16_t command)
{
    const auto [msb, lsb] = split_bits(command);
    return send_long_command(inst, msb, lsb);
}

std::optional<uint16_t> read_register(
    spi_inst_t *inst, 
    const registers reg)
{
    return send_long_command(
        inst,
        concat_bits(commands::read | reg, dummy_data)
    );
}

bool write_register(
    spi_inst_t *inst,
    const registers reg,
    const uint16_t data)
{
    const auto result = send_long_command(inst,
        // Some of these cast might be unnecesary
        (uint16_t)(((uint16_t)(commands::write | reg) << 8) | (uint16_t)(data & data_mask)));
    return result.has_value();
}

template<typename Func>
bool modify_register(spi_inst_t *inst, registers reg, Func f) {
    const auto response = read_register(inst, reg);
    if (!response) {
        return false;
    }
    const uint16_t modified = f(*response);
    if (!write_register(inst, reg, modified)) {
        return false;
    }
    return true;
}

}