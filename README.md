# PSI-Microcontroladores2-Aula07
Atividade: Semáforos de Pedestres e Veículos

## Alunos
    -Alberto Galhego Neto - NUSP 17019141
    -Arthur Londero - NUSP 16855595

# Planejamento
## Requisitos do Sistema:
    -Semáforo de veículos que alterna entre verde, amarelo e vermelho. Duração de 3s, 1s e 4s respectivamente.
    -Semaforo de pedestres que alterna entre verde e vermelho, duracao de 4s cada.
    -Botão de pedestres, que ao acionado deve fechar o semáforo de veiculs e abrir o semáforo de pedestres.(vermelho).
    -Modo noturno: Amarelo piscante (1s on, 1s off) em ambos os semaforos.
    -Comunicação com o semáforo de pedestres para sincronizar.
    -Os LEDs devem ser controlados por threads separadas.

## Arquitetura/Projeto do sistema:
### Comunicação entre os 2 MCU:
        -A comunicação é unidirecional, indo sempre do MCU de veículos ao MCU de pedestres.
        -Comunicação usando 2 fios:
            -OUT1: Indica o status de cor, servirá para implementar o acionamento do botão de pedestre:
                -High: Verde.
                -LOW: Amarelo/Vermelho.
            -OUT2: Indica o modo noturno:
                -High: Modo noturno ativado
                -LOW: Modo noturno desligado
    
    
### Arquitetura/Projeto do Código do semáforo de Veículos:
        A-Ciclo Básico: Vermelho 4 segundos, Verde 3 segundos, Amarelo 1 segundo.
            Pontos: 
            1-  Gerenciar os ciclos no main thread?
                    Criamos um loop no main thread que vai criando threads dinâmicos, que por sua vez acionam os LEDS.
                    Criamos uma variável global, chamada CurrentState, e esse loop usa um switch baseado nessa variavel para decidir qual thread criar de acordo com o seu valor.
                        Ao terminar de executar, cada thread muda o valor da CurrentState, para indicar qual thread deverá ser criada em seguida pelo main thread.
                        Podemos fazer:
                            0/Default: Vermelho, no final da execução muda o valor para 1 (verde)
                            1: Verde, no final da execução muda o valor para 2 (amarelo)
                            2: Amarelo, no final da execução muda o valor para 0 (vermelho)
                            3: Desligado, no final da execução muda o valor para 2 (amarelo) (MODO NOTURNO)

                    Podemos criar, também, uma variável chamada NightMode, que determina o modo noturno.
                        Ela muda as características dos loops. Com NightMode = 1:
                            O vermelho, ao inves de ao terminar definir CurrentState para 0(verde), volta para o amarelo.
                            O amarelo, ao inves de ligar e após 1s desligar e mudar o CurrentState para 2(vermelho), desliga e aguarda 1s e define CurrentState para 1(Amarelo).

        B-Botões:
            Pontos:
                1-Implementar Interrupts, usar polling estava dando problemas.
                    Ao serem acionados, os botões modificam o valor da variável CurrentState e NightMode.
                    Ao serem acionados, os botões também modificam o valor das saídas OUT1 e OUT2.

### Arquitetura/Projeto do Código do semáforo de pedestres:
        A-Ciclo Básico: Vermelho 4 segundos, Verde 4 segundos.
            Pontos: 
            1-  Desenvolvimento do a estrutura básica, que garante um funcionamento independente.
                Implementação de um sistema de sincronismo via GPIO, o semáforo de veículos me fornece informações sobre as transições de estado (Verde<->Vermelho ou Modo noturno <-> Rotina normal).

            2- Foi implementado um sistema de fail-safe, onde, ao estar com modo de sincronismo ativo, e não existir mudança de estado por muito tempo, o sistema ativa um modo fail-safe, garantindo que o semáforo de pedestres fique vermelha indefinidamente.

# Testes / Milestones / Passos de desenvolvimento do projeto:
### 1- Implementar e testar o sistema de loop de cores usando o mainthread e a variável CurrentState.
        [x] Testar se o loop troca as cores adequadamente.
        [x] Testar se o tempo de cada cor está sendo respeitado.
### 2-Implementar e testar o botão de pedestres
        [x] Testar se o pooling ou interrupt do botão está funcionando adequadamente.
        [x] Verificar se ocorre problemas com debouncing/acionamentos multiplos.
        [x] Verificar se há a mudança de ciclo de acordo com o esperado.
        [x] Verificar se ao acionar novamente o programa volta ao ciclo original.

### 3-Implementar e testar o botão de modo noturno.
        [x] Testar se o interrupt do botão está funcionando adequadamente.
        [x] Verificar se ocorre problemas com debouncing/acionamentos multiplos.
        [x] Verificar se há a mudança de modo de acordo com o esperado.
        [x] Verificar se ao acionar novamente o programa volta ao modo original.

### 4-Implementar e testar mecanismos de segurança para o semáforo de pedestres
        [x] O semáforo de pedestres NUNCA fica verde enquanto o de veículos não está vermelho.
        [x] O sistema não perde sincronismo, mesmo forçando reinicializações das placas.
        [x] Caso a conexão entre os sistemas seja interrompida, o semáforo de pedestres entra em modo fail-safe, ficando vermelho piscante indefinidamente.
        [x] Mecanismo de recuperação automática opcional, necessita estudos para equilibrar a resiliência vs segurança do sistema.

### 5-Implementar e testar a comunicação via GPIO
        [x] Implementar o acionamento/toggle das GPIO
        [x] Verificar no osciloscópio se há a mudança do sinal e eles estao funcionando adequadamente.
    
### 6-Juntar os 2 MCU e Testar
        [x]As duas placas conseguem enviar/receber sinal.
        [x] As duas placas conseguem entrar em sincronia das cores e de modos.
        [x] O sinal de pedestres respeita o botao de pedestres interrompendo o ciclo?
        [x] As duas placas funcionam individualmente?
        [x] O botao de pedestre e de modo noturno funciona e o sinal e propagado para a outra placa?
        [x] O piscar do modo noturno e sincronizado?
        [ ] O controlador de pedestres opera com fail-safe para eventualidades?

# Conclusao
        O sistema funciona de acordo com o esperado, todos os testes foram concluidos com sucesso.
