// =============================================================================
//  Exercise 03 — I2C Sensors (Bit-bang)
// =============================================================================
//
//  Virtual hardware:
//    P8 (SCL)  →  io.digital_write(8, …) / io.digital_read(8)
//    P9 (SDA)  →  io.digital_write(9, …) / io.digital_read(9)
//
//  PART 1 — TMP64 temperature sensor at I2C address 0x48
//    Register 0x0F  WHO_AM_I   — 1 byte  (expected: 0xA5)
//    Register 0x00  TEMP_RAW   — 4 bytes, big-endian int32_t, milli-Celsius
//
//  PART 2 — Unknown humidity sensor (same register layout, address unknown)
//    Register 0x0F  WHO_AM_I   — 1 byte
//    Register 0x00  HUM_RAW    — 4 bytes, big-endian int32_t, milli-percent
//
//  Goal (Part 1):
//    1. Implement an I2C master via bit-bang on P8/P9.
//    2. Read WHO_AM_I from TMP64 and confirm the sensor is present.
//    3. Read TEMP_RAW in a loop and print the temperature in °C every second.
//    4. Update display registers 6–7 with the formatted temperature string.
//
//  Goal (Part 2):
//    5. Scan the I2C bus (addresses 0x08–0x77) and print every responding address.
//    6. For each unknown device found, read its WHO_AM_I and print it.
//    7. Add the humidity sensor to the 1 Hz loop: read HUM_RAW and print %RH.
//
//  Read README.md before starting.
// =============================================================================

#include <trac_fw_io.hpp>
#include <cstdint>
#include <cstdio>
#include <cstring>

const int SCL = 8;
const int SDA = 9;
const uint8_t ADDR_TMP64 = 0x48;
const uint8_t REG_WHO_AM_I = 0x0F;

trac_fw_io_t io;

// Micro-pause for I2C bit-banging
// Micro-pausa para bit-banging I2C
void i2c_bus_pause() { for (volatile int i = 0; i < 50; i++); }

// I2C Protocol Functions 
// Funções do Protocolo I2C

void i2c_start() {
    io.digital_write(SDA, 1); io.digital_write(SCL, 1); i2c_bus_pause();
    io.digital_write(SDA, 0); i2c_bus_pause();
    io.digital_write(SCL, 0); i2c_bus_pause();
}

void i2c_stop() {
    io.digital_write(SDA, 0); i2c_bus_pause();
    io.digital_write(SCL, 1); i2c_bus_pause();
    io.digital_write(SDA, 1); i2c_bus_pause();
}

bool i2c_write(uint8_t byte) {
    for (int i = 0; i < 8; i++) {
        io.digital_write(SDA, (byte & 0x80) ? 1 : 0);
        i2c_bus_pause();
        io.digital_write(SCL, 1); i2c_bus_pause();
        io.digital_write(SCL, 0); i2c_bus_pause();
        byte <<= 1;
    }
    io.digital_write(SDA, 1); i2c_bus_pause(); // Release SDA for ACK / Libera SDA para ACK
    io.digital_write(SCL, 1); i2c_bus_pause();
    bool ack = (io.digital_read(SDA) == 0);
    io.digital_write(SCL, 0); i2c_bus_pause();
    return ack;
}

uint8_t i2c_read(bool send_ack) {
    uint8_t res = 0;
    io.digital_write(SDA, 1); // Ensure SDA is input / Garante que SDA é entrada
    for (int i = 0; i < 8; i++) {
        io.digital_write(SCL, 1); i2c_bus_pause();
        if (io.digital_read(SDA)) res |= (1 << (7 - i));
        io.digital_write(SCL, 0); i2c_bus_pause();
    }
    io.digital_write(SDA, send_ack ? 0 : 1); // Master ACK or NACK / ACK ou NACK do Mestre
    i2c_bus_pause();
    io.digital_write(SCL, 1); i2c_bus_pause();
    io.digital_write(SCL, 0); i2c_bus_pause();
    return res;
}

int main() {
    io.set_pullup(SCL, true);
    io.set_pullup(SDA, true);

    uint8_t addr_humd = 0;
    uint32_t last_read_ms = 0;
    bool system_initialized = false;

    while (true) {
        uint32_t current_ms = io.millis();

        // 1. STARTUP: Bus Scan + WHO_AM_I identification
        // 1. STARTUP: Varredura de Barramento + identificação WHO_AM_I
        if (!system_initialized) {
            std::printf("\n--- I2C STARTUP SCAN ---\n");
            for (uint8_t addr = 1; addr < 128; addr++) {
                i2c_start();
                if (i2c_write(addr << 1)) {
                    // Device found, now read its WHO_AM_I 
                    // Dispositivo encontrado, lê o WHO_AM_I
                    i2c_start();
                    i2c_write(addr << 1);
                    i2c_write(REG_WHO_AM_I);
                    i2c_start();
                    i2c_write((addr << 1) | 1);
                    uint8_t id = i2c_read(false);
                    i2c_stop();

                    std::printf("Device @ 0x%02X | WHO_AM_I: 0x%02X\n", addr, id);

                    // If not the fixed temp sensor, it's the humidity sensor
                    // Se não for o sensor de temp fixo, é o de umidade
                    if (addr != ADDR_TMP64) addr_humd = addr;
                }
                i2c_stop();
            }
            system_initialized = true;
            std::printf("--- Monitoring Active ---\n\n");
        }

        // 2. PERIODIC READ: Every 1s using millis() (No busy-waiting)
        // 2. LEITURA PERIÓDICA: A cada 1s usando millis() (Sem espera ocupada)
        if (current_ms - last_read_ms >= 1000) {

            // --- TEMPERATURE (TMP64 @ 0x48) ---
            i2c_start();
            if (i2c_write(ADDR_TMP64 << 1)) {
                i2c_write(0x00); // Point to temp register / Aponta para reg de temp
                i2c_start();
                i2c_write((ADDR_TMP64 << 1) | 1);
                uint32_t raw = 0;
                for (int i = 0; i < 4; i++) raw = (raw << 8) | i2c_read(i < 3);
                i2c_stop();

                float temp = (float)((int32_t)raw) / 1000.0f;
                std::printf("Temp: %.3f C\n", temp);

                // Update LCD Line 0 (Reg 6-7)
                // Atualiza Linha 0 do LCD (Reg 6-7)
                char b_t[9]; std::snprintf(b_t, 9, "%8.3f", temp);
                uint32_t r6, r7;
                std::memcpy(&r6, &b_t[0], 4); std::memcpy(&r7, &b_t[4], 4);
                io.write_reg(6, r6); io.write_reg(7, r7);
            }

            // --- HUMIDITY (Detected Address) / UMIDADE (Endereço Detectado) ---
            if (addr_humd != 0) {
                i2c_start();
                if (i2c_write(addr_humd << 1)) {
                    i2c_write(0x00);
                    i2c_start();
                    i2c_write((addr_humd << 1) | 1);
                    uint32_t raw = 0;
                    for (int i = 0; i < 4; i++) raw = (raw << 8) | i2c_read(i < 3);
                    i2c_stop();

                    float humd = (float)((int32_t)raw) / 1000.0f;
                    std::printf("Humd: %.3f %%\n", humd);

                    // Update LCD Line 1 (Reg 4-5) 
                    // Atualiza Linha 1 do LCD (Reg 4-5)
                    char b_h[9]; std::snprintf(b_h, 9, "%8.3f", humd);
                    uint32_t r4, r5;
                    std::memcpy(&r4, &b_h[0], 4); std::memcpy(&r5, &b_h[4], 4);
                    io.write_reg(4, r4); io.write_reg(5, r5);
                }
            }
            last_read_ms = current_ms; // Update last execution time / Atualiza tempo da última execução
        }
    }
    return 0;
}