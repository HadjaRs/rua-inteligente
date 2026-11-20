#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool manual;
    bool enabled;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t brightness;
} ControleIluminacaoWeb;

void iniciarServidorWeb(void);
void atualizarLeituraWeb(int valorLdr, bool ledAtivo);
void obterControleIluminacaoWeb(ControleIluminacaoWeb *controle);
void obterEstadoCasaWeb(bool *ligada);

#endif
