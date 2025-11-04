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
#define BUTTON_NODE_NIGHT DT_NODELABEL(user_button_1) //Botao Modo Noturno PTA17

static const struct gpio_dt_spec ledG = GPIO_DT_SPEC_GET(LED_A_NODE, gpios);
static const struct gpio_dt_spec ledR = GPIO_DT_SPEC_GET(LED_B_NODE, gpios);
static const struct gpio_dt_spec ledB = GPIO_DT_SPEC_GET(LED_C_NODE, gpios);
static const struct gpio_dt_spec buttonPedestrian = GPIO_DT_SPEC_GET(BUTTON_NODE_PED, gpios);
static const struct gpio_dt_spec buttonNightMode = GPIO_DT_SPEC_GET(BUTTON_NODE_NIGHT, gpios);
static const struct gpio_dt_spec out1 = { .port = DEVICE_DT_GET(DT_NODELABEL(gpioa)), .pin = 5, .dt_flags = GPIO_ACTIVE_HIGH };
static const struct gpio_dt_spec out2 = { .port = DEVICE_DT_GET(DT_NODELABEL(gpioa)), .pin = 4, .dt_flags = GPIO_ACTIVE_HIGH };
static struct gpio_callback button_cbped_data;
static struct gpio_callback button_cbnight_data;
int64_t button_night_debounce;

// --- Prioridades, tempos e outras configuracoes ---
#define PEDESTRIAN_PURPLE 1 //O vermelho de pedestres fica roxo para diferenciar do vermelho normal
#define PRIO_THREAD_CREATED 1 //Prioridade dos Threads de cores que sao criados
#define RED_DURATION_MS 4000
#define GREEN_DURATION_MS 3000
#define YELLOW_DURATION_MS 1000
#define OFF_DURATION_MS 1000

// --- Variaveis Globais ---
atomic_t CurrentState = ATOMIC_INIT(2); //Comecar sempre amarelo (2)
atomic_t NightMode = ATOMIC_INIT(true); //Modo noturno
atomic_t PedestrianMode = ATOMIC_INIT(false); //Modo Pedestre
atomic_t currentColorThreadID; //TID da thread de cor atual criada pela main(vermelho, verde, amarelo)

// --- Threads ---
//Se faltar memoria, mudar o tamanho das stacks, provavelmente quase nao esta sendo usado.
K_THREAD_STACK_DEFINE(red_stack, 512);
struct k_thread red_data;
K_THREAD_STACK_DEFINE(green_stack, 512);
struct k_thread green_data;
K_THREAD_STACK_DEFINE(yellow_stack, 512);
struct k_thread yellow_data;


void red_thread(void *arg1, void *arg2, void *arg3) {
    LOG_INF("NOVA THREAD RED");
    gpio_pin_set_dt(&out1, 1); // OUT1 HIGH: Vermelho
    if (atomic_get(&PedestrianMode) && PEDESTRIAN_PURPLE) //Deixa o LED Roxo
    {
        LOG_INF("MODO PEDESTRE!");
        gpio_pin_set_dt(&ledB, 1);
    }
    gpio_pin_set_dt(&ledR, 1); // Liga LED Vermelho
    k_msleep(RED_DURATION_MS);
    gpio_pin_set_dt(&ledR, 0); // Desliga LED Vermelho
    LOG_INF("FIM RED");


    //Muda o CurrentState de acordo com o NightMode
    if (atomic_get(&NightMode))
    {
        atomic_set(&CurrentState, 2); // Próximo estado: Amarelo (ciclo noturno)
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
    gpio_pin_set_dt(&out1, 0); // OUT1 LOW: Vermelho
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
    //OUT1 High no night mode, para sincronizar os piscares
    if(atomic_get(&NightMode))
    {
        gpio_pin_set_dt(&out1, 1); // OUT1 HIGH
    }
    gpio_pin_set_dt(&ledG, 1);
    gpio_pin_set_dt(&ledR, 1);
    k_msleep(YELLOW_DURATION_MS);
    gpio_pin_set_dt(&ledG, 0);
    gpio_pin_set_dt(&ledR, 0);
    
    //Se NightMode, permanece um período desligado e define CurrentState para 2 (amarelo) novamente
    if (atomic_get(&NightMode))
    {
        gpio_pin_set_dt(&out1, 0);//OUT1 LOW
        k_msleep(OFF_DURATION_MS);
        //Muda o CurrentState
        atomic_set(&CurrentState, 2); //Próximo estado: Desligado (ciclo noturno)
    }
    else
    {
        //Muda o CurrentState
        atomic_set(&CurrentState, 0); //Próximo estado: Vermelho
    }

    LOG_INF("FIM YELLOW");
}

// --- Interrupts ---

void buttonPedestrian_isr(const struct device *devped, struct gpio_callback *cbped, uint32_t pins)
{
    LOG_INF("INTERRUPT - PREFILTRO");
    k_sched_lock();
    if(atomic_get(&NightMode))
    {
        LOG_INF("TENTATIVA DE ATIVAR O MODO PEDESTRE - NOTURNO ESTA ATIVADO");
        k_sched_unlock();
        return;
    }
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

void buttonNightMode_isr(const struct device *devnig, struct gpio_callback *cbnig, uint32_t pins)
{
    LOG_INF("INTERRUPT - Botao Modo Noturno Pressionado");
    k_sched_lock();
    //Debounce
    if((k_cyc_to_ms_floor32((k_cycle_get_32() - button_night_debounce)))>=100)
    {
        atomic_set(&NightMode, !atomic_get(&NightMode));
        gpio_pin_set_dt(&out2, atomic_get(&NightMode)); // Atualiza OUT2
        gpio_pin_set_dt(&ledR, 0); //Desliga o vermelho
        gpio_pin_set_dt(&ledG, 0); //Desliga o verde
        gpio_pin_set_dt(&ledB, 0); //Desliga o azul
        atomic_set(&CurrentState, 2); //Muda o proximo para amarelo
        k_thread_abort((k_tid_t)atomic_get(&currentColorThreadID)); //Aborta o thread da cor atual, caso seja inválido (o thread já finalizou/terminou), nada acontece.
        LOG_INF("Modo Noturno: %ld", atomic_get(&NightMode));
        button_night_debounce = k_cycle_get_32();
    }
    k_sched_unlock();
}

// ----------------------------------------------------
// Função principal
// ----------------------------------------------------
int main(void)
{
    int64_t button_night_debounce = k_cycle_get_32();
    
    //Inicializa GPIOs
    if (!device_is_ready(ledG.port) || !device_is_ready(ledR.port) || !device_is_ready(ledB.port)){
        LOG_ERR("Erro ao inicializar GPIOS - LED - G: %d, R: %d, B: %d", device_is_ready(ledG.port), device_is_ready(ledR.port), device_is_ready(ledB.port));
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

    //Botao de Modo Noturno
    gpio_pin_configure_dt(&buttonNightMode, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure_dt(&buttonNightMode, GPIO_INT_EDGE_FALLING);
    gpio_init_callback(&button_cbnight_data, buttonNightMode_isr, BIT(buttonNightMode.pin));
    gpio_add_callback(buttonNightMode.port, &button_cbnight_data);
    button_night_debounce = k_cycle_get_32();

    //Inicializa OUT1 e OUT2
    if (!device_is_ready(out1.port) || !device_is_ready(out2.port)){
        LOG_ERR("Erro ao inicializar GPIOS - OUT1: %d, OUT2: %d", device_is_ready(out1.port), device_is_ready(out2.port));
        return 1;
    }
    gpio_pin_configure_dt(&out1, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&out2, GPIO_OUTPUT_INACTIVE);
    gpio_pin_set_dt(&out2, atomic_get(&NightMode));

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
                k_tid_t tid = k_thread_create(&green_data, green_stack, K_THREAD_STACK_SIZEOF(green_stack), green_thread, NULL, NULL, NULL, PRIO_THREAD_CREATED, 0, K_NO_WAIT);
                atomic_set(&currentColorThreadID, (atomic_val_t)tid);
                k_thread_join(tid, K_FOREVER);
                break;
            }
        case 2:
            {
                //Amarelo
                k_tid_t tid = k_thread_create(&yellow_data, yellow_stack, K_THREAD_STACK_SIZEOF(yellow_stack), yellow_thread, NULL, NULL, NULL, PRIO_THREAD_CREATED, 0, K_NO_WAIT);
                atomic_set(&currentColorThreadID, (atomic_val_t)tid);
                k_thread_join(tid, K_FOREVER);
                break;
            }
        default:
            {
                //0 ou Default - Vermelho
                k_tid_t tid = k_thread_create(&red_data, red_stack, K_THREAD_STACK_SIZEOF(red_stack), red_thread, NULL, NULL, NULL, PRIO_THREAD_CREATED, 0, K_NO_WAIT);
                atomic_set(&currentColorThreadID, (atomic_val_t)tid);
                k_thread_join(tid, K_FOREVER);
                break;
            }
        }
        //k_msleep(1); //Pequeno delay para evitar busy-waiting e ceder a CPU
    }
    return 0;
}