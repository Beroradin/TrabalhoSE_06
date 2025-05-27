# ✨ Painel de Controle de Acesso por meio do freeRTOS e Semaforos
<p align="center"> Repositório dedicado ao sistema de controle de acesso interativo utilizando a placa BitDogLab com RP2040, que controla o número de usuários simultâneos em um espaço físico, utilizando semáforos de contagem, semáforos binários e mutex para sincronização entre tarefas.</p>

## :clipboard: Apresentação da tarefa

Para este trabalho foi necessário implementar um painel de controle interativo simulando o controle de acesso de usuários a um determinado espaço (nesse caso, uma balada). O sistema utiliza semáforos de contagem para controlar o número de usuários simultâneos, semáforo binário com interrupção para resetar o sistema e mutex para proteger o acesso ao display OLED. O sistema oferece feedback visual através do LED RGB, sinalização sonora com o buzzer e exibição de mensagens no display OLED.

## :dart: Objetivos

- Implementar um sistema de controle de acesso com capacidade máxima definida (USUARIOS_FULL)
- Utilizar semáforo de contagem para controlar o número de usuários simultâneos
- Implementar semáforo binário com interrupção para reset do sistema
- Proteger o acesso ao display OLED com mutex
- Exibir dados em tempo real no display OLED SSD1306
- Controlar o LED RGB indicando a ocupação (azul: vazio, verde: disponível, amarelo: quase cheio, vermelho: lotado)
- Acionar um buzzer quando tentativa de entrada com sistema cheio e ao resetar (beep único e beep duplo)
- Implementar um sistema de tempo real usando FreeRTOS para gerenciar todas as tarefas

## :books: Descrição do Projeto
Utilizou-se a placa BitDogLab com o microcontrolador RP2040 para criar um sistema completo de controle de acesso. O sistema monitora a entrada e saída de usuários através dos botões A e B respectivamente. O botão A registra a entrada de um usuário (se houver vaga disponível), o botão B registra a saída de um usuário e o botão do joystick reseta todo o sistema.

O sistema utiliza um semáforo de contagem inicializado com 9 vagas para controlar o acesso simultâneo. Quando um usuário tenta entrar, o sistema verifica se há vagas disponíveis através do semáforo. Se não houver, emite um beep de alerta. O reset é controlado por um semáforo binário acionado por interrupção, garantindo resposta imediata. O acesso ao display OLED é protegido por mutex para evitar corrupção de dados entre tarefas.

O sistema é gerenciado pelo sistema operacional de tempo real FreeRTOS, que divide o processamento em tarefas concorrentes, permitindo operação multitarefa.

## :walking: Integrantes do Projeto
- Matheus Pereira Alves

## :bookmark_tabs: Funcionamento do Projeto

O sistema é dividido em cinco tarefas principais do FreeRTOS:

- vTaskEntrada: Monitora o botão A para registrar entrada de usuários, utilizando semáforo de contagem
- vTaskSaida: Monitora o botão B para registrar saída de usuários, liberando vagas no semáforo de contagem
- vTaskReset: Aguarda semáforo binário (acionado por interrupção) para resetar o sistema com beep duplo
- vLEDRGBTask: Controla o LED RGB conforme ocupação (azul: vazio, verde: disponível, amarelo: quase cheio, vermelho: lotado)
- vDisplayOLEDTask: Atualiza o display OLED com informações do sistema, protegido por mutex

## :eyes: Observações

- A comunicação entre tarefas utiliza variáveis globais protegidas por mecanismos de sincronização;
- As tarefas operam em diferentes prioridades: reset (mais alta), entrada/saída (média), display/LED (mais baixa);
- O semáforo binário para reset é acionado por interrupção, garantindo resposta imediata;
- De preferência, utilize o PICO SDK versão 2.1.0;
- Lembre-se de anexar o seu diretório do FreeRTOS;
- Foram utilizados xSemaphoreCreateCounting(), xSemaphoreCreateBinary(), xSemaphoreCreateMutex(), xSemaphoreTake() e xSemaphoreGive().

## :camera: GIF mostrando o funcionamento do programa na placa Raspberry Pi Pico
<p align="center">
  <img src="images/trabalhose06.gif" alt="GIF" width="526px" />
</p>

## :arrow_forward: Vídeo no youtube mostrando o funcionamento do programa na placa Raspberry Pi Pico
<p align="center">
    <a href="https://youtu.be/WDqr31tVhzg">Clique aqui para acessar o vídeo</a>
</p>
