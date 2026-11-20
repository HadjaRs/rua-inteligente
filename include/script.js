const valorLdr = document.getElementById("ldr-value");
const estadoSensor = document.getElementById("sensor-status");
const indicadorLdr = document.querySelector(".sensor-gauge");
const preenchimentoLdr = document.querySelector(".sensor-gauge-fill");
const controleLuz = document.getElementById("light-toggle");
const seletorCor = document.getElementById("light-color");
const valorCor = document.getElementById("color-value");
const seletorBrilho = document.getElementById("light-brightness");
const valorBrilho = document.getElementById("brightness-value");
const modoLuz = document.getElementById("light-mode");
const botaoAutomatico = document.getElementById("auto-mode");
const controleCasa = document.getElementById("house-toggle");
const estadoCasa = document.getElementById("house-status");
const VALOR_MAXIMO_LUMINOSIDADE = 1000;
const ARCO_MAXIMO_GRAUS = 235;
const CIRCUNFERENCIA_GAUGE = 2 * Math.PI * 42;
const COMPRIMENTO_ARCO_MAXIMO = CIRCUNFERENCIA_GAUGE * ARCO_MAXIMO_GRAUS / 360;
const COR_MINIMA = [59, 130, 246];
const COR_MAXIMA = [236, 72, 153];
let modoAutomatico = true;
let casaLigada = false;

function atualizarEstadoCasa(ligada) {
    casaLigada = ligada;
    controleCasa.checked = ligada;
    estadoCasa.textContent = ligada ? "Ligada" : "Desligada";
}

async function carregarEstadoCasa() {
    try {
        const resposta = await fetch("/api/house", { cache: "no-store" });
        if (!resposta.ok) {
            throw new Error(`Falha ao consultar a iluminação da casa: HTTP ${resposta.status}`);
        }

        const casa = await resposta.json();
        if (typeof casa.enabled !== "boolean") {
            throw new Error("Resposta da iluminação da casa inválida");
        }
        atualizarEstadoCasa(casa.enabled);
    } catch (erro) {
        estadoCasa.textContent = "Sem conexão";
        console.error(erro);
    }
}

function atualizarIndicadorLdr(leitura) {
    const progresso = Math.max(0, Math.min(leitura / VALOR_MAXIMO_LUMINOSIDADE, 1));
    const comprimentoPreenchido = progresso * COMPRIMENTO_ARCO_MAXIMO;
    const comprimentoVazio = CIRCUNFERENCIA_GAUGE - comprimentoPreenchido;
    const cor = COR_MINIMA.map((componente, indice) =>
        Math.round(componente + (COR_MAXIMA[indice] - componente) * progresso)
    );

    preenchimentoLdr.style.strokeDasharray = `${comprimentoPreenchido} ${comprimentoVazio}`;
    preenchimentoLdr.style.stroke = `rgb(${cor.join(", ")})`;
    indicadorLdr.setAttribute("aria-label", `Leitura do LDR: ${Math.round(progresso * 100)}%`);
}

function definirCor(cor) {
    seletorCor.value = cor;
    valorCor.value = cor.toUpperCase();
    valorCor.textContent = cor.toUpperCase();

    document.querySelectorAll(".color-swatch").forEach((botao) => {
        const selecionado = botao.dataset.color.toLowerCase() === cor.toLowerCase();
        botao.classList.toggle("is-selected", selecionado);
        botao.setAttribute("aria-pressed", String(selecionado));
    });
}

function atualizarModo(modo) {
    modoAutomatico = modo === "auto";
    modoLuz.textContent = modoAutomatico ? "Modo automático" : "Controle manual";
}

async function enviarControle(modo) {
    const dados = new URLSearchParams({ mode: modo });

    if (modo === "manual") {
        const cor = seletorCor.value;
        dados.set("enabled", controleLuz.checked ? "1" : "0");
        dados.set("r", String(parseInt(cor.slice(1, 3), 16)));
        dados.set("g", String(parseInt(cor.slice(3, 5), 16)));
        dados.set("b", String(parseInt(cor.slice(5, 7), 16)));
        dados.set("brightness", seletorBrilho.value);
    }

    const resposta = await fetch("/api/led", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: dados.toString(),
        cache: "no-store"
    });
    if (!resposta.ok) {
        throw new Error(`Falha ao atualizar a iluminação: HTTP ${resposta.status}`);
    }

    atualizarModo(modo);
}

async function atualizarLeitura() {
    try {
        const resposta = await fetch("/api/sensor", { cache: "no-store" });
        if (!resposta.ok) {
            throw new Error(`Falha ao consultar o sensor: HTTP ${resposta.status}`);
        }

        const sensor = await resposta.json();
        if (typeof sensor.ldr !== "number" || typeof sensor.estado !== "string" ||
            typeof sensor.led !== "boolean") {
            throw new Error("Resposta do sensor inválida");
        }

        valorLdr.textContent = sensor.ldr;
        estadoSensor.textContent = `Estado: ${sensor.estado}`;
        atualizarIndicadorLdr(sensor.ldr);
        if (modoAutomatico) {
            controleLuz.checked = sensor.led;
        }
    } catch (erro) {
        estadoSensor.textContent = "Sem conexão com o sensor";
        console.error(erro);
    }
}

async function carregarControle() {
    try {
        const resposta = await fetch("/api/led", { cache: "no-store" });
        if (!resposta.ok) {
            throw new Error(`Falha ao consultar a iluminação: HTTP ${resposta.status}`);
        }
        const controle = await resposta.json();
        if (typeof controle.mode !== "string" || typeof controle.enabled !== "boolean" ||
            typeof controle.r !== "number" || typeof controle.g !== "number" ||
            typeof controle.b !== "number" || typeof controle.brightness !== "number") {
            throw new Error("Resposta do controle de iluminação inválida");
        }

        controleLuz.checked = controle.enabled;
        definirCor(`#${[controle.r, controle.g, controle.b]
            .map((componente) => componente.toString(16).padStart(2, "0"))
            .join("")}`);
        seletorBrilho.value = controle.brightness;
        valorBrilho.value = `${controle.brightness}%`;
        valorBrilho.textContent = `${controle.brightness}%`;
        atualizarModo(controle.mode);
    } catch (erro) {
        modoLuz.textContent = "Sem conexão";
        console.error(erro);
    }
}

document.querySelectorAll(".color-swatch").forEach((botao) => {
    botao.addEventListener("click", () => {
        definirCor(botao.dataset.color);
        enviarControle("manual").catch((erro) => {
            modoLuz.textContent = "Falha ao atualizar";
            console.error(erro);
        });
    });
});

seletorCor.addEventListener("input", () => definirCor(seletorCor.value));
seletorCor.addEventListener("change", () => {
    enviarControle("manual").catch((erro) => {
        modoLuz.textContent = "Falha ao atualizar";
        console.error(erro);
    });
});

seletorBrilho.addEventListener("input", () => {
    valorBrilho.value = `${seletorBrilho.value}%`;
    valorBrilho.textContent = `${seletorBrilho.value}%`;
});
seletorBrilho.addEventListener("change", () => {
    enviarControle("manual").catch((erro) => {
        modoLuz.textContent = "Falha ao atualizar";
        console.error(erro);
    });
});

controleLuz.addEventListener("change", () => {
    enviarControle("manual").catch((erro) => {
        modoLuz.textContent = "Falha ao atualizar";
        console.error(erro);
    });
});

botaoAutomatico.addEventListener("click", () => {
    enviarControle("auto").catch((erro) => {
        modoLuz.textContent = "Falha ao atualizar";
        console.error(erro);
    });
});

controleCasa.addEventListener("change", async () => {
    const ligada = controleCasa.checked;
    try {
        const resposta = await fetch("/api/house", {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body: new URLSearchParams({ enabled: ligada ? "1" : "0" }).toString(),
            cache: "no-store"
        });
        if (!resposta.ok) {
            throw new Error(`Falha ao atualizar a iluminação da casa: HTTP ${resposta.status}`);
        }
        atualizarEstadoCasa(ligada);
    } catch (erro) {
        controleCasa.checked = casaLigada;
        estadoCasa.textContent = "Falha ao atualizar";
        console.error(erro);
    }
});

carregarControle();
carregarEstadoCasa();
atualizarLeitura();
setInterval(atualizarLeitura, 1000);
