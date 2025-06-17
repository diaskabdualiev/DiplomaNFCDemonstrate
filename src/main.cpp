#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PN532.h>

// Инициализация PN532 по I2C
#define PN532_IRQ   (2)
#define PN532_RESET (3)  

// Создаем объект PN532 для I2C
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("PN532 NFC Reader - Чтение UID карт");
  Serial.println("===================================");
  
  // Инициализация PN532
  nfc.begin();
  
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("Модуль PN532 не найден! Проверьте подключение.");
    while (1); // останавливаем выполнение
  }
  
  // Выводим информацию о версии
  Serial.print("Найден чип PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  Serial.print("Версия прошивки: "); Serial.print((versiondata>>16) & 0xFF, DEC); 
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  
  // Настраиваем на чтение MiFare карт
  nfc.SAMConfig();
  
  Serial.println("Ожидание NFC карты...");
  Serial.println("");
}

void loop() {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Буфер для хранения UID
  uint8_t uidLength;                        // Длина UID (4 или 7 байт)
  
  // Очищаем буфер UID
  memset(uid, 0, sizeof(uid));
  
  // Ждем NFC карту с таймаутом
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 1000);
  
  if (success) {
    // Найдена карта!
    Serial.println("═══════════════════════════════════");
    Serial.println("🔍 NFC КАРТА ОБНАРУЖЕНА!");
    Serial.println("═══════════════════════════════════");
    
    Serial.print("📱 Длина UID: "); Serial.print(uidLength, DEC); Serial.println(" байт");
    
    Serial.print("🆔 UID (HEX): ");
    for (uint8_t i = 0; i < uidLength; i++) {
      if (uid[i] < 0x10) {
        Serial.print("0");
      }
      Serial.print(uid[i], HEX);
      if (i < uidLength - 1) {
        Serial.print(" ");
      }
    }
    Serial.println();
    
    Serial.print("🔢 UID (DEC): ");
    for (uint8_t i = 0; i < uidLength; i++) {
      Serial.print(uid[i], DEC);
      if (i < uidLength - 1) {
        Serial.print(" ");
      }
    }
    Serial.println();
    
    // Преобразуем UID в одно число (для коротких UID)
    if (uidLength == 4) {
      uint32_t cardid = uid[0];
      cardid <<= 8;
      cardid |= uid[1];
      cardid <<= 8;
      cardid |= uid[2];  
      cardid <<= 8;
      cardid |= uid[3]; 
      Serial.print("🎯 Card ID: ");
      Serial.println(cardid);
    }
    
    // Определяем тип карты
    Serial.print("🏷️  Тип карты: ");
    if (uidLength == 4) {
      Serial.println("MiFare Classic (1K/4K)");
    } else if (uidLength == 7) {
      Serial.println("MiFare Ultralight/NTAG");
    } else {
      Serial.println("Неизвестный тип");
    }
    
    Serial.println("═══════════════════════════════════");
    Serial.println("⏳ Ожидание удаления карты...");
    
    // Ждем пока карта не будет убрана с большим таймаутом
    uint8_t tempUid[7];
    uint8_t tempUidLength;
    while (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, tempUid, &tempUidLength, 500)) {
      Serial.print(".");
      delay(200);
    }
    
    Serial.println("\n✅ Карта убрана!");
    
    // Принудительно сбрасываем состояние PN532
    nfc.SAMConfig();
    delay(100);
    
    Serial.println("🔄 Готов к чтению новой карты...");
    Serial.println("");
    
    delay(1000); // Большая задержка для стабилизации
  } else {
    // Карта не найдена
    static unsigned long lastPrintTime = 0;
    if (millis() - lastPrintTime > 3000) { // Выводим сообщение каждые 3 секунды
      Serial.println("⏳ Ожидание NFC карты...");
      lastPrintTime = millis();
    }
  }
  
  delay(200); // Увеличенная задержка между попытками чтения
}

