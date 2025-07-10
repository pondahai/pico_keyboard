#include <Arduino.h>

// --- 硬體引腳定義 ---
const int DATA_OUT_PIN = 15;
const int LATCH_PIN    = 14;
const int CLOCK_PIN    = 26;
const int DATA_IN_PIN  = 27;

// --- 按鍵轉換設定 ---
// 1. 定義功能鍵碼 (新增 KEY_ALT)
#define KEY_NULL        0
#define KEY_SHIFT_R   202
#define KEY_ENTER     203
#define KEY_BACKSPACE 204
#define KEY_CTRL_L    205
#define KEY_TAB       206
#define KEY_ESC       207
#define KEY_CAPS_LOCK 208
#define KEY_UP_ARROW  209
#define KEY_DOWN_ARROW 210
#define KEY_LEFT_ARROW 211
#define KEY_RIGHT_ARROW 212
#define KEY_PGUP      213
#define KEY_PGDOWN    214
#define KEY_ALT       215  // *** 新增 ***

// 2. Keymap 表格 (您提供的最終版本)
const char keymap_base[8][8] = {
  // COL 0        1         2         3         4         5         6         7
  {'1', '3', '5', '7', '9', '-', KEY_TAB, KEY_BACKSPACE},
  {'q', 'e', 't', 'u', 'o', '[', KEY_ESC, '\\'},
  {'a', 'd', 'g', 'j', 'l', '\'', KEY_CTRL_L, KEY_CAPS_LOCK},
  {'z', 'c', 'b', 'm', '.', KEY_SHIFT_R, KEY_DOWN_ARROW, KEY_NULL},
  {'2', '4', '6', '8', '0', '=', '`', KEY_NULL},
  {'w', 'r', 'y', 'i', 'p', ']', KEY_UP_ARROW, KEY_PGUP},
  {'s', 'f', 'h', 'k', ';', KEY_ENTER, KEY_RIGHT_ARROW, KEY_PGDOWN},
  {'x', 'v', 'n', ',', '/', ' ', KEY_LEFT_ARROW, KEY_ALT}
};

const char keymap_shifted[8][8] = {
  // COL 0        1         2         3         4         5         6         7
  {'!', '#', '%', '&', '(', '_', '-', KEY_BACKSPACE},
  {'Q', 'E', 'T', 'U', 'O', '{', KEY_ESC, '\\'},
  {'A', 'D', 'G', 'J', 'L', '"', KEY_CTRL_L, KEY_CAPS_LOCK},
  {'Z', 'C', 'B', 'M', '>', KEY_SHIFT_R, KEY_DOWN_ARROW, KEY_NULL},
  {'@', '$', '^', '*', ')', '+', '~', KEY_NULL},
  {'W', 'R', 'Y', 'I', 'P', '}', KEY_UP_ARROW, KEY_PGUP},
  {'S', 'F', 'H', 'K', ':', KEY_ENTER, KEY_RIGHT_ARROW, KEY_PGDOWN},
  {'X', 'V', 'N', '<', '?', ' ', KEY_LEFT_ARROW, KEY_ALT}
};


// --- 全域變數 ---
bool ledState[2] = {false, false};
bool keyState[8][8];
bool lastKeyState[8][8];
bool isShiftPressed = false;
bool isCapsLockOn = false;
bool isAltPressed = false; // *** 新增 ***
bool debugMode = true;

// --- 函式原型宣告 ---
void updateKeyboardAndLeds_TwoPulse();
byte myShiftIn(uint8_t dataPin, uint8_t clockPin);
void processKeyEvent(int r, int c, bool isPressed);
void printKeyboardStateMatrix();

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("\n--- Final Keyboard Firmware with ALT key ---");
  Serial.println("Keymap updated. Defaulting to DEBUG MODE.");
  Serial.println("Press Right_Shift + Ctrl to toggle mode.");

  pinMode(DATA_OUT_PIN, OUTPUT);
  pinMode(LATCH_PIN,    OUTPUT);
  pinMode(CLOCK_PIN,    OUTPUT);
  pinMode(DATA_IN_PIN,  INPUT);

  digitalWrite(LATCH_PIN, LOW);
  digitalWrite(CLOCK_PIN, LOW);
  
  updateKeyboardAndLeds_TwoPulse();
  memcpy(lastKeyState, keyState, sizeof(keyState));
}

void loop() {
  updateKeyboardAndLeds_TwoPulse();

  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      if (keyState[r][c] != lastKeyState[r][c]) {
        processKeyEvent(r, c, keyState[r][c]);
      }
    }
  }

  // LED 1 指示 Caps Lock, LED 2 指示 Debug Mode
  ledState[0] = isCapsLockOn;
  ledState[1] = debugMode; 

  byte ledData = 0;
  if (ledState[0]) bitSet(ledData, 0);
  if (ledState[1]) bitSet(ledData, 1);
  byte rowScanData = 0x00;
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_OUT_PIN, CLOCK_PIN, MSBFIRST, ledData);
  shiftOut(DATA_OUT_PIN, CLOCK_PIN, MSBFIRST, rowScanData);
  digitalWrite(LATCH_PIN, HIGH);

  memcpy(lastKeyState, keyState, sizeof(keyState));
  delay(10);
}

/**
 * @brief 處理單個按鍵事件的核心函數
 */
void processKeyEvent(int r, int c, bool isPressed) {
  // --- 模式切換檢查 ---
  if (isPressed) {
    bool ctrl_l_pressed = keyState[2][6];
    bool shift_r_pressed = keyState[3][5];
    
    if ((keymap_base[r][c] == KEY_CTRL_L && shift_r_pressed) || 
        (keymap_base[r][c] == KEY_SHIFT_R && ctrl_l_pressed)) {
      debugMode = !debugMode;
      Serial.print("\n\n--- Switched to "); Serial.print(debugMode ? "DEBUG MODE" : "NORMAL TYPING MODE"); Serial.println(" ---\n");
      isShiftPressed = false;
      return; 
    }
  }

  // --- 根據模式選擇行為 ---
  if (debugMode) {
    if (isPressed) { printKeyboardStateMatrix(); Serial.print("Key Pressed: ("); Serial.print(r); Serial.print(","); Serial.print(c); Serial.println(")");} 
    else { printKeyboardStateMatrix(); }
  } else {
    // === 正常打字模式 ===
    bool isLetter = (keymap_base[r][c] >= 'a' && keymap_base[r][c] <= 'z');
    bool shouldBeCapital = isShiftPressed ^ isCapsLockOn;
    
    char keyCode;
    if (isLetter && shouldBeCapital) {
      keyCode = keymap_shifted[r][c];
    } else {
      keyCode = isShiftPressed ? keymap_shifted[r][c] : keymap_base[r][c];
    }

    // --- 處理功能鍵 (按下時) ---
    if (isPressed) {
      switch (keymap_base[r][c]) { // 使用 base 表來識別功能鍵
        case KEY_CAPS_LOCK: isCapsLockOn = !isCapsLockOn; Serial.print("[CapsLock "); Serial.print(isCapsLockOn ? "ON" : "OFF"); Serial.print("]"); return;
        case KEY_ESC: Serial.print("[ESC]"); return;
        case KEY_TAB: Serial.print(isShiftPressed ? "-" : "\t"); return;
        case KEY_UP_ARROW: Serial.print("[UP]"); return;
        case KEY_DOWN_ARROW: Serial.print("[DOWN]"); return;
        case KEY_LEFT_ARROW: Serial.print("[LEFT]"); return;
        case KEY_RIGHT_ARROW: Serial.print("[RIGHT]"); return;
        case KEY_PGUP: Serial.print("[PGUP]"); return;
        case KEY_PGDOWN: Serial.print("[PGDN]"); return;
      }
    }

    // --- 處理修飾鍵和常規按鍵 ---
    // *** 修改: 新增對 KEY_ALT 的處理 ***
    switch (keyCode) {
      case KEY_SHIFT_R: isShiftPressed = isPressed; return;
      case KEY_CTRL_L: /* 可以在此添加對 Ctrl 的處理 */ return;
      case KEY_ALT: isAltPressed = isPressed; Serial.print(isPressed ? "[ALT down]" : "[ALT up]"); return;
    }

    if (isPressed) {
      // 在此處可以添加對 ALT+Key 組合的處理
      if (isAltPressed) {
          Serial.print("[ALT]+");
          // 例如: if (keyCode == 'x') { // 執行某個快捷操作 }
      }

      switch (keyCode) {
        case KEY_NULL: break;
        case KEY_ENTER: Serial.println(); break;
        case KEY_BACKSPACE: Serial.print("\b \b"); break;
        default:
          if (keyCode > 0 && keyCode < 200) { Serial.print(keyCode); }
          break;
      }
    }
  }
}

/**
 * @brief 核心函數：掃描鍵盤，包含水平顛倒邏輯
 */
void updateKeyboardAndLeds_TwoPulse() {
  for (int row = 0; row < 8; row++) {
    byte rowScanData = (1 << row);
    byte ledData = 0;
    digitalWrite(LATCH_PIN, LOW);
    shiftOut(DATA_OUT_PIN, CLOCK_PIN, MSBFIRST, ledData);
    shiftOut(DATA_OUT_PIN, CLOCK_PIN, MSBFIRST, rowScanData);
    digitalWrite(LATCH_PIN, HIGH); 
    delayMicroseconds(5);
    digitalWrite(LATCH_PIN, LOW);
    delayMicroseconds(1);
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
  Serial.print("\n\n");
  Serial.println("--- Key State Matrix ---");
  Serial.println("      Cols: 0 1 2 3 4 5 6 7");
  Serial.println("      -----------------------");
  for (int r = 0; r < 8; r++) {
    Serial.print("Row "); Serial.print(r); Serial.print(": | ");
    for (int c = 0; c < 8; c++) {
      if(keyState[r][c] != lastKeyState[r][c]) { Serial.print(keyState[r][c] ? "*X " : "*_ "); } 
      else { Serial.print(keyState[r][c] ? "X " : ". "); }
    }
    Serial.println("|");
  }
  Serial.println("      -----------------------");
  Serial.print("LEDs: [1]CapsLock:"); Serial.print(isCapsLockOn ? "ON" : "OFF");
  Serial.print(" [2]Debug:"); Serial.println(debugMode ? "ON" : "OFF");
}

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