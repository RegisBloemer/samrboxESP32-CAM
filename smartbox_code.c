/*
Universidade Federal de Santa Catarina 
Curso: Engenharia de Computação 
Disciplina: Redes sem Fio 
Professora: Analucia Schiaffino Morales
Resumo: O código abaixo serve para configurar a ESP32-CAM e para que ela tire uma foto toda vez que o sensor de fim de curso estiver levantado, além disso, com a 
foto salva no SPIFFS(Sistema de arquivos da ESP32) a imagem é enviada para o storage do Google Firebase que é configurado também aqui é configurado abaixo.

Tutoriais usados: 
limit-switch: https://esp32io.com/tutorials/esp32-limit-switch
button-library: https://arduinogetstarted.com/tutorials/arduino-button-library
Firebase with ESP32-CAM : https://randomnerdtutorials.com/esp32-cam-save-picture-firebase-storage/
*/

#include "WiFi.h"   // Inclui a biblioteca para conexão com WiFi
#include "esp_camera.h"  // Inclui a biblioteca para usar a câmera do ESP32
#include "Arduino.h"  // Inclui a biblioteca padrão do Arduino
#include "soc/soc.h"  // Biblioteca necessária para desabilitar problemas de 'brownout'
#include "soc/rtc_cntl_reg.h"  // Outra biblioteca necessária para desabilitar problemas de 'brownout'
#include "driver/rtc_io.h"  
#include <SPIFFS.h>  // Inclui a biblioteca para lidar com o sistema de arquivos SPIFFS
#include <FS.h>  // Inclui a biblioteca para lidar com sistema de arquivos
#include <Firebase_ESP_Client.h>  // Inclui a biblioteca cliente do Firebase para ESP
#include <addons/TokenHelper.h>  // Inclui a biblioteca auxiliar para lidar com tokens do Firebase
#include <TimeLib.h>  // Inclui a biblioteca para lidar com o tempo
#include <ezButton.h>  // Inclui a biblioteca para facilitar a manipulação de botões

ezButton FIM_DE_CURSO_PIN(4);  // Define o pino 4 como o pino do sensor de fim de curso

const char* ssid = "nome do seu wifi";  // Define o SSID do WiFi que será usado
const char* password = "senha do seu wifi";  // Define a senha do WiFi que será usado

#define API_KEY "API KEY disponibilizada pelo Firebase"  // Chave da API do Firebase

#define USER_EMAIL "email do usuário e permitido a mandar dados para o firebase."  // Email do usuário permitido para enviar dados ao Firebase
#define USER_PASSWORD "senha do usuário permitido a mandar dados para o firebase."  // Senha do usuário permitido para enviar dados ao Firebase

#define STORAGE_BUCKET_ID "Firebase storage bucket ID é fornecido pelo próprio firebase também."  // ID do bucket de armazenamento do Firebase

#define FILE_PHOTO "/data/photo.jpg"  // Nome do arquivo da foto para salvar no SPIFFS

// Definição dos pinos para o módulo de câmera OV2640
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

FirebaseData fbdo;  // Define o objeto para dados do Firebase
FirebaseAuth auth;  // Define o objeto para autenticação do Firebase
FirebaseConfig configF;  // Define o objeto para configuração do Firebase

bool taskCompleted = true;  // Define uma variável booleana para verificar se a tarefa foi concluída

bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FILE_PHOTO ); // Abre o arquivo de foto
  unsigned int pic_sz = f_pic.size(); // Pega o tamanho do arquivo de foto
  return ( pic_sz > 100 ); // Se o tamanho for maior que 100 (bytes, provavelmente), retorna verdadeiro
}

// Função para capturar uma foto e salvá-la no SPIFFS (sistema de arquivos da esp32)
void capturePhotoSaveSpiffs( void ) {
  camera_fb_t * fb = NULL; // Define um ponteiro para estrutura de framebuffer da câmera
  bool ok = 0; // Define uma variável booleana para verificar se a foto foi tirada corretamente
  do {
    Serial.println("Taking a photo..."); // Imprime no monitor serial que está tirando uma foto

    fb = esp_camera_fb_get(); // Pega o frame da câmera
    if (!fb) {
      Serial.println("Camera capture failed"); // Se não conseguiu pegar o frame, imprime no monitor serial
      return;
    }

    Serial.printf("Picture file name: %s\n", FILE_PHOTO); // Imprime o nome do arquivo de foto
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE); // Abre o arquivo de foto para escrita

    if (!file) { // Se não conseguiu abrir o arquivo
      Serial.println("Failed to open file in writing mode"); // Imprime no monitor serial
    }
    else {
      file.write(fb->buf, fb->len); // Escreve o buffer do frame no arquivo
      Serial.print("The picture has been saved in "); // Imprime onde a foto foi salva
      Serial.print(FILE_PHOTO); // Imprime o nome do arquivo
      Serial.print(" - Size: "); // Imprime o tamanho da foto
      Serial.print(file.size()); // Imprime o tamanho do arquivo
      Serial.println(" bytes");
    }

    file.close(); // Fecha o arquivo
    esp_camera_fb_return(fb); // Devolve o frame para a câmera

    ok = checkPhoto(SPIFFS); // Verifica se a foto foi salva corretamente
  } while ( !ok );
  delay(10000);  // delay por 10 segundos
}

void initWiFi(){
  WiFi.begin(ssid, password); // Inicializa o WiFi com o SSID e senha fornecidos
  while (WiFi.status() != WL_CONNECTED) { // Enquanto a conexão WiFi não estiver estabelecida
    delay(1000); // Espera um segundo
    Serial.println("Connecting to WiFi..."); // Imprime no monitor serial a tentativa de conexão
  }
}

void initSPIFFS(){
  if (!SPIFFS.begin(true)) { // Inicia o SPIFFS. Se falhar,
    Serial.println("An Error has occurred while mounting SPIFFS"); // Imprime no monitor serial que ocorreu um erro ao montar o SPIFFS
    ESP.restart(); // Reinicia o ESP
  }
  else { // Se o SPIFFS iniciar corretamente
    delay(500); // Espera meio segundo
    Serial.println("SPIFFS mounted successfully"); // Imprime no monitor serial que o SPIFFS foi montado com sucesso
  }
}

void initCamera(){
 // Configuração do módulo da câmera OV2640
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Se a memória PSRAM estiver disponível,
  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA; // Configura o tamanho do frame para UXGA
    config.jpeg_quality = 10; // Configura a qualidade do JPEG para 10
    config.fb_count = 2; // Configura o número de framebuffers para 2
  } else {
    config.frame_size = FRAMESIZE_SVGA; // Configura o tamanho do frame para SVGA
    config.jpeg_quality = 12; // Configura a qualidade do JPEG para 12
    config.fb_count = 1; // Configura o número de framebuffers para 1
  }

  // Inicialização da câmera
  esp_err_t err = esp_camera_init(&config); // Inicializa a câmera com as configurações definidas
  if (err != ESP_OK) { // Se a inicialização falhar,
    Serial.printf("Camera init failed with error 0x%x", err); // Imprime no monitor serial que a inicialização da câmera falhou
    ESP.restart(); // Reinicia o ESP
  } 
}

void setup() {
  FIM_DE_CURSO_PIN.setDebounceTime(50); // Define um tempo de debounce de 50ms para o sensor de fim de curso
  Serial.begin(115200); // Inicia a comunicação serial a 115200 bps
  
  initWiFi(); // Chama a função que inicializa a conexão WiFi
  initSPIFFS(); // Chama a função que inicializa o sistema de arquivos SPIFFS
  // Desativa o detector de 'brownout'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  initCamera(); // Chama a função que inicializa a câmera

  // Configura o fuso horário
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov"); 
  
  //Firebase
  // Atribui a chave da API
  configF.api_key = API_KEY; 
  //Atribui as credenciais de login do usuário
  auth.user.email = USER_EMAIL; 
  auth.user.password = USER_PASSWORD; 
  //Atribui a função de callback para a tarefa de geração de token de longa duração
  configF.token_status_callback = tokenStatusCallback; 

  Firebase.begin(&configF, &auth); // Inicia a conexão com o Firebase
  Firebase.reconnectWiFi(true); // Configura para reconectar ao WiFi se a conexão for perdida
}

void loop() {
  FIM_DE_CURSO_PIN.loop(); // Verifica o estado do sensor de fim de curso
  
  // Se o sensor de fim de curso for acionado e a tarefa ainda não estiver concluída
  if (!FIM_DE_CURSO_PIN.isReleased() && !taskCompleted) {  
    Serial.println("Tirar Foto"); // Imprime a mensagem "Tirar Foto"
    capturePhotoSaveSpiffs(); // Chama a função para capturar uma foto e salvar no SPIFFS
    taskCompleted = true; // Marca a tarefa como concluída
    
    // Se a conexão com o Firebase estiver pronta
    if (Firebase.ready()) { 
      Serial.print("Uploading picture... "); // Imprime a mensagem "Uploading picture..."
      // Obtem a data e hora atual
      time_t now = time(nullptr); 
      // Formata a data e hora atual no formato desejado
      char filename[50]; 
      strftime(filename, sizeof(filename), "/data/%Y%m%d_%H%M%S.jpg", localtime(&now)); 
      
      //Aqui, a imagem é enviada para o Firebase Storage
      if (Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID /* Firebase Storage bucket id */, FILE_PHOTO /* caminho do arquivo local */, mem_storage_type_flash /* tipo de armazenamento de memória, mem_storage_type_flash e mem_storage_type_sd */, filenames /* caminho do arquivo remoto armazenado no bucket */, "image/jpeg" /* mime type */)){
        Serial.printf("\nDownload URL: %s\n", fbdo.downloadURL().c_str()); // Se o upload for bem sucedido, imprime a URL de download da imagem
      }
      else{
        Serial.println(fbdo.errorReason()); // Se o upload falhar, imprime a razão do erro
      }
    }
  } else if (FIM_DE_CURSO_PIN.isPressed()) { // Se o sensor de fim de curso for pressionado,
    Serial.println("Autorizar próxima foto!"); // Imprime a mensagem "Não tirar foto"
    taskCompleted = false;  // A tarefa é marcada como não concluída para que, quando o sensor for liberado novamente, uma nova foto seja tirada.
  }
  Serial.println("Não tirar foto -> fora do if"); // Fora da condição, imprime a mensagem "Não tirar foto -> fora do if"
}
