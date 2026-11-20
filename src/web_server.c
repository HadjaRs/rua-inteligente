#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "esp_http_server.h"
#include "web_server.h"
#include "web_assets.h"

static portMUX_TYPE sensorMux = portMUX_INITIALIZER_UNLOCKED;
static int valorLdrAtual;
static bool estadoLedAtual;
static bool estadoCasaAtual;
static ControleIluminacaoWeb controleIluminacao = {
    .manual = false,
    .enabled = false,
    .red = 250,
    .green = 204,
    .blue = 21,
    .brightness = 75,
};

static esp_err_t enviarArquivo(httpd_req_t *requisicao, const char *tipo,
                               const unsigned char *dados, size_t tamanho) {
    esp_err_t resultado = httpd_resp_set_type(requisicao, tipo);
    if (resultado != ESP_OK) {
        return resultado;
    }

    resultado = httpd_resp_set_hdr(requisicao, "Cache-Control", "no-store");
    if (resultado != ESP_OK) {
        return resultado;
    }

    return httpd_resp_send(requisicao, (const char *)dados, tamanho);
}

static esp_err_t paginaInicialHandler(httpd_req_t *requisicao) {
    return enviarArquivo(requisicao, "text/html; charset=utf-8",
                         web_index, sizeof(web_index) - 1);
}

static esp_err_t cssHandler(httpd_req_t *requisicao) {
    return enviarArquivo(requisicao, "text/css; charset=utf-8",
                         web_style, sizeof(web_style) - 1);
}

static esp_err_t javascriptHandler(httpd_req_t *requisicao) {
    return enviarArquivo(requisicao, "application/javascript; charset=utf-8",
                         web_script, sizeof(web_script) - 1);
}

static esp_err_t iconeHandler(httpd_req_t *requisicao) {
    return enviarArquivo(requisicao, "image/png", web_icon, sizeof(web_icon) - 1);
}

static esp_err_t casaImagemHandler(httpd_req_t *requisicao) {
    return enviarArquivo(requisicao, "image/png", web_house, sizeof(web_house) - 1);
}

static esp_err_t sensorHandler(httpd_req_t *requisicao) {
    int leitura;
    bool ledAtivo;
    char resposta[128];

    taskENTER_CRITICAL(&sensorMux);
    leitura = valorLdrAtual;
    ledAtivo = estadoLedAtual;
    taskEXIT_CRITICAL(&sensorMux);

    int tamanho = snprintf(resposta, sizeof(resposta),
                           "{\"ldr\":%d,\"led\":%s,\"estado\":\"%s\"}",
                           leitura, ledAtivo ? "true" : "false",
                           ledAtivo ? "Noite (Escuro)" : "Dia (Claro)");
    esp_err_t resultado = httpd_resp_set_type(requisicao, "application/json; charset=utf-8");
    if (resultado != ESP_OK) {
        return resultado;
    }

    return httpd_resp_send(requisicao, resposta, tamanho);
}

static esp_err_t enviarJson(httpd_req_t *requisicao, const char *resposta);

static esp_err_t estadoCasaHandler(httpd_req_t *requisicao) {
    bool ligada;
    char resposta[32];

    taskENTER_CRITICAL(&sensorMux);
    ligada = estadoCasaAtual;
    taskEXIT_CRITICAL(&sensorMux);

    snprintf(resposta, sizeof(resposta), "{\"enabled\":%s}",
             ligada ? "true" : "false");
    return enviarJson(requisicao, resposta);
}

static esp_err_t enviarJson(httpd_req_t *requisicao, const char *resposta) {
    esp_err_t resultado = httpd_resp_set_type(requisicao, "application/json; charset=utf-8");
    if (resultado != ESP_OK) {
        return resultado;
    }

    resultado = httpd_resp_set_hdr(requisicao, "Cache-Control", "no-store");
    if (resultado != ESP_OK) {
        return resultado;
    }

    return httpd_resp_sendstr(requisicao, resposta);
}

static esp_err_t controleLedHandler(httpd_req_t *requisicao) {
    ControleIluminacaoWeb controle;
    char resposta[128];

    taskENTER_CRITICAL(&sensorMux);
    controle = controleIluminacao;
    taskEXIT_CRITICAL(&sensorMux);

    snprintf(resposta, sizeof(resposta),
             "{\"mode\":\"%s\",\"enabled\":%s,\"r\":%u,\"g\":%u,"
             "\"b\":%u,\"brightness\":%u}",
             controle.manual ? "manual" : "auto",
             controle.enabled ? "true" : "false",
             (unsigned)controle.red, (unsigned)controle.green,
             (unsigned)controle.blue, (unsigned)controle.brightness);
    return enviarJson(requisicao, resposta);
}

static bool lerParametro(const char *dados, const char *chave, int maximo, int *valor) {
    char texto[16];
    char *fim;
    long numero;

    if (httpd_query_key_value(dados, chave, texto, sizeof(texto)) != ESP_OK ||
        texto[0] == '\0') {
        return false;
    }

    numero = strtol(texto, &fim, 10);
    if (*fim != '\0' || numero < 0 || numero > maximo) {
        return false;
    }

    *valor = (int)numero;
    return true;
}

static esp_err_t atualizarEstadoCasaHandler(httpd_req_t *requisicao) {
    char dados[32];
    size_t recebido = 0;
    int ligada;

    if (requisicao->content_len == 0 || requisicao->content_len >= sizeof(dados)) {
        return httpd_resp_send_err(requisicao, HTTPD_400_BAD_REQUEST, "Corpo invalido");
    }

    while (recebido < requisicao->content_len) {
        int bytes = httpd_req_recv(requisicao, dados + recebido,
                                   requisicao->content_len - recebido);
        if (bytes <= 0) {
            return httpd_resp_send_err(requisicao, HTTPD_400_BAD_REQUEST,
                                       "Nao foi possivel ler o controle");
        }
        recebido += (size_t)bytes;
    }
    dados[recebido] = '\0';

    if (!lerParametro(dados, "enabled", 1, &ligada)) {
        return httpd_resp_send_err(requisicao, HTTPD_400_BAD_REQUEST,
                                   "Estado da iluminacao invalido");
    }

    taskENTER_CRITICAL(&sensorMux);
    estadoCasaAtual = ligada != 0;
    taskEXIT_CRITICAL(&sensorMux);

    return enviarJson(requisicao, "{\"ok\":true}");
}

static esp_err_t atualizarControleLedHandler(httpd_req_t *requisicao) {
    char dados[96];
    char modo[16];
    size_t recebido = 0;
    int ligado;
    int vermelho;
    int verde;
    int azul;
    int brilho;

    if (requisicao->content_len == 0 || requisicao->content_len >= sizeof(dados)) {
        return httpd_resp_send_err(requisicao, HTTPD_400_BAD_REQUEST, "Corpo invalido");
    }

    while (recebido < requisicao->content_len) {
        int bytes = httpd_req_recv(requisicao, dados + recebido,
                                   requisicao->content_len - recebido);
        if (bytes <= 0) {
            return httpd_resp_send_err(requisicao, HTTPD_400_BAD_REQUEST,
                                       "Nao foi possivel ler o controle");
        }
        recebido += (size_t)bytes;
    }
    dados[recebido] = '\0';

    if (httpd_query_key_value(dados, "mode", modo, sizeof(modo)) != ESP_OK) {
        return httpd_resp_send_err(requisicao, HTTPD_400_BAD_REQUEST, "Modo invalido");
    }

    if (strcmp(modo, "auto") == 0) {
        taskENTER_CRITICAL(&sensorMux);
        controleIluminacao.manual = false;
        taskEXIT_CRITICAL(&sensorMux);
        return enviarJson(requisicao, "{\"ok\":true}");
    }

    if (strcmp(modo, "manual") != 0 ||
        !lerParametro(dados, "enabled", 1, &ligado) ||
        !lerParametro(dados, "r", 255, &vermelho) ||
        !lerParametro(dados, "g", 255, &verde) ||
        !lerParametro(dados, "b", 255, &azul) ||
        !lerParametro(dados, "brightness", 100, &brilho)) {
        return httpd_resp_send_err(requisicao, HTTPD_400_BAD_REQUEST,
                                   "Configuracao de iluminacao invalida");
    }

    taskENTER_CRITICAL(&sensorMux);
    controleIluminacao.manual = true;
    controleIluminacao.enabled = ligado != 0;
    controleIluminacao.red = (uint8_t)vermelho;
    controleIluminacao.green = (uint8_t)verde;
    controleIluminacao.blue = (uint8_t)azul;
    controleIluminacao.brightness = (uint8_t)brilho;
    taskEXIT_CRITICAL(&sensorMux);

    return enviarJson(requisicao, "{\"ok\":true}");
}

void iniciarServidorWeb(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 10;
    httpd_handle_t servidor = NULL;
    ESP_ERROR_CHECK(httpd_start(&servidor, &config));

    const httpd_uri_t rotas[] = {
        {.uri = "/", .method = HTTP_GET, .handler = paginaInicialHandler},
        {.uri = "/style.css", .method = HTTP_GET, .handler = cssHandler},
        {.uri = "/script.js", .method = HTTP_GET, .handler = javascriptHandler},
        {.uri = "/images/iconeW.png", .method = HTTP_GET, .handler = iconeHandler},
        {.uri = "/images/casa.png", .method = HTTP_GET, .handler = casaImagemHandler},
        {.uri = "/api/sensor", .method = HTTP_GET, .handler = sensorHandler},
        {.uri = "/api/led", .method = HTTP_GET, .handler = controleLedHandler},
        {.uri = "/api/led", .method = HTTP_POST, .handler = atualizarControleLedHandler},
        {.uri = "/api/house", .method = HTTP_GET, .handler = estadoCasaHandler},
        {.uri = "/api/house", .method = HTTP_POST, .handler = atualizarEstadoCasaHandler},
    };

    for (size_t i = 0; i < sizeof(rotas) / sizeof(rotas[0]); i++) {
        ESP_ERROR_CHECK(httpd_register_uri_handler(servidor, &rotas[i]));
    }

    printf("Pagina web disponivel em http://192.168.4.1/\n");
}

void atualizarLeituraWeb(int valorLdr, bool ledAtivo) {
    taskENTER_CRITICAL(&sensorMux);
    valorLdrAtual = valorLdr;
    estadoLedAtual = ledAtivo;
    taskEXIT_CRITICAL(&sensorMux);
}

void obterControleIluminacaoWeb(ControleIluminacaoWeb *controle) {
    taskENTER_CRITICAL(&sensorMux);
    *controle = controleIluminacao;
    taskEXIT_CRITICAL(&sensorMux);
}

void obterEstadoCasaWeb(bool *ligada) {
    taskENTER_CRITICAL(&sensorMux);
    *ligada = estadoCasaAtual;
    taskEXIT_CRITICAL(&sensorMux);
}
