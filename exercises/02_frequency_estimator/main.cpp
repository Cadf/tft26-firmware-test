// =============================================================================
//  Challenge 02 — Frequency Estimator
// =============================================================================
//
//  Virtual hardware:
//    ADC Ch 0  →  io.analog_read(0)      Process sensor signal (0–4095)
//    OUT reg 3 →  io.write_reg(3, …)     Frequency estimate in centiHz
//                                        e.g. write_reg(3, 4733) = 47.33 Hz
//
//  Goal:
//    Measure the frequency of the signal on ADC channel 0 and publish your
//    estimate continuously via register 3.
//
//  Read README.md before starting.
// =============================================================================

#include <trac_fw_io.hpp>
#include <cstdint>
#include <cstdio>

// --- Fast Convergence Config / Configuração de Convergência Rápida ---
const uint16_t HYSTERESIS = 65;
const uint16_t SIGNAL_MEAN = 2048;

// Increased Alpha for faster tracking (< 500ms convergence)
// Alpha aumentado para rastreamento mais rápido (convergência < 500ms)
const float EMA_ALPHA = 0.7f;

trac_fw_io_t io;

int main() {
    uint32_t t_start = io.millis();
    uint32_t last_loop_ms = io.millis();

    bool last_state = false;
    float filtered_freq_hz = 0.0f;

    // Small buffer for quick smoothing without lag
    // Buffer pequeno para suavização rápida sem atraso (lag)
    const int MA_SIZE = 3;
    uint16_t ma_buffer[MA_SIZE] = { 2048, 2048, 2048 };
    int ma_idx = 0;

    while (true) {
        uint32_t current_ms = io.millis();

        // 1ms Fixed-rate task / Tarefa de taxa fixa de 1ms
        if (current_ms - last_loop_ms >= 1) {
            last_loop_ms = current_ms;

            // Fast filtering / Filtragem rápida
            uint32_t sum_os = 0;
            for (int i = 0; i < 30; i++) sum_os += io.analog_read(0);

            ma_buffer[ma_idx] = static_cast<uint16_t>(sum_os / 30);
            ma_idx = (ma_idx + 1) % MA_SIZE;

            uint32_t ma_sum = 0;
            for (int i = 0; i < MA_SIZE; i++) ma_sum += ma_buffer[i];
            uint16_t final_val = ma_sum / MA_SIZE;

            // Schmitt Trigger
            bool current_state;
            if (final_val > (SIGNAL_MEAN + HYSTERESIS))      current_state = true;
            else if (final_val < (SIGNAL_MEAN - HYSTERESIS)) current_state = false;
            else                                             current_state = last_state;

            // Edge detection / Detecção de borda
            if (current_state && !last_state) {
                uint32_t period_ms = current_ms - t_start;

                // Frequency check (6-9Hz range) / Verificação de frequência
                if (period_ms > 90 && period_ms < 200) {
                    float instant_freq_hz = 1000.0f / static_cast<float>(period_ms);

                    // Reset filter if change is too abrupt (Fast Step Response)
                    // Reinicia o filtro se a mudança for muito abrupta (Resposta ao Degrau Rápida)
                    float error = instant_freq_hz - filtered_freq_hz;
                    if (error > 2.0f || error < -2.0f) {
                        filtered_freq_hz = instant_freq_hz;
                    }
                    else {
                        filtered_freq_hz = (instant_freq_hz * EMA_ALPHA) + (filtered_freq_hz * (1.0f - EMA_ALPHA));
                    }

                    // Write result * 100 / Escreve resultado * 100
                    uint32_t output_val = static_cast<uint32_t>(filtered_freq_hz * 100.0f + 0.5f);
                    io.write_reg(3, output_val);
                }
                t_start = current_ms;
            }
            last_state = current_state;
        }
    }
    return 0;
}