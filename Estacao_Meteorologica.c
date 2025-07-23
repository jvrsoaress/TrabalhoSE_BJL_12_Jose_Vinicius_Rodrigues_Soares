// Estação Meteorológica com Servidor Web e Display OLED
// Monitora temperatura, umidade, pressão e altitude com sensores AHT20 e BMP280.
// Exibe dados localmente em um display OLED e remotamente via página web com gráficos.
// Permite a configuração de limites de alerta através da interface web.
// Desenvolvido por José Vinicius

#include "pico/cyw43_arch.h"        // biblioteca para suporte ao módulo Wi-Fi CYW43
#include "pico/stdlib.h"           // biblioteca de funções padrão do SDK do Pico
#include "lwip/tcp.h"              // biblioteca para a pilha de protocolos TCP/IP (servidor web)
#include "hardware/i2c.h"            // biblioteca para controle da comunicação I2C
#include "hardware/adc.h"            // biblioteca para controle do conversor analógico-digital
#include "hardware/gpio.h"           // biblioteca para controle dos pinos de entrada/saída (GPIO)
#include "hardware/pio.h"            // biblioteca para controle do PIO (Programmable I/O)
#include "hardware/pwm.h"            // biblioteca para controle de PWM (Pulse Width Modulation)
#include <stdio.h>                   // biblioteca padrão de entrada e saída (para printf)
#include <string.h>                  // biblioteca para manipulação de strings (para strcmp, strlen, etc.)
#include <stdlib.h>                  // biblioteca para funções de utilidade geral (para atof)
#include <math.h>                    // biblioteca para funções matemáticas (para pow)
#include "aht20.h"                   // driver para o sensor de umidade AHT20
#include "bmp280.h"                  // driver para o sensor de pressão e temperatura BMP280
#include "ssd1306.h"                 // driver para o display OLED SSD1306
#include "font.h"                    // fonte de caracteres para o display OLED
#include "generated/ws2812.pio.h"    // programa PIO pré-compilado para o LED WS2812

// --- Definições de Pinos ---
#define WS2812_PIN 7                 // pino GPIO conectado ao pino de dados do LED Neopixel WS2812
#define LED_G 11                     // pino GPIO para o componente verde do LED RGB
#define LED_B 12                     // pino GPIO para o componente azul do LED RGB
#define LED_R 13                     // pino GPIO para o componente vermelho do LED RGB
#define BUZZER_PIN 10                // pino GPIO conectado ao buzzer
#define BOTAO_A 5                    // pino GPIO para o botão A
#define BOTAO_B 6                    // pino GPIO para o botão B
#define JOYSTICK_Y 27                // pino GPIO para o eixo Y do joystick (não utilizado neste código)
#define JOYSTICK_SW 22               // pino GPIO para o botão de switch do joystick
#define I2C_PORT_DISP i2c1           // porta I2C 1 usada para o display OLED
#define I2C_SDA_DISP 14              // pino GPIO para a linha de dados (SDA) do I2C do display
#define I2C_SCL_DISP 15              // pino GPIO para a linha de clock (SCL) do I2C do display
#define ENDERECO 0x3C                // endereço I2C do display OLED SSD1306
#define I2C_PORT_SENSORES i2c0       // porta I2C 0 usada para os sensores
#define I2C_SDA_SENSORES 0           // pino GPIO para a linha de dados (SDA) do I2C dos sensores
#define I2C_SCL_SENSORES 1           // pino GPIO para a linha de clock (SCL) do I2C dos sensores

// --- Variáveis Globais ---
float temperatura_bmp = 0.0, umidade_aht = 0.0; // armazenam os valores lidos dos sensores
float pressao_bmp = 0.0, altitude_bmp = 0.0;    // armazenam os valores lidos e calculados
float temp_lim_min = 18.0, temp_lim_max = 40.0; // limites de temperatura para o sistema de alerta
float umid_lim_min = 30.0, umid_lim_max = 70.0; // limites de umidade para o sistema de alerta
float press_lim_min = 950.0, press_lim_max = 1050.0; // limites de pressão para o sistema de alerta
bool alerta_ativo = false;                         // flag que indica se o alerta está ativo (true) ou não (false)
char ip_str[16] = "?.?.?.?";                       // string para armazenar o endereço IP do dispositivo
enum { MENU_PRINCIPAL, TELA_MONITORAMENTO, TELA_LIMITES } estado_menu = MENU_PRINCIPAL; // controla qual tela principal é exibida no OLED

int tela_monitor_sub_estado = 0; // controla qual subtela de monitoramento é exibida (0:Temp, 1:Umid, 2:Pressão, 3:Altitude)
int tela_limites_sub_estado = 0; // controla qual subtela de limites é exibida (0:Temp, 1:Umid, 2:Pressão, 3:IP)

// --- Páginas HTML ---
// string C contendo o código HTML para a página principal (dashboard)
const char HTML_PAGE[] =
    "<!DOCTYPE html>\n"
    "<html lang=\"pt-br\">\n"
    "<head>\n"
    "    <meta charset=\"UTF-8\">\n"
    "    <meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
    "    <title>Estação Meteorológica</title>\n"
    "    <script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>\n"
    "    <style>\n"
    "        body{font-family:sans-serif;background-color:#f0f2f5;display:flex;flex-direction:column;align-items:center;padding:20px}\n"
    "        h1{color:#333}\n"
    "        .container{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:20px;width:100%;max-width:1400px}\n"
    "        .data-block{background-color:#fff;border-radius:10px;box-shadow:0 4px 6px rgba(0,0,0,.1);padding:20px;text-align:center}\n"
    "        h2{margin-top:0;font-size:1.1em;color:#6c757d}\n"
    "        .value{font-size:2em;font-weight:700;color:#333}\n"
    "        .alert-banner{display:none;width:100%;max-width:1000px;background-color:#dc3545;color:#fff;padding:10px;border-radius:8px;text-align:center;font-weight:700;margin-bottom:20px}\n"
    "        .nav{margin-bottom:20px;font-size:1.2em}\n"
    "    </style>\n"
    "</head>\n"
    "<body>\n"
    "    <h1>Estação Meteorológica</h1>\n"
    "    <div class=\"nav\"><a href=\"/settings\">Configurar Limites</a></div>\n"
    "    <div id=\"alert_msg\" class=\"alert-banner\">ALERTA DE LIMITE!</div>\n"
    "    <div class=\"container\">\n"
    "        <div class=\"data-block\"><h2>Temperatura</h2><p class=\"value\"><span id=\"temp_val\">--</span>&deg;C</p><canvas id=\"tempChart\"></canvas></div>\n"
    "        <div class=\"data-block\"><h2>Umidade</h2><p class=\"value\"><span id=\"umid_val\">--</span>%</p><canvas id=\"humidChart\"></canvas></div>\n"
    "        <div class=\"data-block\"><h2>Pressão</h2><p class=\"value\"><span id=\"press_val\">--</span> hPa</p><canvas id=\"pressChart\"></canvas></div>\n"
    "        <div class=\"data-block\"><h2>Altitude</h2><p class=\"value\"><span id=\"alt_val\">--</span> m</p><canvas id=\"altChart\"></canvas></div>\n"
    "    </div>\n"
    "    <script>\n"
    "        const charts={};\n"
    "        function createChart(e,a,t,l,r,n){const o=document.getElementById(e).getContext(\"2d\");charts[e]=new Chart(o,{type:\"line\",data:{labels:[],datasets:[{label:a,data:[],borderColor:t,tension:.2,fill:!0,backgroundColor:l}]},options:{scales:{y:{suggestedMin:r,suggestedMax:n}}}})}\n"
    "        createChart(\"tempChart\",\"Temperatura (C)\",\"rgb(255,99,132)\",\"rgba(255,99,132,0.1)\",10,40);\n"
    "        createChart(\"humidChart\",\"Umidade (%)\",\"rgb(54,162,235)\",\"rgba(54,162,235,0.1)\",0,100);\n"
    "        createChart(\"pressChart\",\"Pressão (hPa)\",\"rgb(75,192,192)\",\"rgba(75,192,192,0.1)\",980,1030);\n"
    "        createChart(\"altChart\",\"Altitude (m)\",\"rgb(255,159,64)\",\"rgba(255,159,64,0.1)\",-50,250);\n"
    "        function fetchData(){fetch(\"/data\").then(e=>e.json()).then(e=>{document.getElementById(\"temp_val\").innerText=e.temp.toFixed(1);document.getElementById(\"umid_val\").innerText=e.hum.toFixed(1);document.getElementById(\"press_val\").innerText=e.press.toFixed(0);document.getElementById(\"alt_val\").innerText=e.alt.toFixed(0);document.getElementById(\"alert_msg\").style.display=e.alerta?\"block\":\"none\";const a=new Date,t=a.getHours()+\":\"+(\"0\"+a.getMinutes()).slice(-2)+\":\"+(\"0\"+a.getSeconds()).slice(-2);Object.values(charts).forEach(e=>{if(e.data.labels.length>15){e.data.labels.shift();e.data.datasets[0].data.shift()}e.data.labels.push(t)});charts.tempChart.data.datasets[0].data.push(e.temp);charts.humidChart.data.datasets[0].data.push(e.hum);charts.pressChart.data.datasets[0].data.push(e.press);charts.altChart.data.datasets[0].data.push(e.alt);Object.values(charts).forEach(e=>{e.update()})})}\n"
    "        fetchData();setInterval(fetchData,2000);\n"
    "    </script>\n"
    "</body>\n"
    "</html>\n";

// string C contendo o código HTML para a página de configurações
const char HTML_PAGE_SETTINGS_FORMAT[] =
    "<!DOCTYPE html>\n"
    "<html lang=\"pt-br\">\n"
    "<head>\n"
    "    <meta charset=\"UTF-8\">\n"
    "    <meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
    "    <title>Configurações</title>\n"
    "    <style>\n"
    "        body{font-family:sans-serif;background-color:#f0f2f5;display:flex;flex-direction:column;align-items:center;padding:20px}\n"
    "        h1{color:#333}\n"
    "        form{background-color:#fff;padding:30px;border-radius:10px;box-shadow:0 4px 6px rgba(0,0,0,.1);width:100%%;max-width:500px}\n"
    "        .form-group{margin-bottom:20px}\n"
    "        label{display:block;margin-bottom:5px;font-weight:700;color:#555}\n"
    "        input[type=number]{width:100%%;box-sizing:border-box;padding:10px;border:1px solid #ccc;border-radius:5px}\n"
    "        input[type=submit]{background-color:#007bff;color:#fff;padding:12px 20px;border:none;border-radius:5px;cursor:pointer;font-size:1em;width:100%%}\n"
    "        a{display:inline-block;margin-top:20px;color:#007bff}\n"
    "    </style>\n"
    "</head>\n"
    "<body>\n"
    "    <h1>Configurar Limites de Alerta</h1>\n"
    "    <form action=\"/settings\" method=\"get\">\n"
    "        <div class=\"form-group\"><label for=\"temp_min\">Temp. Mínima (°C):</label><input type=\"number\" id=\"temp_min\" name=\"temp_min\" step=\"0.1\" value=\"%.1f\"></div>\n"
    "        <div class=\"form-group\"><label for=\"temp_max\">Temp. Máxima (°C):</label><input type=\"number\" id=\"temp_max\" name=\"temp_max\" step=\"0.1\" value=\"%.1f\"></div>\n"
    "        <div class=\"form-group\"><label for=\"umid_min\">Umidade Mínima (%%):</label><input type=\"number\" id=\"umid_min\" name=\"umid_min\" step=\"1\" value=\"%.0f\"></div>\n"
    "        <div class=\"form-group\"><label for=\"umid_max\">Umidade Máxima (%%):</label><input type=\"number\" id=\"umid_max\" name=\"umid_max\" step=\"1\" value=\"%.0f\"></div>\n"
    "        <div class=\"form-group\"><label for=\"press_min\">Pressão Mínima (hPa):</label><input type=\"number\" id=\"press_min\" name=\"press_min\" step=\"1\" value=\"%.0f\"></div>\n"
    "        <div class=\"form-group\"><label for=\"press_max\">Pressão Máxima (hPa):</label><input type=\"number\" id=\"press_max\" name=\"press_max\" step=\"1\" value=\"%.0f\"></div>\n"
    "        <input type=\"submit\" value=\"Salvar Configurações\">\n"
    "    </form>\n"
    "    <a href=\"/\">Voltar à Página Principal</a>\n"
    "</body>\n"
    "</html>\n";

// --- Funções Auxiliares de Periféricos ---
// função para enviar um pixel para a matriz de LEDs WS2812 via PIO
static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

// função para converter componentes R, G, B em um único valor de 32 bits
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

// inicializa os pinos GPIO para o LED RGB
void init_led_rgb() {
    gpio_init(LED_R);                       // inicializa o pino do LED vermelho
    gpio_set_dir(LED_R, GPIO_OUT);          // define o pino como saída
    gpio_init(LED_G);                       // inicializa o pino do LED verde
    gpio_set_dir(LED_G, GPIO_OUT);          // define o pino como saída
    gpio_init(LED_B);                       // inicializa o pino do LED azul
    gpio_set_dir(LED_B, GPIO_OUT);          // define o pino como saída
}

// acende o LED RGB em vermelho (alerta) ou verde (normal)
void set_led_rgb(bool alerta) {
    gpio_put(LED_R, alerta);                // acende o vermelho se houver alerta
    gpio_put(LED_G, !alerta);               // acende o verde se não houver alerta
    gpio_put(LED_B, 0);                     // mantém o azul desligado
}

// inicializa o pino do buzzer usando PWM para controle de volume
void init_buzzer() {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM); // define a função do pino como PWM
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN); // obtém a fatia de PWM correspondente ao pino
    pwm_set_wrap(slice_num, 4095);          // define o valor máximo do contador de PWM (resolução)
    pwm_set_clkdiv(slice_num, 250);         // define o divisor de clock para ajustar a frequência
    pwm_set_enabled(slice_num, true);       // habilita o canal de PWM
    pwm_set_gpio_level(BUZZER_PIN, 0);      // desliga o buzzer inicialmente
}

// ativa ou desativa o buzzer com base no estado de alerta
void set_buzzer(bool alerta) {
    // define o nível do PWM para 50% do ciclo (2048) se houver alerta, ou 0% caso contrário
    pwm_set_gpio_level(BUZZER_PIN, alerta ? 2048 : 0);
}

// atualiza a matriz de LEDs WS2812 para mostrar um indicador de nível
void set_matriz_indicador(float valor, float min, float max) {
    // calcula a porcentagem do valor atual em relação aos limites min e max
    float percentual = 100.0 * (valor - min) / (max - min);
    if (percentual < 0) percentual = 0;
    if (percentual > 100) percentual = 100;

    // converte a porcentagem em um número de linhas a serem acesas (0 a 5)
    int linhas_acesas = (int)(percentual / 20.0f);
    if (linhas_acesas < 0) linhas_acesas = 0;
    if (linhas_acesas > 5) linhas_acesas = 5;

    uint32_t pixels[25] = {0};              // array para armazenar a cor de cada um dos 25 pixels
    // mapa que corresponde ao índice do pixel físico na matriz
    int pixel_map[5][5] = {
        {24, 23, 22, 21, 20}, 
        {15, 16, 17, 18, 19}, 
        {14, 13, 12, 11, 10},
        {5,  6,  7,  8,  9}, 
        {4,  3,  2,  1,  0}
    };

    // acende as linhas correspondentes ao indicador de nível em azul
    for (int i = 4; i >= (4 - linhas_acesas + 1); i--) {
        for (int j = 0; j < 5; j++) {
            pixels[pixel_map[i][j]] = urgb_u32(0, 0, 8); // cor azul fraca
        }
    }
    // se o alerta estiver ativo, acende a primeira linha em vermelho
    if (alerta_ativo) {
        for (int j = 0; j < 5; j++) {
            pixels[pixel_map[0][j]] = urgb_u32(20, 0, 0); // cor vermelha
        }
    }
    // envia as cores para todos os 25 pixels da matriz
    for (int i = 0; i < 25; i++) {
        put_pixel(pixels[i]);
    }
}

// --- Funções de Display OLED e Interrupções ---
// desenha a tela do menu principal no display OLED
void draw_menu_principal(ssd1306_t *ssd) {
    ssd1306_fill(ssd, false);               // limpa o buffer do display
    ssd1306_draw_string(ssd, "Estacao", 26, 0);
    ssd1306_draw_string(ssd, "Meteorologica", 12, 12);
    ssd1306_line(ssd, 0, 24, 127, 24, true); // desenha uma linha separadora
    ssd1306_draw_string(ssd, "A: Monitorar", 4, 36);
    ssd1306_draw_string(ssd, "B: Limites/IP", 4, 50);
}

// desenha as telas de monitoramento de dados no display OLED
void draw_tela_monitoramento(ssd1306_t *ssd) {
    ssd1306_fill(ssd, false);               // limpa o buffer do display
    char buffer[20];                        // buffer temporário para formatar strings

    // usa um switch para decidir qual subtela mostrar com base na variável de estado
    switch (tela_monitor_sub_estado) {
        case 0: // Temperatura
            ssd1306_draw_string(ssd, "Temperatura:", 20, 4);
            snprintf(buffer, sizeof(buffer), "%.1f C", temperatura_bmp);
            ssd1306_draw_string(ssd, buffer, 38, 20);
            break;
        case 1: // Umidade
            ssd1306_draw_string(ssd, "Umidade:", 32, 4);
            snprintf(buffer, sizeof(buffer), "%.1f%%", umidade_aht);
            ssd1306_draw_string(ssd, buffer, 38, 20);
            break;
        case 2: // Pressão
            ssd1306_draw_string(ssd, "Pressao:", 32, 4);
            snprintf(buffer, sizeof(buffer), "%.0f hPa", pressao_bmp);
            ssd1306_draw_string(ssd, buffer, 30, 20);
            break;
        case 3: // Altitude
            ssd1306_draw_string(ssd, "Altitude:", 28, 4);
            snprintf(buffer, sizeof(buffer), "%.0fm", altitude_bmp);
            ssd1306_draw_string(ssd, buffer, 42, 20);
            break;
    }
    // exibe o status de alerta em todas as subtelas de monitoramento
    ssd1306_draw_string(ssd, alerta_ativo ? "ALERTA!" : "Normal", 38, 52);
}

// desenha as telas de limites e IP no display OLED
void draw_tela_limites(ssd1306_t *ssd) {
    ssd1306_fill(ssd, false);               // limpa o buffer do display
    char buffer[25];                        // buffers temporários para formatar strings
    char buffer2[25];

    // usa um switch para decidir qual subtela mostrar
    switch (tela_limites_sub_estado) {
        case 0: // Limites de Temperatura
            ssd1306_draw_string(ssd, "Limites Temp:", 4, 4);
            snprintf(buffer, sizeof(buffer), "Min: %.1f C", temp_lim_min);
            ssd1306_draw_string(ssd, buffer, 4, 28);
            snprintf(buffer2, sizeof(buffer2), "Max: %.1f C", temp_lim_max);
            ssd1306_draw_string(ssd, buffer2, 4, 44);
            break;
        case 1: // Limites de Umidade
            ssd1306_draw_string(ssd, "Limites Umid:", 4, 4);
            snprintf(buffer, sizeof(buffer), "Min: %.0f%%", umid_lim_min);
            ssd1306_draw_string(ssd, buffer, 4, 28);
            snprintf(buffer2, sizeof(buffer2), "Max: %.0f%%", umid_lim_max);
            ssd1306_draw_string(ssd, buffer2, 4, 44);
            break;
        case 2: // Limites de Pressão
            ssd1306_draw_string(ssd, "Limites Press:", 4, 4);
            snprintf(buffer, sizeof(buffer), "Min: %.0f hPa", press_lim_min);
            ssd1306_draw_string(ssd, buffer, 4, 28);
            snprintf(buffer2, sizeof(buffer2), "Max: %.0f hPa", press_lim_max);
            ssd1306_draw_string(ssd, buffer2, 4, 44);
            break;
        case 3: // IP de Conexão
            ssd1306_draw_string(ssd, "IP p/ Conexao:", 4, 16);
            ssd1306_draw_string(ssd, ip_str, 4, 40);
            break;
    }
}

// função central que decide qual tela desenhar e envia para o display
void update_display(ssd1306_t *ssd) {
    switch (estado_menu) {
        case MENU_PRINCIPAL:
            draw_menu_principal(ssd);
            break;
        case TELA_MONITORAMENTO:
            draw_tela_monitoramento(ssd);
            break;
        case TELA_LIMITES:
            draw_tela_limites(ssd);
            break;
    }
    ssd1306_send_data(ssd); // envia o conteúdo do buffer para o hardware do display
}

// função de callback para tratar interrupções dos botões
void gpio_irq_handler(uint gpio, uint32_t events) {
    static uint32_t last_press = 0;          // armazena o tempo do último clique para debounce
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    if (current_time - last_press < 250) return; // ignora cliques muito rápidos (debounce)
    last_press = current_time;

    if (gpio == BOTAO_A) {
        if (estado_menu == MENU_PRINCIPAL) {
            estado_menu = TELA_MONITORAMENTO; // se no menu, vai para a tela de monitoramento
            tela_monitor_sub_estado = 0;      // reseta para a primeira subtela (temperatura)
        } else if (estado_menu == TELA_MONITORAMENTO) {
            tela_monitor_sub_estado = (tela_monitor_sub_estado + 1) % 4; // cicla entre as subtelas
        }
    } else if (gpio == BOTAO_B) {
        if (estado_menu == MENU_PRINCIPAL) {
            estado_menu = TELA_LIMITES;       // se no menu, vai para a tela de limites
            tela_limites_sub_estado = 0;      // reseta para a primeira subtela (limites de temp)
        } else if (estado_menu == TELA_LIMITES) {
            tela_limites_sub_estado = (tela_limites_sub_estado + 1) % 4; // cicla entre as subtelas
        }
    } else if (gpio == JOYSTICK_SW) {
        estado_menu = MENU_PRINCIPAL;         // botão do joystick sempre retorna ao menu principal
    }
}

// --- LÓGICA DO WEBSERVER ---
// estrutura para manter o estado da resposta HTTP
struct http_state { char response[4096]; size_t len; size_t sent; };

// callback chamado quando os dados TCP são enviados com sucesso
static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;
    if (hs->sent >= hs->len) {
        tcp_close(tpcb);                      // fecha a conexão TCP
        free(hs);                             // libera a memória da estrutura de estado
    }
    return ERR_OK;
}

// função para analisar a URL da requisição e atualizar os valores de limite
void parse_and_update_value(const char* request, const char* key, float* value) {
    char* key_ptr = strstr(request, key);     // procura a chave (ex: "temp_min=") na string da requisição
    if (key_ptr) {
        float new_val = atof(key_ptr + strlen(key)); // converte o número após a chave para float
        *value = new_val;                     // atualiza a variável global correspondente
    }
}

// callback chamado quando dados TCP são recebidos (uma requisição HTTP)
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }
    char *req = (char *)p->payload;           // ponteiro para os dados da requisição
    struct http_state *hs = malloc(sizeof(struct http_state));
    hs->sent = 0;

    // Roteamento: decide o que fazer com base na URL da requisição
    if (strncmp(req, "GET /data", 9) == 0) { // se a requisição é para /data
        char json_payload[256];
        // cria uma string JSON com os dados atuais dos sensores
        int json_len = snprintf(json_payload, sizeof(json_payload),
                                "{\"temp\":%.2f, \"hum\":%.2f, \"press\":%.2f, \"alt\":%.2f, \"alerta\":%s}",
                                temperatura_bmp, umidade_aht, pressao_bmp, altitude_bmp, alerta_ativo ? "true" : "false");

        // monta a resposta HTTP com o cabeçalho de JSON
        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n%s",
                           json_len, json_payload);

    } else if (strncmp(req, "GET /settings", 13) == 0) { // se a requisição é para /settings
        if (strstr(req, "?")) {               // se a URL contém '?', indica um envio de formulário
            // chama a função de parse para cada um dos 6 limites
            parse_and_update_value(req, "temp_min=", &temp_lim_min);
            parse_and_update_value(req, "temp_max=", &temp_lim_max);
            parse_and_update_value(req, "umid_min=", &umid_lim_min);
            parse_and_update_value(req, "umid_max=", &umid_lim_max);
            parse_and_update_value(req, "press_min=", &press_lim_min);
            parse_and_update_value(req, "press_max=", &press_lim_max);
            printf("Limites atualizados via web!\n");
            
            // envia uma resposta de redirecionamento para o navegador voltar à página principal
            hs->len = snprintf(hs->response, sizeof(hs->response), "HTTP/1.1 302 Found\r\nLocation: /\r\n\r\n");
        } else {                              // se não tem '?', apenas exibe a página de configurações
            char* page_buffer = hs->response + 1024; // usa um buffer temporário para a página
            // monta a página de configurações, inserindo os valores atuais nos campos do formulário
            int page_len = snprintf(page_buffer, sizeof(hs->response) - 1024, HTML_PAGE_SETTINGS_FORMAT,
                                    temp_lim_min, temp_lim_max, umid_lim_min, umid_lim_max, press_lim_min, press_lim_max);

            // monta a resposta HTTP com o cabeçalho de HTML
            hs->len = snprintf(hs->response, sizeof(hs->response),
                               "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n%s",
                               page_len, page_buffer);
        }
    } else { // para qualquer outra requisição (ex: "/"), serve a página principal
        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n%s",
                           (int)strlen(HTML_PAGE), HTML_PAGE);
    }

    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);                // define a função de callback para quando os dados são enviados
    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY); // enfileira a resposta para ser enviada
    tcp_output(tpcb);                         // envia os dados enfileirados
    pbuf_free(p);                             // libera o buffer da requisição
    return ERR_OK;
}

// callback chamado quando uma nova conexão TCP é estabelecida
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_recv);              // define a função de callback para quando dados são recebidos
    return ERR_OK;
}

// inicializa e inicia o servidor HTTP na porta 80
static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb || tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
        return;
    }
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);     // define a função de callback para novas conexões
}

// --- Função Principal (main) ---
int main() {                                  // ponto de entrada do programa
    stdio_init_all();                         // inicializa a comunicação serial para o printf
    sleep_ms(2000);                           // aguarda 2 segundos para estabilizar
    printf("Iniciando Estacao Meteorologica ...\n");

    // inicializa o módulo Wi-Fi
    if (cyw43_arch_init()) {
        printf("Falha ao inicializar o modulo Wi-Fi\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();             // habilita o modo "station" (cliente Wi-Fi)
    printf("Conectando ao Wi-Fi...\n");
    // tenta conectar à rede Wi-Fi com um timeout de 30 segundos
    if (cyw43_arch_wifi_connect_timeout_ms("Apartamento 01", "12345678", CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha na conexao Wi-Fi\n");
    } else {
        printf("Conectado ao Wi-Fi\n");
        // obtém e exibe o endereço IP
        uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
        snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        printf("IP: %s\n", ip_str);
    }
    
    // inicialização do I2C e do display
    i2c_init(I2C_PORT_DISP, 400 * 1000);
    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISP);
    gpio_pull_up(I2C_SCL_DISP);
    
    ssd1306_t ssd;
    ssd1306_init(&ssd, 128, 64, false, ENDERECO, I2C_PORT_DISP);
    ssd1306_config(&ssd);
    
    // inicialização dos botões
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    gpio_init(JOYSTICK_SW);
    gpio_set_dir(JOYSTICK_SW, GPIO_IN);
    gpio_pull_up(JOYSTICK_SW);

    // configura as interrupções para os botões
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(JOYSTICK_SW, GPIO_IRQ_EDGE_FALL, true);
    
    // inicialização do I2C e dos sensores
    printf("Inicializando I2C para sensores...\n");
    i2c_init(I2C_PORT_SENSORES, 100000);
    gpio_set_function(I2C_SDA_SENSORES, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_SENSORES, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_SENSORES);
    gpio_pull_up(I2C_SCL_SENSORES);

    struct bmp280_calib_param bmp280_params;
    bmp280_init(I2C_PORT_SENSORES);
    bmp280_get_calib_params(I2C_PORT_SENSORES, &bmp280_params);

    aht20_init(I2C_PORT_SENSORES);
    
    // inicialização da matriz de LEDs WS2812 via PIO
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, 0, offset, WS2812_PIN, 800000, false);
    
    // inicializa os periféricos restantes
    init_led_rgb();
    init_buzzer();
    start_http_server();
    printf("Sistema pronto.\n");

    int32_t raw_temp, raw_press;
    AHT20_Data aht20_data;

    // loop principal infinito
    while (true) {
        cyw43_arch_poll(); // processa eventos de rede (essencial para o servidor web funcionar)
        
        // --- LEITURA E PROCESSAMENTO DOS SENSORES ---
        bmp280_read_raw(I2C_PORT_SENSORES, &raw_temp, &raw_press);
        temperatura_bmp = bmp280_convert_temp(raw_temp, &bmp280_params) / 100.0f;
        pressao_bmp = bmp280_convert_pressure(raw_press, raw_temp, &bmp280_params) / 100.0f;

        if (aht20_read(I2C_PORT_SENSORES, &aht20_data)) {
            umidade_aht = aht20_data.humidity;
        }

        // calcula a altitude com base na pressão atmosférica
        altitude_bmp = 44330.0 * (1.0 - pow(pressao_bmp / 1013.25, 0.1903));
        
        // --- LÓGICA DE ALERTA ---
        // verifica se alguma das leituras está fora dos limites configurados
        alerta_ativo = (temperatura_bmp > temp_lim_max || temperatura_bmp < temp_lim_min ||
                        umidade_aht > umid_lim_max     || umidade_aht < umid_lim_min ||
                        pressao_bmp > press_lim_max    || pressao_bmp < press_lim_min);
                        
        // --- ATUALIZAÇÃO DOS PERIFÉRICOS ---
        set_led_rgb(alerta_ativo);                  // atualiza o LED RGB de status
        set_buzzer(alerta_ativo);                   // atualiza o buzzer de alerta
        set_matriz_indicador(temperatura_bmp, 10.0, 40.0); // atualiza o indicador de nível da matriz
        update_display(&ssd);                       // atualiza as informações no display OLED
        
        sleep_ms(500);                              // aguarda 500ms antes da próxima iteração do loop
    }
    return 0; // fim do programa
}