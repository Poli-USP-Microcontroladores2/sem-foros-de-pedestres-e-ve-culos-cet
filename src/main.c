#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(LOG_INF_PED, LOG_LEVEL_INF);

// ----------------------------------------------------
// Configuração das GPIOs
// ----------------------------------------------------
static const struct gpio_dt_spec ped_red   = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
static const struct gpio_dt_spec ped_green = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static const struct gpio_dt_spec in1 = { .port = DEVICE_DT_GET(DT_NODELABEL(gpiob)), .pin = 0, .dt_flags = GPIO_ACTIVE_HIGH }; // sincronismo
static const struct gpio_dt_spec in2 = { .port = DEVICE_DT_GET(DT_NODELABEL(gpiob)), .pin = 1, .dt_flags = GPIO_ACTIVE_HIGH }; // modo noturno

// ----------------------------------------------------
// Variáveis globais
// ----------------------------------------------------
atomic_t vehicle_red  = ATOMIC_INIT(false);
atomic_t night_mode   = ATOMIC_INIT(false);

// A variável sync alterna entre o modo independente e o modo sincronizado.
// OBS: o modo independente não possui modo noturno, pois a botoeira é parte do sistema mestre.
bool sync = true;
struct k_mutex ped_mutex;

// ----------------------------------------------------
// Threads
// ----------------------------------------------------
K_THREAD_STACK_DEFINE(red_stack, 256);
K_THREAD_STACK_DEFINE(green_stack, 256);
static struct k_thread red_thread_data;
static struct k_thread green_thread_data;

// ----------------------------------------------------
// Thread Vermelho
// ----------------------------------------------------
void ped_red_thread(void *a, void *b, void *c)
{
    while (1) {
        int night   = atomic_get(&night_mode);
        int veh_red = atomic_get(&vehicle_red);

        if (night) {
            // No modo noturno, o LED vermelho pisca em sincronia com o sinal mestre.
            // veh_red == 1 (sinal mestre alto) -> LED vermelho aceso
            k_mutex_lock(&ped_mutex, K_FOREVER);
            gpio_pin_set_dt(&ped_red, veh_red);
            gpio_pin_set_dt(&ped_green, 0); // Garante que o verde esteja sempre apagado
            k_mutex_unlock(&ped_mutex);
            k_msleep(50);
        } 
        else if (!veh_red) {
            // Veículo verde → pedestre vermelho
            k_mutex_lock(&ped_mutex, K_FOREVER);
            gpio_pin_set_dt(&ped_red, 1);
            gpio_pin_set_dt(&ped_green, 0);
            k_mutex_unlock(&ped_mutex);
            k_msleep(50);
        } 
        else {
            k_msleep(50);
        }
    }
}

// ----------------------------------------------------
// Thread do LED Verde (pedestre verde)
// ----------------------------------------------------
void ped_green_thread(void *a, void *b, void *c)
{
    while (1) {
        int night   = atomic_get(&night_mode);
        int veh_red = atomic_get(&vehicle_red);

        if (!night && veh_red) {
            // Veículo vermelho = pedestre verde
            k_mutex_lock(&ped_mutex, K_FOREVER);
            gpio_pin_set_dt(&ped_green, 1);
            gpio_pin_set_dt(&ped_red, 0);
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

    gpio_pin_configure_dt(&ped_red, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ped_green, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&in1, GPIO_INPUT);
    gpio_pin_configure_dt(&in2, GPIO_INPUT);

    k_mutex_init(&ped_mutex);

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
    const int LOOP_MS = 10;

    while (1) {
        int sync_high = gpio_pin_get_dt(&in1);
        int night_in  = gpio_pin_get_dt(&in2);

        atomic_set(&night_mode, night_in ? 1 : 0);

        // Sincronismo
        if (sync) {
            veh_state = sync_high ? 1 : 0;
            last_sync_high = sync_high;
        } 
        else {
            // Modo independente
            local_timer += LOOP_MS;
            if (local_timer >= CYCLE_TIME_MS) {
                veh_state = !veh_state;
                local_timer = 0;
            }
        }

        // Atualiza o estado global
        atomic_set(&vehicle_red, veh_state);

        k_msleep(LOOP_MS);
    }
}
