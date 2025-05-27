// Sistema de controle de acesso a uma balada por meio de freeRTOS e api de semaforos.
// O sistema utiliza semaforos e mutex para controlar o acesso de usuários que são definidos por USUARIOS_FULL.


// Bibliotecas utilizadas
#include <stdio.h>              
#include <ctype.h>              
#include <string.h>             
#include "pico/stdlib.h"        
#include "pico/binary_info.h"   
#include "hardware/pwm.h"             
#include "hardware/clocks.h"    
#include "ssd1306.h"            
#include "FreeRTOS.h"           // Bibliotecas do FreeRTOS
#include "FreeRTOSConfig.h"     
#include "task.h"               // API da Biblioteca para tarefas
#include "semphr.h"             // API da Biblioteca para semáforos e mutex

// Definições e constantes
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO 0x3C
#define WIDTH 128
#define HEIGHT 64
#define RED_PIN 13
#define BLUE_PIN 12
#define GREEN_PIN 11
#define BOTAO_A 5
#define BOTAO_B 6
#define BOTAO_JOYSTICK 22       // Botão do joystick para reset
#define BUZZER 21

// Definindo a frequência desejada
#define PWM_FREQ_LED 1000  // 1 kHz
#define PWM_FREQ_BUZZER 1000  // 1 kHz
#define PWM_WRAP 255   // 8 bits de wrap (256 valores)

// Número máximo de usuários permitidos
uint8_t USUARIOS_FULLS = 9;

// Flags e Variáveis globais
ssd1306_t ssd;
volatile uint8_t g_num_usuarios = 0;   // Quantidade atual de usuários (inicia zero)

// Definição dos semáforos
SemaphoreHandle_t xSemaphoreContagem;  // Semáforo de contagem para controlar usuários
SemaphoreHandle_t xSemaphoreReset;     // Semáforo binário para reset
SemaphoreHandle_t xMutexDisplay;       // Mutex para proteger acesso ao display OLED

// Protótipos de funções
void initSettings();
void initssd1306();
void configurarBuzzer(uint32_t volume);
void configurarLEDRGB(uint8_t r, uint8_t g, uint8_t b);

// Protótipos das tarefas
void vTaskEntrada(void *pvParameters);
void vTaskSaida(void *pvParameters);
void vTaskReset(void *pvParameters);
void vDisplayOLEDTask(void *pvParameters);
void vLEDRGBTask(void *pvParameters);

// Função de callback para interrupção do botão de reset
void reset_button_callback(uint gpio, uint32_t events) {
    static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if (gpio == BOTAO_JOYSTICK) {
        // Liberar o semáforo binário para acionar o reset
        xSemaphoreGiveFromISR(xSemaphoreReset, &xHigherPriorityTaskWoken);
        
        // Realizar o context switch se necessário
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

int main() {
    // Inicializa stdio
    stdio_init_all();
    
    // Inicializa as configurações
    initSettings();
    initssd1306();
    
    // Cria o semáforo de contagem
    xSemaphoreContagem = xSemaphoreCreateCounting(USUARIOS_FULLS, USUARIOS_FULLS);
    
    // Cria o semáforo binário para reset
    xSemaphoreReset = xSemaphoreCreateBinary();
    
    // Cria o mutex para o display
    xMutexDisplay = xSemaphoreCreateMutex();
    
    // Configura a interrupção para o botão do joystick
    gpio_set_irq_enabled_with_callback(BOTAO_JOYSTICK, GPIO_IRQ_EDGE_FALL, true, &reset_button_callback);
    
    // Cria as tarefas
    xTaskCreate(vTaskEntrada, "Entrada Usuario", configMINIMAL_STACK_SIZE*2,
               NULL, tskIDLE_PRIORITY + 2, NULL);
               
    xTaskCreate(vTaskSaida, "Saida Usuario", configMINIMAL_STACK_SIZE*2,
               NULL, tskIDLE_PRIORITY + 2, NULL);
               
    xTaskCreate(vTaskReset, "Reset Sistema", configMINIMAL_STACK_SIZE*2,
               NULL, tskIDLE_PRIORITY + 3, NULL);
               
    xTaskCreate(vDisplayOLEDTask, "Display OLED", configMINIMAL_STACK_SIZE*2,
               NULL, tskIDLE_PRIORITY + 1, NULL);
               
    xTaskCreate(vLEDRGBTask, "LED RGB", configMINIMAL_STACK_SIZE*2,
               NULL, tskIDLE_PRIORITY + 1, NULL);
    
    // Inicia o escalonador do FreeRTOS
    vTaskStartScheduler();
    
    // O código nunca deve chegar aqui
    panic_unsupported();
}

// Função para configurar o volume do buzzer
void configurarBuzzer(uint32_t volume) {
    pwm_set_gpio_level(BUZZER, volume);
}

// Função para configurar a cor do LED RGB
void configurarLEDRGB(uint8_t r, uint8_t g, uint8_t b) {
    pwm_set_gpio_level(RED_PIN, r);
    pwm_set_gpio_level(GREEN_PIN, g);
    pwm_set_gpio_level(BLUE_PIN, b);
}

void initSettings(){
    // Inicializa os pinos dos botões
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    
    gpio_init(BOTAO_JOYSTICK);
    gpio_set_dir(BOTAO_JOYSTICK, GPIO_IN);
    gpio_pull_up(BOTAO_JOYSTICK);
    
    // Inicializa os pinos PWM para os LEDs e buzzer
    gpio_set_function(RED_PIN, GPIO_FUNC_PWM);
    gpio_set_function(BLUE_PIN, GPIO_FUNC_PWM);
    gpio_set_function(GREEN_PIN, GPIO_FUNC_PWM);
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);
    
    // Obtém os números dos canais PWM para os pinos
    uint slice_num_red = pwm_gpio_to_slice_num(RED_PIN);
    uint slice_num_blue = pwm_gpio_to_slice_num(BLUE_PIN);
    uint slice_num_green = pwm_gpio_to_slice_num(GREEN_PIN);
    uint slice_num_buzzer = pwm_gpio_to_slice_num(BUZZER);
    
    // Configuração da frequência PWM
    pwm_set_clkdiv(slice_num_red, (float)clock_get_hz(clk_sys) / PWM_FREQ_LED / (PWM_WRAP + 1));
    pwm_set_clkdiv(slice_num_blue, (float)clock_get_hz(clk_sys) / PWM_FREQ_LED / (PWM_WRAP + 1));
    pwm_set_clkdiv(slice_num_green, (float)clock_get_hz(clk_sys) / PWM_FREQ_LED / (PWM_WRAP + 1));
    pwm_set_clkdiv(slice_num_buzzer, (float)clock_get_hz(clk_sys) / PWM_FREQ_BUZZER / (PWM_WRAP + 1));
    
    // Configura o wrap do contador PWM para 8 bits (256)
    pwm_set_wrap(slice_num_red, PWM_WRAP);
    pwm_set_wrap(slice_num_blue, PWM_WRAP);
    pwm_set_wrap(slice_num_green, PWM_WRAP);
    pwm_set_wrap(slice_num_buzzer, PWM_WRAP);
    
    // Habilita o PWM
    pwm_set_enabled(slice_num_red, true);
    pwm_set_enabled(slice_num_blue, true);
    pwm_set_enabled(slice_num_green, true);
    pwm_set_enabled(slice_num_buzzer, true);
    
    // Configura LED RGB inicialmente para azul (nenhum usuário)
    configurarBuzzer(0);
    configurarLEDRGB(0, 0, 255);
}

void initssd1306() {
    // I2C Initialisation.
    i2c_init(I2C_PORT, 600 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
}

// Tarefa responsável pela entrada de usuários
void vTaskEntrada(void *pvParameters) {
    const TickType_t xDelayTicks = pdMS_TO_TICKS(100); // 100ms
    const TickType_t xDebounceDelay = pdMS_TO_TICKS(200); // debounce de 200ms
    bool botao_a_anterior = true; // true = não pressionado
    
    while (1) {
        // Lê o estado do botão A (entrada)
        bool botao_a_atual = gpio_get(BOTAO_A);
        
        // Verifica se o botão foi pressionado (borda de descida)
        if (botao_a_anterior && !botao_a_atual) {
            // Tenta obter um "slot" no semáforo de contagem
            // Se xSemaphoreTake retornar pdTRUE, então há espaço para mais um usuário
            if (xSemaphoreTake(xSemaphoreContagem, 0) == pdTRUE) {
                // Incrementa o contador global de usuários
                g_num_usuarios++;
            } else {
                // Capacidade máxima atingida, emite sinal sonoro
                configurarBuzzer(50);
                vTaskDelay(pdMS_TO_TICKS(100));
                configurarBuzzer(0);
            }
            
            // Debounce
            vTaskDelay(xDebounceDelay);
        }
        
        // Armazena o estado atual do botão para a próxima iteração
        botao_a_anterior = botao_a_atual;
        
        vTaskDelay(xDelayTicks);
    }
}

// Tarefa responsável pela saída de usuários
void vTaskSaida(void *pvParameters) {
    const TickType_t xDelayTicks = pdMS_TO_TICKS(100); // 100ms
    const TickType_t xDebounceDelay = pdMS_TO_TICKS(200); // debounce de 200ms
    bool botao_b_anterior = true; // true = não pressionado
    
    while (1) {
        // Lê o estado do botão B (saída)
        bool botao_b_atual = gpio_get(BOTAO_B);
        
        // Verifica se o botão foi pressionado
        if (botao_b_anterior && !botao_b_atual) {
            // Verifica se há usuários para remover
            if (g_num_usuarios > 0) {
                // Libera um "slot" no semáforo de contagem
                xSemaphoreGive(xSemaphoreContagem);
                
                // Decrementa o contador de usuários
                g_num_usuarios--;
            }
            
            // Debounce
            vTaskDelay(xDebounceDelay);
        }
        
        // Armazena o estado atual do botão para a próxima iteração
        botao_b_anterior = botao_b_atual;
        
        vTaskDelay(xDelayTicks);
    }
}

// Tarefa responsável pelo reset do sistema
void vTaskReset(void *pvParameters) {
    while (1) {
        // Aguarda a liberação do semáforo (que ocorre por interrupção)
        if (xSemaphoreTake(xSemaphoreReset, portMAX_DELAY) == pdTRUE) {
            // Emite beep duplo
            configurarBuzzer(50);
            vTaskDelay(pdMS_TO_TICKS(100));
            configurarBuzzer(0);
            vTaskDelay(pdMS_TO_TICKS(100));
            configurarBuzzer(50);
            vTaskDelay(pdMS_TO_TICKS(100));
            configurarBuzzer(0);
            
            // Reseta o contador de usuários
            int usuarios_anteriores = g_num_usuarios;
            g_num_usuarios = 0;
            
            // Libera o semaforo de contagem
            for (int i = 0; i < usuarios_anteriores; i++) {
                xSemaphoreGive(xSemaphoreContagem);
            }
        }
    }
}

// Tarefa para atualizar o LED RGB com base no número de usuários
void vLEDRGBTask(void *pvParameters) {
    const TickType_t xDelayTicks = pdMS_TO_TICKS(100); // 100ms
    uint8_t num_usuarios_anterior = 0;
    
    while (1) {
        // Verificar se houve mudança no número de usuários
        if (num_usuarios_anterior != g_num_usuarios) {
            // Atualiza o LED RGB com base no número atual de usuários
            if (g_num_usuarios == 0) {
                // Azul: Nenhum usuário
                configurarLEDRGB(0, 0, 255);
            } else if (g_num_usuarios < USUARIOS_FULLS - 1) {
                // Verde: Há espaço disponível
                configurarLEDRGB(0, 255, 0);
            } else if (g_num_usuarios == USUARIOS_FULLS - 1) {
                // Amarelo: Apenas 1 vaga restante
                configurarLEDRGB(255, 255, 0);
            } else {
                // Vermelho: Capacidade máxima
                configurarLEDRGB(255, 0, 0);
            }
            
            // Atualiza o valor anterior
            num_usuarios_anterior = g_num_usuarios;
        }
        
        vTaskDelay(xDelayTicks);
    }
}

// Tarefa para atualizar o display OLED
void vDisplayOLEDTask(void *pvParameters) {
    const TickType_t xDelayTicks = pdMS_TO_TICKS(100); // 100ms
    char buffer[64];
    uint8_t num_usuarios_anterior = 140; // Valor inicial diferente de qualquer número de usuários possível
    
    while (1) {
        // Verifica se houve mudança no número de usuários
        if (num_usuarios_anterior != g_num_usuarios) {
            // Tenta obter o mutex para acessar o display
            if (xSemaphoreTake(xMutexDisplay, portMAX_DELAY) == pdTRUE) {
                // Limpar o display
                ssd1306_fill(&ssd, false);
                
                // Título do sistema
                ssd1306_draw_string(&ssd, "Controle acesso", 0, 0);
                
                // Status atual
                sprintf(buffer, "Usuarios: %d/%d", g_num_usuarios, USUARIOS_FULLS);
                ssd1306_draw_string(&ssd, buffer, 0, 20);
                
                // Vagas disponíveis
                sprintf(buffer, "Vagas: %d", USUARIOS_FULLS - g_num_usuarios);
                ssd1306_draw_string(&ssd, buffer, 0, 30);
                
                // Status de lotação
                if (g_num_usuarios == 0) {
                    ssd1306_draw_string(&ssd, "Status: Vazio", 0, 40);
                } else if (g_num_usuarios < USUARIOS_FULLS - 1) {
                    ssd1306_draw_string(&ssd, "Status: Livre", 0, 40);
                } else if (g_num_usuarios == USUARIOS_FULLS - 1) {
                    ssd1306_draw_string(&ssd, "Status: Ultima", 0, 40);
                } else {
                    ssd1306_draw_string(&ssd, "Status: Lotado", 0, 40);
                }
                
                // Instruções
                ssd1306_draw_string(&ssd, "A entrar B sair", 0, 55);
                
                // Enviar dados para o display
                ssd1306_send_data(&ssd);
                
                // Libera o mutex
                xSemaphoreGive(xMutexDisplay);
                
                // Atualiza o valor anterior
                num_usuarios_anterior = g_num_usuarios;
            }
        }
        
        vTaskDelay(xDelayTicks);
    }
}