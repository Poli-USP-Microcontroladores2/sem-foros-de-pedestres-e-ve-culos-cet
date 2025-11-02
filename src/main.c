#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/sys/time_units.h>
#include <zephyr/logging/log.h>

// --- Logs ---
LOG_MODULE_REGISTER(LOG_INF_APP, LOG_LEVEL_INF);

// --- Configuração de LEDs via DeviceTree ---
#define LED_A_NODE DT_ALIAS(led0)  // LED verde
#define LED_B_NODE DT_ALIAS(led2)  // LED vermelho
#define BUTTON_NODE DT_NODELABEL(user_button_0)

static const struct gpio_dt_spec ledA = GPIO_DT_SPEC_GET(LED_A_NODE, gpios);
static const struct gpio_dt_spec ledB = GPIO_DT_SPEC_GET(LED_B_NODE, gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
static struct gpio_callback button_cb_data;

// --- Prioridades e tempos ---
#define PRIO_THREAD_A 7

#define BLINK_DURATION_MS 10
#define BLINK_INTERVAL_MS 200

//Interrupt
void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    LOG_INF("Toggle Botão");
    gpio_pin_toggle_dt(&ledB);
}

// ----------------------------------------------------
// THREAD A — Blink Repetitivo
// ----------------------------------------------------
void thread_A(void *p1, void *p2, void *p3)
{
    k_msleep(100); //Para dar tempo pro main thread printar
    int cyclecounter = 0;
    while(1)  
    {
        unsigned long startupcycle = k_cycle_get_32();
        gpio_pin_set_dt(&ledA, 1);  // Liga LED verde
        
        //Loop feito com a intenção de travar o thread/processador, para mostrar que a interrupção possui maior prioridade
        while(1){
            if(k_cyc_to_ms_floor32(k_cycle_get_32()-startupcycle) >= BLINK_DURATION_MS){
                break;
            }
        }

        gpio_pin_set_dt(&ledA, 0);  // Desliga LED verde
   
        //Log de Fim
        unsigned long endcyle = k_cycle_get_32();
        LOG_INF("A - Fim de ciclo %d        -Horario de Inicio(ms): %d        -Horario de Fim(ms): %d       -Duracao(ms): %d", cyclecounter, k_cyc_to_ms_floor32(startupcycle), k_cyc_to_ms_floor32(endcyle), k_cyc_to_ms_floor32(endcyle-startupcycle));
        cyclecounter++;
        k_msleep(BLINK_INTERVAL_MS);       // Dorme — libera CPU
    }

}

// ----------------------------------------------------
// Definição da thread
// ----------------------------------------------------
K_THREAD_DEFINE(a_tid, 512, thread_A, NULL, NULL, NULL, PRIO_THREAD_A, 0, 0);

// ----------------------------------------------------
// Função principal
// ----------------------------------------------------
int main(void)
{
    // Inicializa GPIOs dos LEDs
    if (!device_is_ready(ledA.port) || !device_is_ready(ledB.port)) {
        return 1;
    }

    gpio_pin_configure_dt(&ledA, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledB, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&button, GPIO_INPUT | GPIO_PULL_UP);

    // Configurar interrupção na borda de subida e descida
    gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
    gpio_init_callback(&button_cb_data, button_isr, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    LOG_INF("\nMain Thread - Iniciando - V: %s - %s \n", __DATE__, __TIME__);
    while (1) {
        k_sleep(K_FOREVER); // Main dorme para liberar CPU.
    }
    return 0;
}
