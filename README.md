<img width=100% src="https://capsule-render.vercel.app/api?type=waving&color=02A6F4&height=120&section=header"/>
<h1 align="center">Embarcatech - Projeto Integrado - BitDogLab </h1>

## Objetivo do Projeto

Uma estação meteorológica completa e interativa utilizando o Raspberry Pi Pico W na placa BitDogLab. O sistema captura dados de temperatura, umidade e pressão atmosférica em tempo real através dos sensores I2C AHT20 e BMP280. As informações são exibidas localmente em um display OLED e, principalmente, em um dashboard web responsivo servido pela própria placa. A interface web apresenta gráficos dinâmicos, permite a configuração remota de limites de alerta e exibe um log de eventos, tudo atualizado via AJAX/JSON, simulando uma central de monitoramento climático inteligente.

## 🗒️ Lista de requisitos

- **Leitura de Sensores I2C:** Capturar dados contínuos de temperatura e umidade do sensor AHT20 e de temperatura e pressão barométrica do sensor BMP280;
- **Utilização da Matriz de LEDs:** Representar visualmente a temperatura ambiente e sinalizar estados de alerta;
- **Utilização de LED RGB:** Fornecer um status visual imediato do sistema (Normal vs. Alerta);
- **Display OLED (SSD1306):** Exibir um menu local com os dados dos sensores, status da conexão e os limites configurados;
- **Utilização do Buzzer:** Emitir um alarme sonoro quando os valores dos sensores ultrapassarem os limites pré-definidos;
- **Webserver:** Servir uma página HTML via Wi-Fi com um dashboard completo;
- **Dashboard Interativo:** Servir uma página HTML via Wi-Fi com um dashboard completo;
  - Exibir os 4 principais dados (Temperatura, Umidade, Pressão, Altitude) em cards.
  - Mostrar gráficos de linha individuais para cada medição, atualizados em tempo real.
  - Permitir a configuração de limites de alerta (Mín/Máx) para os sensores.
- **Leitura de Botões:** Utilizar os botões da placa para navegação no menu do display OLED, com tratamento de debounce via interrupção.;
- **Estruturação do projeto:** Código em C no VS Code, usando Pico SDK e lwIP, com estrutura clara e comentários;
  

## 🛠 Tecnologias

1. **Microcontrolador:** Raspberry Pi Pico W (na BitDogLab).
2. **Sensores I2C:** AHT20 (Temperatura e Umidade) e BMP280 (Pressão e Temperatura), conectados via I2C0 (GPIO 0 e 1).
3. **Display OLED SSD1306:** 128x64 pixels, conectado via I2C (GPIO 14 - SDA, GPIO 15 - SCL).
4. **Botões:** Botão A (GPIO 5), Botão B (GPIO 6) e Botão do Joystick (GPIO 22) para navegação no menu local.
5. **Matriz de LEDs:** WS2812 (GPIO 7).
6. **LED RGB:** GPIOs 11 (verde), 12 (azul), 13 (vermelho).
7. **Buzzer:** GPIO 10.
8. **Linguagem de Programação:** C.
9. **Frameworks:** Pico SDK, lwIP (para o Webserver).
13. **Tecnologias Web:** Tecnologias Web: HTML, CSS, JavaScript, AJAX, JSON.
14. **Biblioteca de Gráficos:** Chart.js.


## 🔧 Funcionalidades Implementadas:

**Funções dos Componentes**

- **Interface Web (Dashboard)**
  
  - **Visualização de Dados** Apresenta 4 cards principais com os valores de Temperatura, Umidade, Pressão e Altitude, atualizados a cada 2 segundos.
  - **Gráficos em Tempo Real:** Cada card possui um gráfico de linha individual que plota o histórico recente da medição correspondente.
  - **Alerta Visual:** Uma faixa vermelha de "ALERTA DE LIMITE!" aparece no topo da página sempre que um dos sensores excede os limites configurados.
  - **Configuração Remota:** Através de um link na página principal, o usuário acessa uma página de configurações dedicada onde pode ajustar os valores mínimos e máximos para os alertas de temperatura, umidade e pressão.

  
- **Interface Local (Hardware na BitDogLab)**
  
  - **Display OLED:**
    - **Menu Principal:** Permite a navegação para as telas de Monitoramento e Limites.
    - **Tela de Monitoramento:** Exibe todos os dados dos sensores em tempo real.
    - **Tela de Limites:** Mostra os limites de alerta atuais, que podem ser ajustados dinamicamente pelo código e pela interface web.
      
  - **Sistema de Alertas Físico::**
    - **Buzzer:** Emite um alarme sonoro contínuo enquanto o sistema estiver em estado de alerta.
    - **LED RGB:** Fica verde em operação normal e muda para vermelho durante um alerta.
    - **Matriz de LEDs:** Funciona como um "termômetro de barras" visual e a fileira superior acende em vermelho para reforçar o sinal de alerta.

  - **Botões:** Os botões A, B e do Joystick são usados para navegar entre as telas do display OLED, permitindo o monitoramento local sem depender da interface web.



## 🚀 Passos para Compilação e Upload do Projeto

1. **Instale o Ambiente**:
- Configure o ambiente de desenvolvimento para o Raspberry Pi Pico W com o Pico SDK e as ferramentas necessárias (CMake, Ninja, etc)
- Configure o Wi-Fi:
  - No arquivo Estacao_Meteorologica.c, altere o nome da rede (SSID) e a senha nas seguintes linhas:
    - if (cyw43_arch_wifi_connect_timeout_ms("SEU_WIFI", "SUA_SENHA", ...)) {
  
2. **Compile o Código**: No terminal, dentro da pasta do projeto, execute os seguintes comandos:
   
   ```bash
   mkdir build
   cd build
   cmake ..
   make

3. **Transferir o firmware para a placa:**

- Conectar a placa BitDogLab ao computador via USB segurando o botão "BOOTSEL".
- Copiar o arquivo .uf2 gerado para o drive da placa.

4. **Testar o projeto**

- Após o upload, a placa irá reiniciar. Abra um monitor serial (como o do VS Code) para ver as mensagens de inicialização e o endereço IP da placa.
- Acesse o endereço IP em um navegador na mesma rede Wi-Fi para visualizar o dashboard/webserver.

🛠🔧🛠🔧🛠🔧


## 🎥 Demonstração: 

- Para ver o funcionamento do projeto, acesse o vídeo de demonstração gravado por José Vinicius em: https://youtu.be/bjlL0B2PEMk

## 💻 Desenvolvedor
 
<table>
  <tr>
    <td align="center"><img style="" src="https://avatars.githubusercontent.com/u/191687774?v=4" width="100px;" alt=""/><br /><sub><b> José Vinicius </b></sub></a><br />👨‍💻</a></td>
  </tr>
</table>
