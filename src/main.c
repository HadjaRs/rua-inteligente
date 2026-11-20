#include <stdio.h>               // biblioteca padrão de entrada e saída
#include <stdint.h>              // tipos inteiros com tamanho fixo
#include <string.h>
#include "freertos/FreeRTOS.h"   // gerenciamento de tarefas do FreeRTOS
#include "freertos/task.h"       
#include "driver/gpio.h"         // controle de pinos GPIO
#include "driver/ledc.h"         // controle PWM para os LEDs
#include "esp_adc/adc_oneshot.h" // leitura analógica ADC
#include "esp_event.h" // permite que a esp registre eventos e crie um loop de eventos
#include "esp_netif.h" // faz a interface de rede da esp32 funcionar
#include "esp_wifi.h" // cria o Access Point e gerencia a conexão Wi-Fi
#include "nvs_flash.h" // é a biblioteca que permite gravar dados na memória flash da ESP32
#include "web_server.h"

#define RED_PIN   18
#define GREEN_PIN 19
#define BLUE_PIN  20
#define CASA_LED_PIN GPIO_NUM_21
#define CANAL_LDR ADC_CHANNEL_0  // GPIO 1 na ESP32-S2
#define AP_SSID "PorteESP"
#define AP_PASSWORD "maquete123"
#define AP_CHANNEL 1 // é o canal de frequência do Wi-Fi, pode ser qualquer número entre 1 e 13
#define AP_MAX_CONNECTIONS 2 // número máximo de dispositivos que podem se conectar ao Access Point

const int LIMIAR_ACENDER = 400; // Valor bem baixo: exige cobrir o LDR quase 100%
const int LIMIAR_APAGAR  = 800; // Valor alto: exige luz direta/ambiente limpo para apagar

static adc_oneshot_unit_handle_t leitorLDR;

void iniciarAccessPoint(void) {
    esp_err_t resultado = nvs_flash_init();
    if (resultado == ESP_ERR_NVS_NO_FREE_PAGES ||
        resultado == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        resultado = nvs_flash_init();
    }
    ESP_ERROR_CHECK(resultado);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t configWifi = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&configWifi));

    wifi_config_t configAP = {0};
    memcpy(configAP.ap.ssid, AP_SSID, sizeof(AP_SSID));
    memcpy(configAP.ap.password, AP_PASSWORD, sizeof(AP_PASSWORD));
    configAP.ap.ssid_len = sizeof(AP_SSID) - 1;
    configAP.ap.channel = AP_CHANNEL;
    configAP.ap.max_connection = AP_MAX_CONNECTIONS;
    configAP.ap.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &configAP));
    ESP_ERROR_CHECK(esp_wifi_start());

    printf("Access Point ativo: %s\n", AP_SSID);
    printf("IP do Access Point: 192.168.4.1\n");
}

void definirCores(uint8_t red, uint8_t green, uint8_t blue) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, red);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, blue);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, green);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);

    //ledc_set_duty = informa a ESP qual intensidade usar em um canal
    // LEDC_LOW_SPEED_MODE = escolhe o odo de Velocidade baixa
    // LEDC_CHANNEL_0 = escolhe o canal
    // red = valor da intensidade recebido

    // ledc_update_duty = aplica o valor da intensidade na saida e fica sempre atualizando

}

void ligarPinosLed(void) {
     //define como a esp vai gerar o sinal p controlar o pino
    ledc_timer_config_t temporizador = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0, //escolhe o temporizador 0
        .duty_resolution = LEDC_TIMER_8_BIT, //define 2^8 onde os valores vão de 0  a 255
        .freq_hz = 5000, // diz q o sinal vai se repedir 5k vezes por segundo
        .clk_cfg = LEDC_AUTO_CLK //deixa a espidf escolher a fonte de relógio adequada
    };
    ledc_timer_config(&temporizador); //envia essas opções pra esp

    //config os 3 canais PWM e conecta cada um ao pino correspondente do RGB
    gpio_num_t pinos[3] = {RED_PIN, GREEN_PIN, BLUE_PIN};
    for (int i = 0; i < 3; i++) {
        ledc_channel_config_t canal = { //estrutura p/ config dos canais
            .gpio_num = pinos[i],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = (ledc_channel_t)i,
            .timer_sel = LEDC_TIMER_0
        };
        ledc_channel_config(&canal);
    }
}


void ligarPinoLDR(void) {
    //cria estrutura de configuração do ADC e escolhe a unidade ADC1
    adc_oneshot_unit_init_cfg_t configUnidade = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&configUnidade, &leitorLDR);

    adc_oneshot_chan_cfg_t configCanal = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(leitorLDR, CANAL_LDR, &configCanal);
}

// --- FILTRO: REALIZA A MÉDIA DE 10 LEITURAS ---
int lerLDRFiltrado(void) {
    int soma = 0;
    int leituraTemp = 0;
    for (int i = 0; i < 10; i++) {
        adc_oneshot_read(leitorLDR, CANAL_LDR, &leituraTemp);
        soma += leituraTemp;
        vTaskDelay(pdMS_TO_TICKS(5)); // Pequena pausa entre amostras
    }
    return soma / 10;
}

// --- PROGRAMA PRINCIPAL ---
void app_main(void) {
    iniciarAccessPoint();
    ligarPinosLed();
    ligarPinoLDR();
    iniciarServidorWeb();
    gpio_set_direction(CASA_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(CASA_LED_PIN, 0);
    definirCores(0, 0, 0); // Começa desligado

    bool ledAtivo = false;

    while (1) {
        int ldrValor = lerLDRFiltrado();
        ControleIluminacaoWeb controle;
        bool casaLigada;
        obterControleIluminacaoWeb(&controle);
        obterEstadoCasaWeb(&casaLigada);
        gpio_set_level(CASA_LED_PIN, casaLigada ? 1 : 0);

        if (controle.manual) {
            if (controle.enabled) {
                definirCores(
                    (uint8_t)(controle.red * controle.brightness / 100),
                    (uint8_t)(controle.green * controle.brightness / 100),
                    (uint8_t)(controle.blue * controle.brightness / 100)
                );
            } else {
                definirCores(0, 0, 0);
            }
            ledAtivo = controle.enabled;
        } else {
            // Lógica com histerese
            if (!ledAtivo && ldrValor < LIMIAR_ACENDER) {
                definirCores(255, 255, 255); // Acende luz branca
                ledAtivo = true;
            } else if (ledAtivo && ldrValor > LIMIAR_APAGAR) {
                definirCores(0, 0, 0);       // Apaga o LED
                ledAtivo = false;
            }
        }

        printf("Leitura LDR (Média): %d | Estado LED: %s\n", ldrValor, ledAtivo ? "LIGADO" : "APAGADO");

        atualizarLeituraWeb(ldrValor, ledAtivo);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
