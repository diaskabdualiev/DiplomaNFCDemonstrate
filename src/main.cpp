#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "ESP32_NFC_Demo";
const char* password = "nfc123456";

// Инициализация PN532 по I2C
#define PN532_IRQ   (2)
#define PN532_RESET (3)  

// Создаем объект PN532 для I2C
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

// Веб-сервер на порту 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Структура для хранения данных карты
struct CardData {
  String uid_hex = "";
  String uid_dec = "";
  uint32_t card_id = 0;
  String card_type = "";
  uint8_t uid_length = 0;
  bool card_present = false;
  unsigned long timestamp = 0;
} currentCard;

// HTML страница
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>NFC Reader - SDU University</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 20px;
            color: white;
        }
        
        .header {
            text-align: center;
            margin-bottom: 30px;
            background: rgba(255, 255, 255, 0.1);
            padding: 20px;
            border-radius: 15px;
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.2);
        }
        
        .university-title {
            font-size: 1.8rem;
            font-weight: bold;
            margin-bottom: 10px;
            color: #FFD700;
            text-shadow: 0 0 10px rgba(255, 215, 0, 0.5);
        }
        
        .demo-title {
            font-size: 1.2rem;
            opacity: 0.9;
        }
        
        .nfc-container {
            background: rgba(255, 255, 255, 0.95);
            padding: 30px;
            border-radius: 20px;
            box-shadow: 0 20px 40px rgba(0, 0, 0, 0.3);
            max-width: 600px;
            width: 100%;
            color: #333;
            text-align: center;
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.3);
        }
        
        .status-indicator {
            width: 20px;
            height: 20px;
            border-radius: 50%;
            display: inline-block;
            margin-right: 10px;
            transition: all 0.3s ease;
        }
        
        .status-waiting {
            background: #ff6b6b;
            box-shadow: 0 0 10px rgba(255, 107, 107, 0.5);
            animation: pulse 2s infinite;
        }
        
        .status-reading {
            background: #4ecdc4;
            box-shadow: 0 0 10px rgba(78, 205, 196, 0.5);
        }
        
        @keyframes pulse {
            0% { transform: scale(1); opacity: 1; }
            50% { transform: scale(1.2); opacity: 0.7; }
            100% { transform: scale(1); opacity: 1; }
        }
        
        .nfc-icon {
            font-size: 4rem;
            margin: 20px 0;
            transition: all 0.3s ease;
        }
        
        .waiting { color: #ff6b6b; }
        .reading { color: #4ecdc4; }
        
        .card-info {
            background: #f8f9fa;
            padding: 20px;
            border-radius: 15px;
            margin-top: 20px;
            box-shadow: inset 0 2px 10px rgba(0, 0, 0, 0.1);
            display: none;
        }
        
        .card-info.show {
            display: block;
            animation: slideUp 0.5s ease;
        }
        
        @keyframes slideUp {
            from { transform: translateY(20px); opacity: 0; }
            to { transform: translateY(0); opacity: 1; }
        }
        
        .info-row {
            display: flex;
            justify-content: space-between;
            margin: 10px 0;
            padding: 10px;
            background: white;
            border-radius: 8px;
            border-left: 4px solid #667eea;
        }
        
        .info-label {
            font-weight: bold;
            color: #555;
        }
        
        .info-value {
            color: #333;
            font-family: 'Courier New', monospace;
        }
        
        .uid-value {
            color: #333;
            font-family: 'Courier New', monospace;
            font-size: 1.4rem;
            font-weight: bold;
            letter-spacing: 2px;
        }
        
        .waiting-message {
            font-size: 1.1rem;
            color: #666;
            margin-top: 15px;
        }
        
        .timestamp {
            font-size: 0.9rem;
            color: #888;
            margin-top: 15px;
        }
        
        .connection-status {
            position: fixed;
            top: 20px;
            right: 20px;
            padding: 10px 15px;
            border-radius: 25px;
            font-size: 0.9rem;
            font-weight: bold;
            transition: all 0.3s ease;
        }
        
        .connected {
            background: #4ecdc4;
            color: white;
        }
        
        .disconnected {
            background: #ff6b6b;
            color: white;
        }
        
        @media (max-width: 768px) {
            .university-title { font-size: 1.4rem; }
            .demo-title { font-size: 1rem; }
            .nfc-container { padding: 20px; margin: 0 10px; }
            .info-row { flex-direction: column; text-align: left; }
        }
    </style>
</head>
<body>
    <div class="connection-status" id="connectionStatus">🔴 Отключено</div>
    
    <div class="header">
        <div class="university-title">🏛️ Демонстрационный стэнд SDU University</div>
        <div class="demo-title">📡 NFC Reader Demo - ESP32 + PN532</div>
    </div>
    
    <div class="nfc-container">
        <div class="status-line">
            <span class="status-indicator status-waiting" id="statusIndicator"></span>
            <span id="statusText">Ожидание NFC карты...</span>
        </div>
        
        <div class="nfc-icon waiting" id="nfcIcon">📱</div>
        
                 <div class="card-info" id="cardInfo">
             <div class="info-row">
                 <span class="info-label">🆔 UID (HEX):</span>
                 <span class="uid-value" id="uidHex">-</span>
             </div>
             <div class="info-row">
                 <span class="info-label">🔢 UID (DEC):</span>
                 <span class="uid-value" id="uidDec">-</span>
             </div>
            <div class="info-row">
                <span class="info-label">🎯 Card ID:</span>
                <span class="info-value" id="cardId">-</span>
            </div>
            <div class="info-row">
                <span class="info-label">🏷️ Тип карты:</span>
                <span class="info-value" id="cardType">-</span>
            </div>
            <div class="info-row">
                <span class="info-label">📏 Длина UID:</span>
                <span class="info-value" id="uidLength">- байт</span>
            </div>
        </div>
        
        <div class="waiting-message" id="waitingMessage">
            Приложите NFC карту к считывателю
        </div>
        
        <div class="timestamp" id="timestamp"></div>
    </div>

    <script>
        let socket = null;
        let reconnectInterval = null;
        
        function connectWebSocket() {
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const socketUrl = `${protocol}//${window.location.host}/ws`;
            
            socket = new WebSocket(socketUrl);
            
            socket.onopen = function() {
                console.log('WebSocket соединение установлено');
                updateConnectionStatus(true);
                if (reconnectInterval) {
                    clearInterval(reconnectInterval);
                    reconnectInterval = null;
                }
            };
            
            socket.onmessage = function(event) {
                try {
                    const data = JSON.parse(event.data);
                    updateCardData(data);
                } catch (e) {
                    console.error('Ошибка парсинга JSON:', e);
                }
            };
            
            socket.onclose = function() {
                console.log('WebSocket соединение закрыто');
                updateConnectionStatus(false);
                
                if (!reconnectInterval) {
                    reconnectInterval = setInterval(() => {
                        console.log('Попытка переподключения...');
                        connectWebSocket();
                    }, 3000);
                }
            };
            
            socket.onerror = function(error) {
                console.error('WebSocket ошибка:', error);
                updateConnectionStatus(false);
            };
        }
        
        function updateConnectionStatus(connected) {
            const statusElement = document.getElementById('connectionStatus');
            if (connected) {
                statusElement.textContent = '🟢 Подключено';
                statusElement.className = 'connection-status connected';
            } else {
                statusElement.textContent = '🔴 Отключено';
                statusElement.className = 'connection-status disconnected';
            }
        }
        
        function updateCardData(data) {
            const cardInfo = document.getElementById('cardInfo');
            const waitingMessage = document.getElementById('waitingMessage');
            const statusIndicator = document.getElementById('statusIndicator');
            const statusText = document.getElementById('statusText');
            const nfcIcon = document.getElementById('nfcIcon');
            
            if (data.card_present) {
                // Карта обнаружена
                document.getElementById('uidHex').textContent = data.uid_hex;
                document.getElementById('uidDec').textContent = data.uid_dec;
                document.getElementById('cardId').textContent = data.card_id;
                document.getElementById('cardType').textContent = data.card_type;
                document.getElementById('uidLength').textContent = data.uid_length + ' байт';
                
                cardInfo.classList.add('show');
                waitingMessage.style.display = 'none';
                
                statusIndicator.className = 'status-indicator status-reading';
                statusText.textContent = 'NFC карта обнаружена!';
                nfcIcon.className = 'nfc-icon reading';
                nfcIcon.textContent = '✅';
                
                // Обновляем timestamp
                const now = new Date();
                document.getElementById('timestamp').textContent = 
                    `Последнее обновление: ${now.toLocaleTimeString('ru-RU')}`;
                
            } else {
                // Карта не обнаружена
                cardInfo.classList.remove('show');
                waitingMessage.style.display = 'block';
                
                statusIndicator.className = 'status-indicator status-waiting';
                statusText.textContent = 'Ожидание NFC карты...';
                nfcIcon.className = 'nfc-icon waiting';
                nfcIcon.textContent = '📱';
            }
        }
        
        // Подключаемся при загрузке страницы
        window.addEventListener('load', connectWebSocket);
    </script>
</body>
</html>
)rawliteral";

void notifyClients() {
  DynamicJsonDocument doc(1024);
  
  doc["card_present"] = currentCard.card_present;
  doc["uid_hex"] = currentCard.uid_hex;
  doc["uid_dec"] = currentCard.uid_dec;
  doc["card_id"] = currentCard.card_id;
  doc["card_type"] = currentCard.card_type;
  doc["uid_length"] = currentCard.uid_length;
  doc["timestamp"] = currentCard.timestamp;
  
  String message;
  serializeJson(doc, message);
  ws.textAll(message);
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type,
               void * arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WebSocket клиент #%u подключен с IP: %s\n", client->id(), client->remoteIP().toString().c_str());
    // Отправляем текущие данные новому клиенту
    notifyClients();
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WebSocket клиент #%u отключен\n", client->id());
  }
}

void setupWiFi() {
  // Создаем WiFi точку доступа
  Serial.println("Создание WiFi точки доступа...");
  WiFi.softAP(ssid, password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("IP адрес точки доступа: ");
  Serial.println(IP);
  Serial.println("Подключитесь к WiFi: " + String(ssid));
  Serial.println("Пароль: " + String(password));
  Serial.println("Откройте в браузере: http://" + IP.toString());
}

String getCardType(uint8_t uidLength) {
  if (uidLength == 4) {
    return "MiFare Classic (1K/4K)";
  } else if (uidLength == 7) {
    return "MiFare Ultralight/NTAG";
  } else {
    return "Неизвестный тип";
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("═══════════════════════════════════");
  Serial.println("🏛️ SDU University NFC Demo");
  Serial.println("📡 ESP32 + PN532 Web Interface");
  Serial.println("═══════════════════════════════════");
  
  // Инициализация PN532
  Serial.println("Инициализация PN532...");
  nfc.begin();
  
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("❌ Модуль PN532 не найден! Проверьте подключение.");
    while (1);
  }
  
  Serial.print("✅ Найден чип PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  Serial.print("📦 Версия прошивки: "); Serial.print((versiondata>>16) & 0xFF, DEC); 
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  
  nfc.SAMConfig();
  
  // Настройка WiFi
  setupWiFi();
  
  // Настройка WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  
  // Настройка веб-сервера
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  
  server.begin();
  Serial.println("🌐 Веб-сервер запущен!");
  Serial.println("═══════════════════════════════════");
}

void loop() {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
  uint8_t uidLength;
  
  // Очищаем буфер UID
  memset(uid, 0, sizeof(uid));
  
  // Ждем NFC карту с таймаутом
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 1000);
  
  static unsigned long lastCardTime = 0;
  static bool lastCardState = false;
  
  if (success) {
    // Карта найдена
    if (!lastCardState || (millis() - lastCardTime > 2000)) {
      Serial.println("═══════════════════════════════════");
      Serial.println("🔍 NFC КАРТА ОБНАРУЖЕНА!");
      Serial.println("═══════════════════════════════════");
      
      // Обновляем данные карты
      currentCard.card_present = true;
      currentCard.uid_length = uidLength;
      currentCard.timestamp = millis();
      
      // Формируем UID в HEX
      currentCard.uid_hex = "";
      for (uint8_t i = 0; i < uidLength; i++) {
        if (uid[i] < 0x10) currentCard.uid_hex += "0";
        currentCard.uid_hex += String(uid[i], HEX);
        if (i < uidLength - 1) currentCard.uid_hex += " ";
      }
      currentCard.uid_hex.toUpperCase();
      
      // Формируем UID в DEC
      currentCard.uid_dec = "";
      for (uint8_t i = 0; i < uidLength; i++) {
        currentCard.uid_dec += String(uid[i], DEC);
        if (i < uidLength - 1) currentCard.uid_dec += " ";
      }
      
      // Вычисляем Card ID (для 4-байтных UID)
      if (uidLength == 4) {
        currentCard.card_id = uid[0];
        currentCard.card_id <<= 8;
        currentCard.card_id |= uid[1];
        currentCard.card_id <<= 8;
        currentCard.card_id |= uid[2];  
        currentCard.card_id <<= 8;
        currentCard.card_id |= uid[3];
      } else {
        currentCard.card_id = 0;
      }
      
      currentCard.card_type = getCardType(uidLength);
      
      // Выводим в Serial для отладки
      Serial.printf("📱 Длина UID: %d байт\n", uidLength);
      Serial.println("🆔 UID (HEX): " + currentCard.uid_hex);
      Serial.println("🔢 UID (DEC): " + currentCard.uid_dec);
      if (currentCard.card_id > 0) {
        Serial.printf("🎯 Card ID: %u\n", currentCard.card_id);
      }
      Serial.println("🏷️ Тип карты: " + currentCard.card_type);
      
      // Уведомляем веб-клиентов
      notifyClients();
      
      lastCardTime = millis();
      lastCardState = true;
    }
  } else {
    // Карта не найдена
    if (lastCardState && (millis() - lastCardTime > 1000)) {
      Serial.println("✅ Карта убрана - ожидание новой карты...");
      currentCard.card_present = false;
      notifyClients();
      lastCardState = false;
    }
  }
  
  // Очистка WebSocket клиентов
  ws.cleanupClients();
  
  delay(200);
}

