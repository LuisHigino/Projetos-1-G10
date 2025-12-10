// Uno: lê RFID MFRC522 e envia UID via Serial (para Mega)
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN_2 10
#define RST_PIN_2 9

MFRC522 rfid2(SS_PIN_2, RST_PIN_2);

void setup() {
  Serial.begin(9600); // comunicação TTL com Mega (e USB para debug)
  SPI.begin();
  rfid2.PCD_Init();
  Serial.println("UNO RFID ready (7-byte UID mode)");
}

void loop() {
  if (!rfid2.PICC_IsNewCardPresent()) return;
  if (!rfid2.PICC_ReadCardSerial()) return;

  // Monta e envia: "UID:XX XX XX XX XX XX XX\n" (n bytes dependendo do tag)
  char buffer[64];
  int idx = 0;
  idx += snprintf(buffer + idx, sizeof(buffer) - idx, "UID:");
  for (byte i = 0; i < rfid2.uid.size; i++) {
    idx += snprintf(buffer + idx, sizeof(buffer) - idx, "%02X", rfid2.uid.uidByte[i]);
    if (i < rfid2.uid.size - 1) idx += snprintf(buffer + idx, sizeof(buffer) - idx, " ");
  }
  idx += snprintf(buffer + idx, sizeof(buffer) - idx, "\n");
  Serial.print(buffer); // envia ao Mega pela serial TTL

  // Debug local também (aparece no monitor serial do PC se Uno estiver conectado)
  Serial.flush();

  rfid2.PICC_HaltA();
  delay(200); // debounce
}
