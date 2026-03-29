// =============================================================================
//  Exercise 01 — Parts Counter
// =============================================================================
//
//  Virtual hardware:
//    SW 0        →  io.digital_read(0)        Inductive sensor input
//    Display     →  io.write_reg(6, …)        LCD debug (see README for format)
//                   io.write_reg(7, …)
//
//  Goal:
//    Count every part that passes the sensor and show the total on the display.
//
//  Read README.md before starting.
// =============================================================================

#include <trac_fw_io.hpp>
#include <cstdint>
#include <cstdio>   
#include <cstring>  
#include <atomic> 

// Global atomic variables for thread safety 
// Variáveis atômicas globais para segurança
std::atomic<uint32_t> count{ 0 };
std::atomic<bool> piece_detected{ false };

// Debounce constants 
// Constantes de debouncing
const uint32_t DEBOUNCE_RISING_MS = 135;
const uint32_t DEBOUNCE_FALLING_MS = 125;

std::atomic<uint32_t> last_rising_time{ 0 };
std::atomic<uint32_t> last_falling_time{ 0 };

trac_fw_io_t io;

// Function to update the display
// Função para atualizar o display
void update_display(uint32_t current_count) {
    char buf[9] = {};
    std::snprintf(buf, sizeof(buf), "%8u", current_count);
    uint32_t r6, r7;
    std::memcpy(&r6, buf + 0, 4);
    std::memcpy(&r7, buf + 4, 4);
    io.write_reg(6, r6);
    io.write_reg(7, r7);
}

// Sensor Interrupt Service Routine 
// Rotina de Serviço de Interrupção do Sensor
void sensor_callback() {
    uint32_t current_time = io.millis();
    bool sensor_state = io.digital_read(0);

    // RISING EDGE: Piece enters 
    // BORDA DE SUBIDA: Peça entra
    if (sensor_state == true) {
        if ((current_time - last_rising_time.load()) > DEBOUNCE_RISING_MS) {
            piece_detected.store(true);
            last_rising_time.store(current_time);
        }
    }
    else {
        // FALLING EDGE: Piece leaves 
        // BORDA DE DESCIDA: Peça sai
        if ((current_time - last_falling_time.load()) > DEBOUNCE_FALLING_MS) {
            if (piece_detected.load() == true) {
                count++;
                piece_detected.store(false);
                update_display(count.load());
            }
            last_falling_time.store(current_time);
        }
    }
}

int main() {
    // Initial display update 
    // Atualização inicial do display
    update_display(0);

    // Attach interrupt to pin 0 
    // Configura interrupção no pino 0
    io.attach_interrupt(0, sensor_callback, InterruptMode::CHANGE);

    // Timing control variables for main loop 
    // Variáveis de controle de tempo para o loop principal
    uint32_t last_heartbeat_ms = 0;
    const uint32_t HEARTBEAT_INTERVAL = 500; // 500ms for status check / 500ms para check de status

    while (true) {
        uint32_t current_ms = io.millis();

        // Non-blocking task execution 
        // Execução de tarefas não-bloqueantes
        if (current_ms - last_heartbeat_ms >= HEARTBEAT_INTERVAL) {
            last_heartbeat_ms = current_ms;
        }
    }

    return 0;
}