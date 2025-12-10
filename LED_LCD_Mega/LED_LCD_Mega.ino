// Mega: lê seu próprio RFID, recebe UIDs do Uno via Serial1,
// identifica Paracetamol/Buscopan (7 bytes), conta UIDs únicos e atualiza LCD E PYTHON.
// COM FUNCIONALIDADE DE RESET
#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#define SS_PIN_1 53
#define RST_PIN_1 5
#define GREEN_LED_PIN 8
#define RED_LED_PIN 7

MFRC522 rfid1(SS_PIN_1, RST_PIN_1);
// Supondo endereço 0x27.
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// --------- CONFIGURAÇÃO PARA 7-BYTES ----------
const byte UID_SIZE = 7; 

// --------- LISTAS (Paracetamol / Buscopan) ----------
byte listaParacetamol[][UID_SIZE] = {
  {0x04, 0x27, 0x81, 0x35, 0xBF, 0x2A, 0x81},
  {0x04, 0x78, 0x84, 0x35, 0xBF, 0x2A, 0x81},
  {0x04, 0x25, 0x8B, 0x35, 0xBF, 0x2A, 0x81},
  {0x04, 0xCD, 0x8F, 0x35, 0xBF, 0x2A, 0x81},
  {0x04, 0x21, 0x98, 0x35, 0xBF, 0x2A, 0x81},
  {0x04, 0xE6, 0x9C, 0x35, 0xBF, 0x2A, 0x81},
  {0x04, 0x45, 0xA5, 0x35, 0xBF, 0x2A, 0x81},
  {0x04, 0x84, 0xAA, 0x35, 0xBF, 0x2A, 0x81},
  {0x04, 0xF3, 0xB3, 0x35, 0xBF, 0x2A, 0x81},
  {0x04, 0x42, 0xB9, 0x35, 0xBF, 0x2A, 0x81}
};
const byte paracetamolCount = sizeof(listaParacetamol) / sizeof(listaParacetamol[0]);

byte listaBuscopan[][UID_SIZE] = {
  {0x04, 0x5D, 0x9A, 0x35, 0xBF, 0x2A, 0x81},
  {0x04, 0x3C, 0X9F, 0x35, 0xBF, 0x2A, 0x81},
  {0x04, 0x2C, 0xD8, 0x36, 0xBF, 0x2A, 0x81},
  {0x04, 0x7A, 0xAD, 0x35, 0xBF, 0x2A, 0x81},
  {0x04, 0x7D, 0xB6, 0x35, 0xBF, 0x2A, 0x81},
  {0x04, 0x2F, 0xBB, 0x35, 0xBF, 0x2A, 0x81},
  {0x04, 0xAD, 0xC3, 0x35, 0xBF, 0x2A, 0x81},
  {0x04, 0x76, 0xC9, 0x35, 0xBF, 0x2A, 0x81},
  {0x04, 0xE4, 0xD2, 0x35, 0xBF, 0x2A, 0x81},
  {0x04, 0xCC, 0xD8, 0x35, 0xBF, 0x2A, 0x81}
};
const byte buscopanCount = sizeof(listaBuscopan) / sizeof(listaBuscopan[0]);

// --------- ARMAZENAMENTO DE UIDs VISTOS ----------
const byte MAX_UNIQUE = 64;
byte seenUIDs[MAX_UNIQUE][UID_SIZE];
byte seenSizes[MAX_UNIQUE];
byte seenCount = 0;

// contadores por lista
unsigned int counterParacetamol = 0;
unsigned int counterBuscopan = 0;

// 0 = nenhuma lida ainda; 1 = Paracetamol primeiro; 2 = Buscopan primeiro
byte firstListRead = 0;

char serialBuffer[128];
uint8_t bufPos = 0;

// ----------------------------------------------------
// Função para enviar JSON ao Python via Serial (USB)
// ----------------------------------------------------
void sendDataToPython() {
  Serial.print("{\"p\": ");
  Serial.print(counterParacetamol);
  Serial.print(", \"b\": ");
  Serial.print(counterBuscopan);
  Serial.println("}"); 
}

void setup() {
  Serial.begin(9600);    // monitor USB (para Python e debug)
  Serial1.begin(9600);   // comunicação com Uno (via Serial1)
  SPI.begin();
  rfid1.PCD_Init();

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Mega ready");
  delay(600);
  updateLCD();
   
  sendDataToPython(); 

  Serial.println("Mega ready (7-byte UID mode) com RESET");
}

// --------- FUNÇÕES AUXILIARES ----------
bool uidEquals(const byte *a, byte asize, const byte *b, byte bsize) {
  if (asize != bsize) return false;
  for (byte i = 0; i < asize; i++) if (a[i] != b[i]) return false;
  return true;
}

bool uidMatchList(const byte *uid, byte size, const byte list[][UID_SIZE], byte count) {
  if (size != UID_SIZE) return false;
  for (byte i = 0; i < count; i++) {
    bool match = true;
    for (byte j = 0; j < UID_SIZE; j++) {
      if (uid[j] != list[i][j]) { match = false; break; }
    }
    if (match) return true;
  }
  return false;
}

int identifyList(const byte *uid, byte size) {
  if (uidMatchList(uid, size, listaParacetamol, paracetamolCount)) return 1;
  if (uidMatchList(uid, size, listaBuscopan, buscopanCount)) return 2;
  return 0;
}

bool alreadySeen(const byte *uid, byte size) {
  for (byte i = 0; i < seenCount; i++) {
    if (uidEquals(uid, size, seenUIDs[i], seenSizes[i])) return true;
  }
  return false;
}

void addSeenUID(const byte *uid, byte size) {
  if (seenCount >= MAX_UNIQUE) {
    Serial.println("WARN: capacidade de UIDs vistos atingida!");
    return;
  }
  for (byte i = 0; i < size; i++) seenUIDs[seenCount][i] = uid[i];
  seenSizes[seenCount] = size;
  seenCount++;
}

void updateLCD() {
  lcd.clear();
  if (firstListRead == 2) {
    lcd.setCursor(0,0);
    lcd.print("Buscopan:");
    lcd.print(counterBuscopan);
    lcd.setCursor(0,1);
    lcd.print("Paracetamol:");
    lcd.print(counterParacetamol);
  } else {
    lcd.setCursor(0,0);
    lcd.print("Paracetamol:");
    lcd.print(counterParacetamol);
    lcd.setCursor(0,1);
    lcd.print("Buscopan:");
    lcd.print(counterBuscopan);
  }
}

void handleListRead(int listaId, const byte *uid, byte size, const char *source) {
  if (!alreadySeen(uid, size)) {
    addSeenUID(uid, size);
    if (listaId == 1) counterParacetamol++;
    else if (listaId == 2) counterBuscopan++;

    if (firstListRead == 0) firstListRead = listaId;

    Serial.print("Novo UID visto (");
    Serial.print(source);
    Serial.print(") - lista ");
    Serial.print(listaId == 1 ? "Paracetamol" : "Buscopan");
    Serial.print(" -> contador: ");
    Serial.println( listaId == 1 ? counterParacetamol : counterBuscopan );

    updateLCD();
    sendDataToPython();
    
  } else {
    Serial.print("UID ja visto (");
    Serial.print(source);
    Serial.println(") - sem incremento.");
    sendDataToPython(); 
  }
}

void handleUnknownUID(const char *line, const char *source) {
  Serial.print("Nao reconhecido (");
  Serial.print(source);
  Serial.print("): ");
  Serial.println(line);
}

bool parseUIDString(const char* s, byte *outBytes, byte &outSize) {
  const char *p = strchr(s, ':');
  if (p) p++; else p = s;

  outSize = 0;
  int nibble = -1;
  while (*p && outSize < UID_SIZE) {
    char c = *p;
    int v = -1;
    if (c >= '0' && c <= '9') v = c - '0';
    else if (c >= 'A' && c <= 'F') v = 10 + (c - 'A');
    else if (c >= 'a' && c <= 'f') v = 10 + (c - 'a');

    if (v >= 0) {
      if (nibble < 0) {
        nibble = v;
      } else {
        outBytes[outSize++] = (byte)((nibble << 4) | v);
        nibble = -1;
      }
    }
    p++;
  }
  return outSize > 0;
}

// --------- LOOP PRINCIPAL ----------
void loop() {
  
  // =========================================================
  // NOVO: Verifica se o Python mandou comando de RESET ('R')
  // =========================================================
  if (Serial.available() > 0) {
    char comando = Serial.read();
    
    if (comando == 'R') {
      // 1. Zera os contadores
      counterParacetamol = 0;
      counterBuscopan = 0;
      
      // 2. Limpa a memória de tags já vistas (CRUCIAL!)
      seenCount = 0;
      firstListRead = 0; // Reseta a ordem de exibição também
      
      // 3. Atualiza LCD para dar feedback visual
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(">> CONTAGEM <<");
      lcd.setCursor(0,1);
      lcd.print(">> ZERADA!  <<");
      
      delay(1500); // Mostra a mensagem por 1.5 segundos
      updateLCD(); // Volta a mostrar os números (agora 0)
      
      // 4. Envia confirmação zerada pro Python
      sendDataToPython();
      Serial.println("LOG: Sistema resetado pelo Python");
    }
  }
  // =========================================================


  // Recebe dados do Uno via Serial1
  while (Serial1.available()) {
    char c = Serial1.read();
    if (c == '\r') continue;
    if (c == '\n' || bufPos >= (sizeof(serialBuffer)-1)) {
      serialBuffer[bufPos] = 0;
      Serial.print("Recebido do Uno: ");
      Serial.println(serialBuffer);

      byte parsed[UID_SIZE];
      byte parsedSize = 0;
      if (parseUIDString(serialBuffer, parsed, parsedSize)) {
        int which = identifyList(parsed, parsedSize);
        if (which == 1) handleListRead(1, parsed, parsedSize, "Uno Reader");
        else if (which == 2) handleListRead(2, parsed, parsedSize, "Uno Reader");
        else handleUnknownUID(serialBuffer, "Uno Reader");
      } else {
        Serial.println("Falha parsing UID vindo do Uno");
      }
      bufPos = 0;
    } else {
      serialBuffer[bufPos++] = c;
    }
  }

  // Verifica o leitor do Mega
  if (rfid1.PICC_IsNewCardPresent() && rfid1.PICC_ReadCardSerial()) {
    byte scanned[UID_SIZE];
    byte scannedSize = rfid1.uid.size;
    for (byte i = 0; i < scannedSize && i < UID_SIZE; i++) scanned[i] = rfid1.uid.uidByte[i];

    Serial.print("Mega read UID: ");
    for (byte i = 0; i < scannedSize; i++) {
      if (scanned[i] < 16) Serial.print('0');
      Serial.print(scanned[i], HEX);
      Serial.print(' ');
    }
    Serial.println();

    int which = identifyList(scanned, scannedSize);
    if (which == 1) handleListRead(1, scanned, scannedSize, "Mega Reader");
    else if (which == 2) handleListRead(2, scanned, scannedSize, "Mega Reader");
    else {
      char tmp[64];
      int idx = 0;
      for (byte i = 0; i < scannedSize; i++) {
        idx += snprintf(tmp + idx, sizeof(tmp) - idx, "%02X", scanned[i]);
        if (i < scannedSize - 1) idx += snprintf(tmp + idx, sizeof(tmp) - idx, " ");
      }
      handleUnknownUID(tmp, "Mega Reader");
    }

    rfid1.PICC_HaltA();
  }

  delay(10);
}