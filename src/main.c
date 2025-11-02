/*
Requisitos do Sistema:
    -Semáforo de veículos que alterna entre verde, amarelo e vermelho. Duração de 3s, 1s e 4s respectivamente.
    -Botão de pedestres, que ao acionado deve fechar o semáforo (vermelho).
        -Implementar amarelo por 1s e depois vermelho? Ficaria mais interessante.
    -Modo noturno: Amarelo piscante (1s on, 1s off).
    -Comunicação com o semáforo de pedestres.
    -Os LEDs devem ser controlados por threads separadas, para garantir mutex.
        -Fazer sem mutex, com threads separadas de mesma prioridade e sem round-robin, facilitaria muito ao permitir o uso de interrupts para os botões/comunicações. Mas entendemos que o uso de mutex é requisito.

Arquitetura/Projeto do sistema:
    -Comunicação entre os 2 MCU:
        -A comunicação é unidirecional, indo sempre do MCU de veículos ao MCU de pedestres.
        -Comunicação usando 2 fios:
            -OUT1: Indica o status de cor, servirá para implementar o acionamento do botão de pedestre:
                -High: Verde.
                -LOW: Amarelo/Vermelho.
            -OUT2: Indica o modo noturno:
                -High: Modo noturno ativado
                -LOW: Modo noturno desligado
    
    
    -Arquitetura/Projeto do Código do semáforo de Veículos:
        A-Ciclo Básico: Vermelho 4 segundos, Verde 3 segundos, Amarelo 1 segundo.
            Pontos: 
            1-  Gerenciar os ciclos no main thread?
                    Criamos um loop no main thread que vai criando threads dinâmicos, que por sua vez acionam os LEDS.
                    Criamos uma variável global, chamada CurrentState, e esse loop usa um switch baseado nessa variavel para decidir qual thread criar de acordo com o seu valor.
                        Ao terminar de executar, cada thread muda o valor da CurrentState, para indicar qual thread deverá ser criada em seguida pelo main thread.
                        Podemos fazer:
                            0/Default: Verde, no final da execução muda o valor para 1
                            1: Amarelo, no final da execução muda o valor para 2
                            2: Vermelho, no final da execução, muda o valor para 0

                    Podemos criar, também, uma variável chamada NightMode, que determina o modo noturno.
                        Ela muda as características dos loops. Com NightMode = 1:
                            O vermelho, ao inves de ao terminar definir CurrentState para 0(verde), volta para o amarelo.
                            O amarelo, ao inves de ligar e após 1s desligar e mudar o CurrentState para 2(vermelho), desliga e aguarda 1s e define CurrentState para 1(Amarelo).

        B-Botões:
            Pontos:
                1-  Foi solicitado o uso de mutex, nesse caso, não será possível usar interrupt para os botões. Usar polling.
                    Seria interessante criar uma função de polling para facilitar o código. Ela verifica os botões e retorna se a função de aguardo do loop atual deve ser interrompida precocemente (retorna uma bool).
                    Ao serem acionados, os botões modificam o valor da variável CurrentState e NightMode.
                    Ao serem acionados, os botões tb modificam o valor das saídas OUT1 e OUT2


Milestones/Passos de desenvolvimento:
    1-Implementar e testar o sistema de loop de cores usando o mainthread e a variável CurrentState
        -Testar se o loop troca as cores adequadamente
        -Testar se o tempo de cada cor está sendo respeitado.

    2-Implementar e testar o botão de pedestres
        -Testar se o pooling do botão está funcionando adequadamente.
        -Verificar e ajustar a frequencia de pooling.
        -Verificar se ocorre problemas com debouncing/acionamentos multiplos.
        -Verificar se há a mudança de ciclo de acordo com o esperado

    3-Implementar e testar o botão de modo noturno.
        -Testar se o pooling do botão está funcionando adequadamente.
        -Verificar e ajustar a frequencia de pooling.
        -Verificar se ocorre problemas com debouncing/acionamentos multiplos.
        -Verificar se há a mudança de modo de acordo com o esperado.
        -Verificar se ao acionar novamente o programa volta ao modo original.

    4-Implementar e testar a comunicação via GPIO
        -Implementar o acionamento/toggle das GPIO
        -Verificar no osciloscópio se há a mudança do sinal.
    
    5-Juntar os 2 MCU e testar (CONTINUA....)

*/


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
    gpio_pin_toggle_dt(&ledB); //Toggle led Vermelho
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
