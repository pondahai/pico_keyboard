#include <Arduino.h>
#include <Keyboard.h> // 使用標準的 Keyboard.h

// --- 硬體引腳定義 ---
const int DATA_OUT_PIN = 15;
const int LATCH_PIN    = 14;
const int CLOCK_PIN    = 26;
const int DATA_IN_PIN  = 27;

// --- 按鍵轉換設定 ---
// Keyboard.h 的特殊鍵碼定義
#define KEY_NULL        0x00
#define KEY_CTRL_L    0x80
#define KEY_SHIFT_R   0x85
#define KEY_ALT_L     0x82
#define KEY_ENTER     0xB0
#define KEY_ESC       0xB1
#define KEY_BACKSPACE 0xB2
#define KEY_TAB       0xB3
#define KEY_SPACE     0x20
#define KEY_CAPS_LOCK 0xC1
#define KEY_PGUP      0xD3
#define KEY_PGDOWN    0xD6
#define KEY_RIGHT_ARROW 0xD7
#define KEY_LEFT_ARROW 0xD8
#define KEY_DOWN_ARROW 0xD9
#define KEY_UP_ARROW  0xDA

// Keymap 表格
const uint16_t keymap[8][8] = {
  {'1', '3', '5', '7', '9', '-', KEY_TAB, KEY_BACKSPACE},
  {'q', 'e', 't', 'u', 'o', '[', KEY_ESC, '\\'},
  {'a', 'd', 'g', 'j', 'l', '\'', KEY_CTRL_L, KEY_CAPS_LOCK},
  {'z', 'c', 'b', 'm', '.', KEY_SHIFT_R, KEY_DOWN_ARROW, KEY_NULL},
  {'2', '4', '6', '8', '0', '=', '`', KEY_NULL},
  {'w', 'r', 'y', 'i', 'p', ']', KEY_UP_ARROW, KEY_PGUP},
  {'s', 'f', 'h', 'k', ';', KEY_ENTER, KEY_RIGHT_ARROW, KEY_PGDOWN},
  {'x', 'v', 'n', ',', '/', KEY_SPACE, KEY_LEFT_ARROW, KEY_ALT_L}
};

// --- 全域變數 ---
bool ledState[2] = {false, false};
bool keyState[8][8];
bool lastKeyState[8][8];
bool debugMode = true;

// --- *** 將輔助函數定義移到 loop() 之前 *** ---

/**
 * @brief 自訂的 shiftIn 函數
 */
byte myShiftIn(uint8_t dataPin, uint8_t clockPin) {
  byte data = 0;
  for (int i = 0; i < 8; i++) {
    if (digitalRead(dataPin)) { data |= (1 << i); }
    digitalWrite(clockPin, HIGH);
    digitalWrite(clockPin, LOW);
  }
  return data;
}

/**
 * @brief 更新鍵盤硬體狀態
 */
void updateKeyboard() {
  for (int row = 0; row < 8; row++) {
    byte rowScanData = (1 << row);
    digitalWrite(LATCH_PIN, LOW);
    shiftOut(DATA_OUT_PIN, CLOCK_PIN, MSBFIRST, 0x00); // ledData 佔位
    shiftOut(DATA_OUT_PIN, CLOCK_PIN, MSBFIRST, rowScanData);
    digitalWrite(LATCH_PIN, HIGH); delayMicroseconds(5);
    digitalWrite(LATCH_PIN, LOW); delayMicroseconds(1);
    digitalWrite(LATCH_PIN, HIGH);
    byte colData = myShiftIn(DATA_IN_PIN, CLOCK_PIN);
    for (int col = 0; col < 8; col++) {
      if (colData & (1 << col)) { keyState[row][7 - col] = true; } 
      else { keyState[row][7 - col] = false; }
    }
  }
}

/**
 * @brief 打印矩陣狀態 (用於除錯模式)
 */
void printKeyboardStateMatrix() {
  Serial.print("\n\n"); Serial.println("--- Key State Matrix ---");
  Serial.println("      Cols: 0 1 2 3 4 5 6 7"); Serial.println("      -----------------------");
  for (int r = 0; r < 8; r++) {
    Serial.print("Row "); Serial.print(r); Serial.print(": | ");
    for (int c = 0; c < 8; c++) {
      if(keyState[r][c] != lastKeyState[r][c]) { Serial.print(keyState[r][c] ? "*X " : "*_ "); } 
      else { Serial.print(keyState[r][c] ? "X " : ". "); }
    }
    Serial.println("|");
  }
  Serial.println("      -----------------------");
  Serial.print("LEDs: [1]CapsLock:N/A [2]Debug:"); Serial.println(debugMode ? "ON" : "OFF");
}

/**
 * @brief 處理單個按鍵事件 (使用 Keyboard.h API)
 */
void processKeyEvent(int r, int c, bool isPressed) {
  uint16_t keycode = keymap[r][c];
  if (keycode == KEY_NULL) return;
  
  // 模式切換檢查
  if (isPressed) {
    bool ctrl_l_pressed = keyState[2][6];
    bool shift_r_pressed = keyState[3][5];
    if ((keycode == KEY_CTRL_L && shift_r_pressed) || (keycode == KEY_SHIFT_R && ctrl_l_pressed)) {
      debugMode = !debugMode;
      Serial.print("\n\n--- Switched to "); Serial.print(debugMode ? "DEBUG MODE" : "NORMAL TYPING MODE"); Serial.println(" ---\n");
      Keyboard.releaseAll(); // 切換模式時釋放所有按鍵
      return; 
    }
  }
  
  if (debugMode) {
    if (isPressed) { printKeyboardStateMatrix(); Serial.print("Key Pressed: ("); Serial.print(r); Serial.print(","); Serial.print(c); Serial.println(")");} 
    else { printKeyboardStateMatrix(); }
    return;
  }

  // === 正常鍵盤模式 ===
  if (isPressed) {
    Keyboard.press(keycode);
  } else {
    Keyboard.release(keycode);
  }
}


void setup() {
  Serial.begin(115200);
  delay(1000); 
  
  Serial.println("\n--- USB Keyboard Firmware (using Keyboard.h API) ---");
  
  Keyboard.begin();

  pinMode(DATA_OUT_PIN, OUTPUT); pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT); pinMode(DATA_IN_PIN, INPUT);
  digitalWrite(LATCH_PIN, LOW); digitalWrite(CLOCK_PIN, LOW);
  
  updateKeyboard();
  memcpy(lastKeyState, keyState, sizeof(keyState));
}

void loop() {
  updateKeyboard();

  // 遍歷所有按鍵，檢查狀態變化
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      if (keyState[r][c] != lastKeyState[r][c]) {
        processKeyEvent(r, c, keyState[r][c]);
      }
    }
  }

  // 更新 lastKeyState
  memcpy(lastKeyState, keyState, sizeof(keyState));

  // LED 控制
  // ledState[0] = ???; // CapsLock 狀態在 Keyboard.h 中不易獲取
  ledState[1] = debugMode; 

  byte ledData = 0;
  if (ledState[0]) bitSet(ledData, 0);
  if (ledState[1]) bitSet(ledData, 1);
  byte rowScanData = 0x00;
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_OUT_PIN, CLOCK_PIN, MSBFIRST, ledData);
  shiftOut(DATA_OUT_PIN, CLOCK_PIN, MSBFIRST, rowScanData);
  digitalWrite(LATCH_PIN, HIGH);
  
  delay(5);
}