#include <WiFi.h>
#include <HTTPClient.h>
#include <math.h>

// Definir as posições dos roteadores
const float router_positions[4][2] = {
  {0, 0},
  {10, 0},
  {0, 10},
  {10, 10}
};

// Nome das redes Wi-Fi dos roteadores
const char* ssid_list[4] = {"SniffySage's Galaxy S23+", "Motorola XL", "S24 Ultra de Marina", "S23 Ultra 5G"};

// RSSI valores dos roteadores
int rssi_values[4];

// Configurações de WiFi
const char* ssid_array[4] = {"e4wgqxwk88rzv2p", "Motorola XL", "S24 Ultra de Marina", "S23 Ultra 5G"};  // Lista de SSIDs
const char* password_array[4] = {"avv42q7rw7bgjzz", "leonardo0513tanaka", "marinatop", "Du05137715"};  // Lista de senhas

// Endpoint da API
const char* apiEndpointPOST = "https://esp32-server-b3zj.onrender.com/posicao";  // Substitua pelo endereço do seu servidor

// Configuração do pino do botão
int buttonPin = 13;  // Defina o pino onde o botão está conectado
bool buttonState = HIGH;
bool lastButtonState = HIGH;
unsigned long buttonPressTime = 0;
unsigned long currentTime = 0;
unsigned long debounceTime = 50;

// Variável para armazenar o status do carrinho
String statusCarrinho = "Indefinido";

// Função para tentar conectar ao WiFi
void setup_wifi() {
  delay(10);
  Serial.println();

  // Tentar conectar a cada SSID da lista
  for (int i = 0; i < 4; i++) {
    Serial.print("Tentando se conectar a ");
    Serial.println(ssid_array[i]);

    WiFi.begin(ssid_array[i], password_array[i]);

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 10) {
      delay(1000);
      Serial.print(".");
      retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi conectado");
      Serial.println(WiFi.localIP());
      return;  // Saímos do loop se conectou
    }

    Serial.println("Falha ao conectar ao SSID, tentando o próximo...");
  }

  // Caso não consiga conectar a nenhuma rede
  Serial.println("Falha ao conectar a todas as redes Wi-Fi.");
}

// Converter RSSI para distância
float rssi_to_distance(int rssi, int A = -56, float n = 2.1) {
  return pow(10, (A - rssi) / (10 * n));
}

// Função de trilateração
bool trilateration(float distances[4], float& x, float& y) {
  float x1 = router_positions[0][0], y1 = router_positions[0][1], r1 = distances[0];
  float x2 = router_positions[1][0], y2 = router_positions[1][1], r2 = distances[1];
  float x3 = router_positions[2][0], y3 = router_positions[2][1], r3 = distances[2];
  // float x4 = router_positions[3][0], y4 = router_positions[3][1], r4 = distances[3];

  // Implementação da trilateração usando os três primeiros pontos
  float A = 2 * (x2 - x1);
  float B = 2 * (y2 - y1);
  float C = r1 * r1 - r2 * r2 - x1 * x1 - y1 * y1 + x2 * x2 + y2 * y2;

  float D = 2 * (x3 - x1);
  float E = 2 * (y3 - y1);
  float F = r1 * r1 - r3 * r3 - x1 * x1 - y1 * y1 + x3 * x3 + y3 * y3;

  float denominator = A * E - B * D;
  if (denominator == 0) {
    Serial.println("Erro na trilateração: Sistema não tem solução.");
    return false;
  }

  x = (C * E - B * F) / denominator;
  y = (A * F - C * D) / denominator;

  if (isnan(x) || isnan(y)) {
    Serial.println("Erro na trilateração: Coordenadas inválidas calculadas.");
    return false;
  }

  return true;
}

void setup() {
  Serial.begin(115200);
  setup_wifi();  

  pinMode(buttonPin, INPUT_PULLUP);  // Configura o pino do botão como entrada com pull-up interno
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    setup_wifi();  // Tentar reconectar ao Wi-Fi se desconectado
  }

  Serial.println("Iniciando escaneamento...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("Nenhuma rede encontrada");
    delay(5000);
    return;
  }

  // Coletar valores RSSI
  for (int i = 0; i < 4; i++) {
    rssi_values[i] = -100;
    for (int j = 0; j < n; j++) {
      if (WiFi.SSID(j) == ssid_list[i]) {
        rssi_values[i] = WiFi.RSSI(j);
        break;
      }
    }
  }

  // Calcular distâncias
  float distances[4];
  for (int i = 0; i < 4; i++) {
    distances[i] = rssi_to_distance(rssi_values[i]);
    Serial.print("Distância ao ");
    Serial.print(ssid_list[i]);
    Serial.print(": ");
    Serial.print(distances[i]);
    Serial.println(" metros");
  }

  // Trilateração
  float x, y;
  if (trilateration(distances, x, y)) {
    Serial.print("Posição estimada do ESP32: x = ");
    Serial.print(x);
    Serial.print(" metros, y = ");
    Serial.print(y);
    Serial.println(" metros");

    // Enviar posição e status via HTTP POST
    enviarDados(x, y, statusCarrinho);
  }

  // Leitura do botão
  int reading = digitalRead(buttonPin);
  if (reading == LOW && lastButtonState == HIGH) {
    buttonPressTime = millis();
    delay(debounceTime);  // Evitar "ruído"
  }

  if (reading == HIGH && lastButtonState == LOW) {
    currentTime = millis();
    unsigned long pressDuration = currentTime - buttonPressTime;

    if (pressDuration >= 1000 && pressDuration < 2000) {
      Serial.println("Carrinho Ocupado");
      statusCarrinho = "Carrinho Ocupado";
    } else if (pressDuration >= 2000) {
      Serial.println("Carrinho Livre");
      statusCarrinho = "Carrinho Livre";
    }
  }

  lastButtonState = reading;

  delay(5000);  // Aguardar antes de repetir
}

// Função para enviar dados via HTTP POST
void enviarDados(float x, float y, String status) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(apiEndpointPOST);
    http.addHeader("Content-Type", "application/json");

    // Criar payload JSON com os dados a serem enviados
    String payload = "{";
    payload += "\"x\": ";
    payload += x;
    payload += ", \"y\": ";
    payload += y;
    payload += ", \"status\": \"";
    payload += status;
    payload += "\"}";

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Resposta do servidor: " + response);
    } else {
      Serial.println("Erro ao enviar POST");
    }

    http.end();
  } else {
    Serial.println("Erro: WiFi não conectado");
  }
}