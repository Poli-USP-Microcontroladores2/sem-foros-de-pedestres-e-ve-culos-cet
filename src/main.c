#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(LOG_INF_PED, LOG_LEVEL_INF);

// ----------------------------------------------------
// Configuração das GPIOs
// ----------------------------------------------------

// LEDs integrados (vermelho e verde do pedestre)
static const struct gpio_dt_spec ped_red   = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec ped_green = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

// Entradas PTB0 e PTB1
static const struct gpio_dt_spec in1 = { .port = DEVICE_DT_GET(DT_NODELABEL(gpiob)), .pin = 0, .dt_flags = GPIO_ACTIVE_HIGH }; // sincronismo (veículo vermelho)
static const struct gpio_dt_spec in2 = { .port = DEVICE_DT_GET(DT_NODELABEL(gpiob)), .pin = 1, .dt_flags = GPIO_ACTIVE_HIGH }; // modo noturno

// ----------------------------------------------------
// Variáveis globais
// ----------------------------------------------------
atomic_t vehicle_red = ATOMIC_INIT(false);
atomic_t night_mode  = ATOMIC_INIT(false);

bool sync = true;  // true = sincronizado, false = independente

struct k_mutex ped_mutex;

// ----------------------------------------------------
// Threads
// ----------------------------------------------------
K_THREAD_STACK_DEFINE(red_stack, 512);
K_THREAD_STACK_DEFINE(green_stack, 512);
static struct k_thread red_thread_data;
static struct k_thread green_thread_data;

// ----------------------------------------------------
// Thread do LED Vermelho
// ----------------------------------------------------
void ped_red_thread(void *a, void *b, void *c)
{
    while (1) {
        int night   = atomic_get(&night_mode);
        int veh_red = atomic_get(&vehicle_red);

        if (night) {
            // --- Modo noturno: vermelho piscando ---
            k_mutex_lock(&ped_mutex, K_FOREVER);
            gpio_pin_set_dt(&ped_red, 0);
            gpio_pin_set_dt(&ped_green, 1);
            k_mutex_unlock(&ped_mutex);
            k_msleep(1000);

            k_mutex_lock(&ped_mutex, K_FOREVER);
            gpio_pin_set_dt(&ped_red, 0);
            gpio_pin_set_dt(&ped_green, 0);
            k_mutex_unlock(&ped_mutex);
            k_msleep(1000);
        } 
        else if (!veh_red) {
            // Veículo verde = pedestre vermelho
            k_mutex_lock(&ped_mutex, K_FOREVER);
            gpio_pin_set_dt(&ped_red, 0);
            gpio_pin_set_dt(&ped_green, 1);
            k_mutex_unlock(&ped_mutex);
            k_msleep(50);
        } 
        else {
            k_msleep(50);
        }
    }
}

// ----------------------------------------------------
// Thread do LED Verde
// ----------------------------------------------------
void ped_green_thread(void *a, void *b, void *c)
{
    while (1) {
        int night   = atomic_get(&night_mode);
        int veh_red = atomic_get(&vehicle_red);

        if (!night && veh_red) {
            // Veículo vermelho = pedestre verde
            k_mutex_lock(&ped_mutex, K_FOREVER);
            gpio_pin_set_dt(&ped_green, 0);
            gpio_pin_set_dt(&ped_red, 1);
            k_mutex_unlock(&ped_mutex);
            k_msleep(50);
        } else {
            k_msleep(50);
        }
    }
}

// ----------------------------------------------------
// Função principal
// ----------------------------------------------------
int main(void)
{
    LOG_INF("Semáforo de Pedestres Iniciando...");

    // Configura LEDs
    gpio_pin_configure_dt(&ped_red, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ped_green, GPIO_OUTPUT_INACTIVE);

    // Configura entradas
    gpio_pin_configure_dt(&in1, GPIO_INPUT);
    gpio_pin_configure_dt(&in2, GPIO_INPUT);

    k_mutex_init(&ped_mutex);

    // Cria threads
    k_thread_create(&red_thread_data, red_stack, K_THREAD_STACK_SIZEOF(red_stack),
                    ped_red_thread, NULL, NULL, NULL, 2, 0, K_NO_WAIT);

    k_thread_create(&green_thread_data, green_stack, K_THREAD_STACK_SIZEOF(green_stack),
                    ped_green_thread, NULL, NULL, NULL, 2, 0, K_NO_WAIT);

    // ----------------------------------------------------
    // Controle principal de sincronismo e temporização
    // ----------------------------------------------------
    bool last_sync_high = false;
    int local_timer = 0;
    int veh_state = 0;  // 0 = verde (ped vermelho), 1 = vermelho (ped verde)

    const int CYCLE_TIME_MS = 4000;
    const int LOOP_MS = 100;

    while (1) {
        int sync_in = gpio_pin_get_dt(&in1);
        int night_in = gpio_pin_get_dt(&in2);

        atomic_set(&night_mode, night_in ? 1 : 0);

        if (sync) {
            // Detecta borda HIGH = reseta sincronismo
            if (sync_in && !last_sync_high) {
                local_timer = 0;
                veh_state = 1; // veículo vermelho = ped verde
            }
            last_sync_high = sync_in;

            local_timer += LOOP_MS;
            if (local_timer >= CYCLE_TIME_MS) {
                veh_state = !veh_state;
                local_timer = 0;
            }
        } 
        else {
            // --- modo independente ---
            local_timer += LOOP_MS;
            if (local_timer >= CYCLE_TIME_MS) {
                veh_state = !veh_state;
                local_timer = 0;
            }
        }

        atomic_set(&vehicle_red, veh_state);
        k_msleep(LOOP_MS);
    }
}
