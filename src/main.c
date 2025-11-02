/*
    Planejamento do Código, comentários iniciais etc. foram movidos para o arquivo PlanejamentoDoProjeto.txt
*/


#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/sys/time_units.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

// --- Logs ---
LOG_MODULE_REGISTER(LOG_INF_APP, LOG_LEVEL_INF);

// --- Configuração via DeviceTree ---
#define LED_A_NODE DT_ALIAS(led0)  // LED verde
#define LED_B_NODE DT_ALIAS(led2)  // LED vermelho
#define LED_C_NODE DT_ALIAS(led1)  // LED azul
#define BUTTON_NODE_PED DT_NODELABEL(user_button_0) //Botao Pedestre PTA16

static const struct gpio_dt_spec ledG = GPIO_DT_SPEC_GET(LED_A_NODE, gpios);
static const struct gpio_dt_spec ledR = GPIO_DT_SPEC_GET(LED_B_NODE, gpios);
static const struct gpio_dt_spec ledB = GPIO_DT_SPEC_GET(LED_C_NODE, gpios);
static const struct gpio_dt_spec buttonPedestrian = GPIO_DT_SPEC_GET(BUTTON_NODE_PED, gpios);
static struct gpio_callback button_cbped_data;

// --- Prioridades e tempos ---
#define PRIO_THREAD_CREATED 0

#define RED_DURATION_MS 4000
#define GREEN_DURATION_MS 3000
#define YELLOW_DURATION_MS 1000
#define OFF_DURATION_MS 1000

// --- Variaveis Globais ---
atomic_t CurrentState = ATOMIC_INIT(3); //Comecar sempre desligado (3)
atomic_t NightMode = ATOMIC_INIT(false); //Modo noturno
atomic_t PedestrianMode = ATOMIC_INIT(false); //Modo Pedestre
atomic_t currentColorThreadID; //TID da thread de cor atual criada pela main(vermelho, verde, amarelo, off)

// --- Threads ---
K_THREAD_STACK_DEFINE(red_stack, 512);
struct k_thread red_data;
K_THREAD_STACK_DEFINE(green_stack, 512);
struct k_thread green_data;
K_THREAD_STACK_DEFINE(yellow_stack, 512);
struct k_thread yellow_data;
K_THREAD_STACK_DEFINE(off_stack, 512);
struct k_thread off_data;


void red_thread(void *arg1, void *arg2, void *arg3) {
    LOG_INF("NOVA THREAD RED");
    if (atomic_get(&PedestrianMode))
    {
        LOG_INF("MODO PEDESTRE!");
        gpio_pin_set_dt(&ledB, 1);
    }
    gpio_pin_set_dt(&ledR, 1);
    k_msleep(RED_DURATION_MS);
    gpio_pin_set_dt(&ledR, 0);
    LOG_INF("FIM RED");


    //Muda o CurrentState de acordo com o NightMode
    if (atomic_get(&NightMode))
    {
        atomic_set(&CurrentState, 3); // Próximo estado: Desligado (ciclo noturno)
    }
    else
    {
        atomic_set(&CurrentState, 1); // Próximo estado: Verde
    }
    
    //Desativa o Modo Pedestre, se ativado
    if (atomic_get(&PedestrianMode))
    {
        gpio_pin_set_dt(&ledB, 0);
        atomic_set(&PedestrianMode, false);
        LOG_INF("MODO PEDESTRE DESATIVADO");
    }
}

void green_thread(void *arg1, void *arg2, void *arg3) {
    LOG_INF("NOVA THREAD GREEN");
    gpio_pin_set_dt(&ledG, 1);
    k_msleep(GREEN_DURATION_MS);
    gpio_pin_set_dt(&ledG, 0);
    LOG_INF("FIM GREEN");

    //Muda o CurrentState
    atomic_set(&CurrentState, 2); //Próximo estado: Amarelo
}

void yellow_thread(void *arg1, void *arg2, void *arg3) {
    LOG_INF("NOVA THREAD YELLOW");
    gpio_pin_set_dt(&ledG, 1);
    gpio_pin_set_dt(&ledR, 1);
    k_msleep(YELLOW_DURATION_MS);
    gpio_pin_set_dt(&ledG, 0);
    gpio_pin_set_dt(&ledR, 0);
    LOG_INF("FIM YELLOW");
    
    //Muda o CurrentState de acordo com o NightMode
    if (atomic_get(&NightMode))
    {
        //Muda o CurrentState
        atomic_set(&CurrentState, 3); //Próximo estado: Desligado (ciclo noturno)
    }
    else
    {
        //Muda o CurrentState
        atomic_set(&CurrentState, 0); //Próximo estado: Vermelho
    }
}

void off_thread(void *arg1, void *arg2, void *arg3) {
    LOG_INF("NOVA THREAD OFF");
    // Mantém tudo desligado
    k_msleep(OFF_DURATION_MS);
    LOG_INF("FIM OFF");
    //Muda o CurrentState de acordo com o NightMode
    if (atomic_get(&NightMode))
    {
        //Muda o CurrentState
        atomic_set(&CurrentState, 2); //Próximo estado: Amarelo (para piscar)
    }
    else
    {
        //Muda o CurrentState
        atomic_set(&CurrentState, 0); //Próximo estado: Vermelho
    }
}

// --- Interrupts ---

void buttonPedestrian_isr(const struct device *devped, struct gpio_callback *cbped, uint32_t pins)
{
    LOG_INF("INTERRUPT - PREFILTRO");
    k_sched_lock();
    if (atomic_get(&PedestrianMode))
    {
        LOG_INF("MODO PEDESTRE JA ESTA ATIVADO");
        k_sched_unlock();
        return;
    }
    else
    {
        atomic_set(&PedestrianMode, true); //Ativa o modo pedestre
        atomic_set(&CurrentState, 0); //Muda o proximo para vermelho
        gpio_pin_set_dt(&ledR, 0); //Desliga o vermelho
        gpio_pin_set_dt(&ledG, 0); //Desliga o verde
        gpio_pin_set_dt(&ledB, 0); //Desliga o azul
        k_thread_abort((k_tid_t)atomic_get(&currentColorThreadID)); //Aborta o thread da cor atual, caso seja inválido (o thread já finalizou/terminou), nada acontece.
        LOG_INF("TENTATIVA DE ATIVAR O MODO PEDESTRE");
        LOG_INF("END INTERRUPT");
        k_sched_unlock();
    }
}


// ----------------------------------------------------
// Função principal
// ----------------------------------------------------
int main(void)
{
    //k_thread_priority_set(k_current_get(),2); //Define a prioridade da main para 2. Usaremos isso para evitar que ela interrompa as outras threads.

    //Inicializa GPIOs dos LEDs
    if (!device_is_ready(ledG.port) || !device_is_ready(ledR.port) || !device_is_ready(ledB.port)){
        return 1;
    }

    gpio_pin_configure_dt(&ledG, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledR, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledB, GPIO_OUTPUT_INACTIVE);

    //Botao de Pedestres
    gpio_pin_configure_dt(&buttonPedestrian, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure_dt(&buttonPedestrian, GPIO_INT_EDGE_FALLING);
    gpio_init_callback(&button_cbped_data, buttonPedestrian_isr, BIT(buttonPedestrian.pin));
    gpio_add_callback(buttonPedestrian.port, &button_cbped_data);

    //Teste dos LEDs
    gpio_pin_set_dt(&ledB, 1);  //Liga LED azul
    k_msleep(100);
    gpio_pin_set_dt(&ledB, 0);  //Desliga LED azul 
    gpio_pin_set_dt(&ledG, 1);  //Liga LED verde
    k_msleep(100);
    gpio_pin_set_dt(&ledG, 0);  //Desliga LED verde 
    gpio_pin_set_dt(&ledR, 1);  //Liga LED vermelho
    k_msleep(100);
    gpio_pin_set_dt(&ledG, 1);  //Liga os 2 LEDs (Amarelo)
    k_msleep(100);
    gpio_pin_set_dt(&ledG, 0);  //Desliga LED verde 
    gpio_pin_set_dt(&ledR, 0);  //Desliga LED vermelho 

    //Log Inicial
    LOG_INF("\nMain Thread - Iniciando - V: %s - %s \n", __DATE__, __TIME__);
    k_msleep(100); //Para dar tempo de printar o Log


    while (1) {
        LOG_INF("MAIN - ESCOLHENDO NOVO THREAD");
        //Decide para Qual Mudar
        switch (atomic_get(&CurrentState))
        {
        case 1:
            {
                //Verde
                k_tid_t tid = k_thread_create(&green_data, green_stack, K_THREAD_STACK_SIZEOF(green_stack), green_thread, NULL, NULL, NULL, 1, 0, K_NO_WAIT);
                atomic_set(&currentColorThreadID, (atomic_val_t)tid);
                k_thread_join(tid, K_FOREVER);
                break;
            }
        case 2:
            {
                //Amarelo
                k_tid_t tid = k_thread_create(&yellow_data, yellow_stack, K_THREAD_STACK_SIZEOF(yellow_stack), yellow_thread, NULL, NULL, NULL, 1, 0, K_NO_WAIT);
                atomic_set(&currentColorThreadID, (atomic_val_t)tid);
                k_thread_join(tid, K_FOREVER);
                break;
            }
        case 3:
            {
                //Desligado - Ciclo Noturno
                k_tid_t tid = k_thread_create(&off_data, off_stack, K_THREAD_STACK_SIZEOF(off_stack), off_thread, NULL, NULL, NULL, 1, 0, K_NO_WAIT);
                atomic_set(&currentColorThreadID, (atomic_val_t)tid);
                k_thread_join(tid, K_FOREVER);
                break;
            }
        default:
            {
                //0 ou Default - Vermelho
                k_tid_t tid = k_thread_create(&red_data, red_stack, K_THREAD_STACK_SIZEOF(red_stack), red_thread, NULL, NULL, NULL, 1, 0, K_NO_WAIT);
                atomic_set(&currentColorThreadID, (atomic_val_t)tid);
                k_thread_join(tid, K_FOREVER);
                break;
            }
        }
        k_msleep(1); //Pequeno delay para evitar busy-waiting e ceder a CPU
    }
    return 0;
}
