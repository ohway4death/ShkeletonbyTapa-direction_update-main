#include <M5Core2.h>
#include "MFRC522_I2C.h"
#include <Adafruit_NeoPixel.h>

#include <M5GFX.h>

#include <driver/i2s.h>
#include <WiFi.h>

#include "aura.h"
#include "beamcharge.h"
#include "charge.h"
#include "drumroll.h"
#include "wafu.h"

#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>

/**RFID**/
MFRC522_I2C mfrc522(0x28, -1); // Create MFRC522 instance.

// #define ace   "04 28 c2 9a"
#define ace   "1d f4 df 06"
#define jack  "04 28 bf 9a"
#define queen "04 28 c0 9a"
#define king  "04 28 c1 9a"
#define ten   "04 28 be 9a"
#define joker "04 28 bd 9a"

/**LED**/
#define PIN 26
#define NUMPIXELS 21                                            // LEDの数を指定
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800); // 800kHzでNeoPixelを駆動

int ledBrightness = 0; // LEDの明るさカウンタ
int ledPosition = 0;   // LEDの位置カウンタ
int LEDcnt =0;
int LEDaction = 1;
int LED_VAL = 40;

// LED番号再割り当て
int thumbLine[3] = {0, 1, 2};
int indexFingerLine[5] = {7, 6, 5, 4, 3};
int middleFingerLine[5] = {8, 9, 10, 11, 12};
int ringFingerLine[5] = {17, 16, 15, 14, 13};
int littleFingerLine[3] = {18, 19, 20};

// 演出制御
bool isCard = false; // カード読み取りフラグ
int CardID = 0;
int LCDaction = 1;
float TotalTime = 4000; // 演出合計時間(ms)

unsigned long startMillis = 0;
unsigned long currentMillis = 0;
unsigned long previousLEDTime = 0;
unsigned long previousLCDTime = 0;

// 待機画面
int displayControl = 0;               // 0：初期画面設定 1：初期画面 2：回転演出設定 3：回転演出 4：スプライト設定 5：スプライト演出
M5GFX lcd;                      // LGFXのインスタンスを作成。
M5Canvas spriteBase(&lcd);         // スプライトを使う場合はM5Canvasのインスタンスを作成。
M5Canvas spriteSub(&spriteBase);   // スプライトを使う場合はM5Canvasのインスタンスを作成。
M5Canvas spriteWord1(&spriteBase); // スプライトを使う場合はM5Canvasのインスタンスを作成。
M5Canvas spriteWord2(&spriteBase); // スプライトを使う場合はM5Canvasのインスタンスを作成。
M5Canvas spriteWord3(&spriteBase); // スプライトを使う場合はM5Canvasのインスタンスを作成。
M5Canvas spriteWord4(&spriteBase); // スプライトを使う場合はM5Canvasのインスタンスを作成。
void wait_display_setup();
void wait_display();
void reset_display();
void delete_sprite();

// トランプ画像のパスを返す。
String trump_set(int cardID);

// 回転演出
void rotate_display(float diff, int cardId);
void rotate_display_setup();

// 回転演出 ver2
static constexpr unsigned short imageWidth = 320;
static constexpr unsigned short imageHeight = 218;
static uint32_t lcd_width;
static uint32_t lcd_height;
static int_fast16_t sprite_height = 80;
static LGFX_Sprite sprites[2];
static LGFX_Sprite trump;
int_fast16_t center_y = 120;
void rotate_display_v2_setup();
void rotate_display_v2(float diff, int cardId);

// 穴あき演出
void MaskReveal_Sphere(int cardId);
int getRandomValue(int min, int max);
std::vector<int> myList;
void MaskReveal_Sphere_setup();

// カーテン的な
void MaskReveal_Rectangle(int cardId);
int count;

// 加速度リセット
float baseAccX, baseAccY, baseAccZ = 0; // 基準加速度格納用
float accX, accY, accZ;                 // 加速度格納用
float diffAccX, diffAccY, diffAccZ = 0; // 現在値格納用
float maxAccX, maxAccY, maxAccZ = 0;
bool reset_flg = false;
void check_acc();
void zeroSet();
boolean shakeReset();

// 効果音
#define SPEAKER_I2S_NUMBER I2S_NUM_0
const unsigned char *wavList[] = {aura, beamcharge, charge, drumroll, wafu};
const size_t wavSize[] = {sizeof(aura), sizeof(beamcharge), sizeof(charge), sizeof(drumroll), sizeof(wafu)};

// マルチタスク
TaskHandle_t task1Handle;
QueueHandle_t xQueue1;
#define QUEUE1_LENGTH 5

// プロトタイプ宣言
void Fingertip2Wrist(int ledPosition, int ledBrightness);
void Fingertip2WristPlus(int ledPosition_a, int ledBrightness_a, int ledcnt);
void RainbowMove(int ledPosition, int ledBrightness, int ledcnt);
uint32_t Wheel(byte WheelPos);
void rainbow(uint8_t wait, int Rainbowcnt) ;

bool isNewCard();
int identifyCard();
void LEDcontrol(int B, unsigned long C, unsigned long D, int actionID);
void LCDcontrol(int B, unsigned long C, unsigned long D, int actionID);
void uid_display_proc();
void acc_setup();

void sound_effect_setup();
void SEcontrol();
void InitI2SSpeakerOrMic(int mode);
void multi_task_setup();

// ---------------------------------------------------------------
void setup()
{
  pixels.begin(); // NeoPixelの初期化
  pixels.clear(); // NeoPixelのリセット

  Serial.begin(115200); // シリアル通信の開始

  M5.begin();
  Wire.begin();
  mfrc522.PCD_Init();

  wait_display_setup(); // 待機画面SCANのsetup

  acc_setup();          // shake_resetのためのsetup
  sound_effect_setup(); // 効果音のためのsetup

  if (displayControl == 0){
    wait_display_setup(); // 待機画面SCANのsetup
    displayControl = 1;
  }

  xQueue1 = xQueueCreate(QUEUE1_LENGTH, sizeof(boolean));
}

// ---------------------------------------------------------------
void loop()
{
  currentMillis = millis(); // 現在のミリ秒を取得

  if (displayControl == 0){
    delete_sprite();
    wait_display_setup();
    displayControl = 1;
  }

  if (!isCard)
  {
    pixels.clear(); // NeoPixelのリセット
    pixels.show();  // NeoPixelのリセット
    if (displayControl == 1) {
      wait_display(); // 待機画面
    }
    if (isNewCard())
    {
      CardID = identifyCard();
      // CardID = 2;
      isCard = true;
      static bool data = true;
      xQueueSend(xQueue1, &data, 0);
      reset_display();
      count = 0;
      LEDcnt = 0;
      startMillis = currentMillis;
      MaskReveal_Sphere_setup();
      LCDaction = random(2, 4); // 演出を指定
      LEDaction = random(1, 4);
    }
  }
  else
  {
    LCDaction = 5;
    LEDcontrol(1, startMillis, currentMillis, LEDaction);
    LCDcontrol(CardID, startMillis, currentMillis, LCDaction);

    multi_task_setup();

    if ((currentMillis - startMillis) > TotalTime)
    {
      zeroSet();
      while (!shakeReset())
        isCard = false; // isCardをbooleanで更新
        displayControl = 0;
    }
  }
}

// ---------------------------------------------------------------
bool isNewCard()
{ // カード有無を判定
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
  { // カード読み取り関数
    return false;
  }
  else
  {
    // uid_display_proc();
    return true;
  }
}
// ---------------------------------------------------------------
int identifyCard()
{
  String strBuf[mfrc522.uid.size];
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    strBuf[i] = String(mfrc522.uid.uidByte[i], HEX);

    if (strBuf[i].length() == 1)
    {
      strBuf[i] = "0" + strBuf[i];
    }
  }
  String strUID = strBuf[0] + " " + strBuf[1] + " " + strBuf[2] + " " + strBuf[3];

  Serial.printf("%s", strUID);

  // カードを増やしたい場合はdefine増やしてからここに追加
  // switch文でstringの比較ができない
  // カードを増やしたい場合はdefine増やしてからここに追加
  if (strUID.equalsIgnoreCase(ace))
  {
    return 1;
  }
  else if (strUID.equalsIgnoreCase(ten))
  {
    return 10;
  }
  else if (strUID.equalsIgnoreCase(jack))
  {
    return 11;
  }
  else if (strUID.equalsIgnoreCase(queen))
  {
    return 12;
  }
  else if (strUID.equalsIgnoreCase(king))
  {
    return 13;
  }
  else if (strUID.equalsIgnoreCase(joker))
  {
    return 0;
  }
  else
  {
    return 1; // test用
  }
}
// ---------------------------------------------------------------
void LEDcontrol(int ID, unsigned long StartTime, unsigned long CurrentTime , int LEDpattern)
{
  //if ((CurrentTime - StartTime) < 100)
  //{
  //}
  //ID = 2;
  switch (LEDpattern)
  {
  case 1:
    if ((CurrentTime - previousLEDTime) > 100)
    { // 100ms間隔で更新
      Fingertip2Wrist(ledPosition, ledBrightness);
      previousLEDTime = CurrentTime;
      // M5.Lcd.println(previousLEDTime);

      ledBrightness += 10;
      ledPosition += 1;
      if (ledPosition >= 5)
      {
        ledPosition = 0;
      }
      if (ledBrightness >= 250)
      {
        ledBrightness = 250;
      }
      break;

  case 2:
    if ((CurrentTime - previousLEDTime) > 100)
    { // 100ms間隔で更新
      Fingertip2WristPlus(ledPosition, ledBrightness, LEDcnt);
      previousLEDTime = CurrentTime;
      // M5.Lcd.println(previousLEDTime);

      ledBrightness += 10;
      ledPosition += 1;
      if (ledPosition >= 5)
      {
        ledPosition = 0;
        LEDcnt +=1;
      }
      if (ledBrightness >= 250)
      {
        ledBrightness = 250;
      }
      /*for(){
        Fingertip2Wrist(ledPosition, ledBrightness);
      }*/
    }
      break;

  case 3:
    if ((CurrentTime - previousLEDTime) > 20)
    { // 100ms間隔で更新
      RainbowMove(ledPosition, ledBrightness, LEDcnt);
      previousLEDTime = CurrentTime;
      // M5.Lcd.println(previousLEDTime);

      ledBrightness += 10;
      ledPosition += 1;
      LEDcnt +=1;
      if (ledPosition >= 5)
      {
        ledPosition = 0;
        //LEDcnt +=1;
      }
      if (ledBrightness >= 250)
      {
        ledBrightness = 250;
      }
      /*for(){
        Fingertip2Wrist(ledPosition, ledBrightness);
      }*/
    }
      break;
    default:
      break;
    }
  }
}
// ---------------------------------------------------------------
void LCDcontrol(int ID, unsigned long StartTime, unsigned long CurrentTime, int actionID)
{
  if (StartTime == CurrentTime)
  {
    // ledBrightness =100;
    // ledPosition =0;
  }

  switch (actionID)
  {
  case 1:
    if ((CurrentTime - previousLCDTime) > 100)
    { // 100ms間隔で更新
      previousLCDTime = CurrentTime;
    }
    // M5.Lcd.println(previousLEDTime);
    break;

  case 2:
    if ((CurrentTime - previousLCDTime) > 100)
    { // 100ms間隔で更新
      // Serial.printf("--------------------------- %d \n", count);
      float diff = currentMillis - startMillis;
      previousLCDTime = CurrentTime;
      //count+=100;
      rotate_display(diff, ID);
    }
    break;

  case 3:
    if ((CurrentTime - previousLCDTime) > 100)
    { // 100ms間隔で更新
      float diff = currentMillis - startMillis;
      previousLCDTime = CurrentTime;
      MaskReveal_Sphere(ID);
    }
    break;
  case 4:
    if ((CurrentTime - previousLCDTime) > 100)
    { // 100ms間隔で更新
      float diff = currentMillis - startMillis;
      previousLCDTime = CurrentTime;
      MaskReveal_Rectangle(ID);
    }
    break;
  
  case 5:
    if (displayControl == 1)
    {
      displayControl = 4;
      delete_sprite();
      rotate_display_v2_setup();
      displayControl = 5;
    }
    if ((CurrentTime - previousLCDTime) > 100)
    { // 100ms間隔で更新
      float diff = currentMillis - startMillis;
      previousLCDTime = CurrentTime;
      rotate_display_v2(diff, ID);
    }
    break;

  default:
    break;
  }
}

// ---------------------------------------------------------------
void Fingertip2Wrist(int ledPosition_a, int ledBrightness_a)
{
  pixels.clear(); // NeoPixelのリセット
  if (ledPosition_a < 2)
  {
    pixels.setPixelColor(indexFingerLine[ledPosition_a], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));  // i番目の色を設定
    pixels.setPixelColor(middleFingerLine[ledPosition_a], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a)); // i番目の色を設定
    pixels.setPixelColor(ringFingerLine[ledPosition_a], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));   // i番目の色を設定
  }
  else
  {
    pixels.setPixelColor(thumbLine[ledPosition_a - 2], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));        // i番目の色を設定
    pixels.setPixelColor(indexFingerLine[ledPosition_a], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));      // i番目の色を設定
    pixels.setPixelColor(middleFingerLine[ledPosition_a], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));     // i番目の色を設定
    pixels.setPixelColor(ringFingerLine[ledPosition_a], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));       // i番目の色を設定
    pixels.setPixelColor(littleFingerLine[ledPosition_a - 2], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a)); // i番目の色を設定
  }
  pixels.setBrightness(LED_VAL); // 0~255の範囲で明るさを設定
  pixels.show();            // LEDに色を反映

  // delay(100);
}
// ---------------------------------------------------------------
void Fingertip2WristPlus(int ledPosition_a, int ledBrightness_a, int ledcnt)
{
  pixels.clear(); // NeoPixelのリセット
  if (ledPosition_a < 2)
  {
    pixels.setPixelColor(indexFingerLine[ledPosition_a], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));  // i番目の色を設定
    pixels.setPixelColor(middleFingerLine[ledPosition_a], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a)); // i番目の色を設定
    pixels.setPixelColor(ringFingerLine[ledPosition_a], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));   // i番目の色を設定
  }
  else
  {
    pixels.setPixelColor(thumbLine[ledPosition_a - 2], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));        // i番目の色を設定
    pixels.setPixelColor(indexFingerLine[ledPosition_a], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));      // i番目の色を設定
    pixels.setPixelColor(middleFingerLine[ledPosition_a], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));     // i番目の色を設定
    pixels.setPixelColor(ringFingerLine[ledPosition_a], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));       // i番目の色を設定
    pixels.setPixelColor(littleFingerLine[ledPosition_a - 2], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a)); // i番目の色を設定
  }

  for(int j =0;j<ledcnt;j++){

  if (j>0 && j <= 3)
  {
    pixels.setPixelColor(thumbLine[3-j], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));        // i番目の色を設定
    pixels.setPixelColor(indexFingerLine[5-j], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));  // i番目の色を設定
    pixels.setPixelColor(middleFingerLine[5-j], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a)); // i番目の色を設定
    pixels.setPixelColor(ringFingerLine[5-j], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));   // i番目の色を設定
    pixels.setPixelColor(littleFingerLine[3-j], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a)); // i番目の色を設定
  }
  else if(j > 3)
  {
    pixels.setPixelColor(indexFingerLine[5-j], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));      // i番目の色を設定
    pixels.setPixelColor(middleFingerLine[5-j], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));     // i番目の色を設定
    pixels.setPixelColor(ringFingerLine[5-j], pixels.Color(ledBrightness_a / 2, ledBrightness_a / 3, ledBrightness_a));       // i番目の色を設定
  }   
  } 



  pixels.setBrightness(LED_VAL); // 0~255の範囲で明るさを設定
  pixels.show();            // LEDに色を反映

  // delay(100);
}
// ---------------------------------------------
void RainbowMove(int ledPosition_a, int ledBrightness_a, int ledcnt)
{
  //pixels.clear(); // NeoPixelのリセット
rainbow(0, ledcnt);
}
// ---------------------------------------------
// LEDを連続的に虹色に変化させる関数
void rainbow(uint8_t wait, int cntRainbow) {
    uint16_t i;
    //for (j = 0; j < 256; j++) {
      for (i = 0; i < pixels.numPixels(); i++) {
        pixels.setPixelColor(i, Wheel((i*3 + cntRainbow*30) & 255));
      }
      pixels.show();
      delay(wait);
    //}
}
// ---------------------------------------------
// 色の移り変わりはR(赤)→G(緑)→B(青)からR(赤)に戻ります。
uint32_t Wheel(byte WheelPos) {
  if (WheelPos < 85) {
       return pixels.Color((WheelPos * 3) * LED_VAL / 255, (255 - WheelPos * 3) * LED_VAL / 255, 0);
  } else if (WheelPos < 170) {
       WheelPos -= 85;
       return pixels.Color((255 - WheelPos * 3) * LED_VAL / 255, 0, (WheelPos * 3) * LED_VAL / 255);
  } else {
       WheelPos -= 170;
       return pixels.Color(0, (WheelPos * 3) * LED_VAL / 255, (255 - WheelPos * 3) * LED_VAL/ 255);
  }
}
// ---------------------------------------------------------------


// ---------------------------------------------------------------
void uid_display_proc()
{
  short aa[] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  char buf[60];
  sprintf(buf, "mfrc522.uid.size = %d", mfrc522.uid.size);
  Serial.println(buf);
  for (short it = 0; it < mfrc522.uid.size; it++)
  {
    aa[it] = mfrc522.uid.uidByte[it];
  }

  sprintf(buf, "%02x %02x %02x %02x", aa[0], aa[1], aa[2], aa[3]);
  M5.Lcd.println(buf);
  M5.Lcd.println("");
  Serial.println(buf);
}

// ---------------------------------------------------------------
// wait_display_setup
void wait_display_setup()
{
  lcd.init();                   // LCD初期化
  spriteSub.setColorDepth(8);   // カラーモード設定
  spriteBase.setColorDepth(8);  // カラーモード設定
  spriteWord1.setColorDepth(8); // カラーモード設定
  spriteWord2.setColorDepth(8); // カラーモード設定
  spriteWord3.setColorDepth(8); // カラーモード設定
  spriteWord4.setColorDepth(8); // カラーモード設定

  spriteWord1.setTextSize(6); // 文字サイズ42px
  spriteWord2.setTextSize(6); // 文字サイズ42px
  spriteWord3.setTextSize(6); // 文字サイズ42px
  spriteWord4.setTextSize(6); // 文字サイズ42px

  spriteSub.createSprite(320, 206);
  spriteBase.createSprite(320, 240);
  spriteWord1.createSprite(80, 80);
  spriteWord2.createSprite(80, 80);
  spriteWord3.createSprite(80, 80);
  spriteWord4.createSprite(80, 80);

  spriteWord1.setCursor(24, 24);
  spriteWord2.setCursor(24, 24);
  spriteWord3.setCursor(24, 24);
  spriteWord4.setCursor(24, 24);

  spriteWord1.printf("S");
  spriteWord2.printf("C");
  spriteWord3.printf("A");
  spriteWord4.printf("N");
}
// ---------------------------------------------------------------
// 待機画面の表示
void wait_display()
{
  spriteBase.fillScreen(BLACK); // 画面の塗りつぶし
  spriteWord1.pushRotateZoom(280, 120, 90, 1, 1);
  spriteWord2.pushRotateZoom(200, 120, 90, 1, 1);
  spriteWord3.pushRotateZoom(120, 120, 90, 1, 1);
  spriteWord4.pushRotateZoom(40, 120, 90, 1, 1);
  spriteBase.pushSprite(&lcd, 0, 0);
}
// ---------------------------------------------------------------
void reset_display()
{
  spriteBase.fillScreen(BLACK);
  spriteSub.fillScreen(BLACK);
  spriteBase.pushSprite(&lcd, 0, 0);
}
// ---------------------------------------------------------------
// スプライトの全削除
void delete_sprite()
{
  spriteBase.deleteSprite();
  spriteWord1.deleteSprite();
  spriteWord2.deleteSprite();
  spriteWord3.deleteSprite();
  spriteWord4.deleteSprite();
  spriteSub.deleteSprite();

  sprites[0].deleteSprite();
  sprites[1].deleteSprite();
}
// ---------------------------------------------------------------
// 回転演出
int i = 0;
int mode = 1;
void rotate_display_setup()
{
  spriteSub.setColorDepth(8); // カラーモード設定
  spriteBase.setColorDepth(8);  // カラーモード設定

  void *p1 = spriteSub.createSprite(320, 218);
  spriteBase.createSprite(320, 240);
}
void rotate_display(float diff, int cardId)
{
  spriteSub.drawJpgFile(SD, trump_set(cardId), 0, 0);
  int round_diff = int(diff) / 100 * 100;

  Serial.printf("--------------------------- \n");
  Serial.printf("startMillis = %d, currentMillis = %d, diff = %d\n", startMillis, currentMillis, (currentMillis - startMillis));
  Serial.printf("%d \n", int(diff) / 100 * 100);

  spriteBase.fillScreen(BLACK); // 画面の塗りつぶし
  spriteSub.pushRotateZoom(160, 120, round_diff * 360 / (TotalTime - 100), round_diff * 1 / (TotalTime - 100), round_diff * 1 / (TotalTime - 100));
  spriteBase.pushSprite(0, 0);
}
// ---------------------------------------------------------------
// 回転演出 ver2
void rotate_display_v2_setup()
{
  spriteSub.setColorDepth(8); // カラーモード設定
  void *p1 = spriteSub.createSprite(imageWidth, imageHeight);

  // ディスプレイの幅と高さを格納
  lcd_width = lcd.width();
  lcd_height = lcd.height();

  sprites[0].setColorDepth(lcd.getColorDepth());
  sprites[1].setColorDepth(lcd.getColorDepth());
  void *p2 = sprites[0].createSprite(lcd_width, sprite_height);
  void *p3 = sprites[1].createSprite(lcd_width, sprite_height);
}
void rotate_display_v2(float diff, int cardId)
{
  static uint8_t flip = 0;
  float round_diff = round(diff / 100) * 100; // きっちり5000で回転が終わるように調整

  switch (cardId)
  {
  case 1:
      spriteSub.drawJpgFile(SD, "/trump/card_spade_01.jpg", 0, 0);
      break;
  case 10:
      spriteSub.drawJpgFile(SD, "/trump/card_spade_10.jpg", 0, 0);
      break;
  case 11:
      spriteSub.drawJpgFile(SD, "/trump/card_spade_11.jpg", 0, 0);
      break;
  case 12:
      spriteSub.drawJpgFile(SD, "/trump/card_spade_12.jpg", 0, 0);
      break;
  case 13:
      spriteSub.drawJpgFile(SD, "/trump/card_spade_13.jpg", 0, 0);
      break;
  case 0:
      spriteSub.drawJpgFile(SD, "/trump/card_spade_05.jpg", 0, 0);
      break;
  }

  for (int_fast16_t y = 0; y < lcd_height; y += sprite_height)
  {
      flip = flip ? 0 : 1;
      sprites[flip].clear();

      spriteSub.pushRotateZoom(&sprites[flip], 160, center_y - y, round_diff * 360 / TotalTime, round_diff * 1 / TotalTime, round_diff * 1 / TotalTime);
      sprites[flip].pushSprite(&lcd, 0, y);
  }
}
// ---------------------------------------------------------------
// 加速度のセットアップ
void acc_setup()
{
  M5.IMU.Init();                     // 6軸センサ初期化
  M5.IMU.SetAccelFsr(M5.IMU.AFS_2G); // 加速度センサースケール初期値 ±2G(2,4,8,16)
}
// ---------------------------------------------------------------
// 加速度リセット用
void zeroSet()
{
  M5.update();
  M5.IMU.getAccelData(&accX, &accY, &accZ); // 加速度データ取得
  baseAccX = accX;                          // 取得値を補正値としてセット
  baseAccY = accY;
  baseAccZ = accZ;
}
// ---------------------------------------------------------------
// 端末を振ってリセット画面へ
boolean shakeReset()
{
  M5.update();
  M5.IMU.getAccelData(&accX, &accY, &accZ); // 加速度データ取得

  diffAccX = accX - baseAccX; // 補正後の数値を表示値としてセット
  diffAccY = accY - baseAccY;
  diffAccZ = accZ - baseAccZ;

  if (diffAccY > 0.5)
  {
    return true;
  }
  else
  {
    return false;
  }
}
// ---------------------------------------------------------------
// 効果音のセットアップ
void sound_effect_setup()
{
  M5.Axp.SetSpkEnable(true);
  InitI2SSpeakerOrMic(MODE_SPK);
}
// ---------------------------------------------------------------
// 効果音コントロール
void SEcontrol()
{
  int rand_int = random(5);
  size_t bytes_written;

  // rand_int = 0; // test用

  i2s_write(SPEAKER_I2S_NUMBER, wavList[rand_int], wavSize[rand_int], &bytes_written, portMAX_DELAY);
  i2s_zero_dma_buffer(SPEAKER_I2S_NUMBER);
}
// ---------------------------------------------------------------
void InitI2SSpeakerOrMic(int mode)
{
  esp_err_t err = ESP_OK;
  i2s_driver_uninstall(SPEAKER_I2S_NUMBER);
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER),
      .sample_rate = 16000,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ALL_RIGHT,
      .communication_format = I2S_COMM_FORMAT_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 6,
      .dma_buf_len = 60,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0};
  if (mode == MODE_MIC)
  {
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
  }
  else
  {
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  }
  err += i2s_driver_install(SPEAKER_I2S_NUMBER, &i2s_config, 0, NULL);
  i2s_pin_config_t tx_pin_config = {
      .bck_io_num = CONFIG_I2S_BCK_PIN,
      .ws_io_num = CONFIG_I2S_LRCK_PIN,
      .data_out_num = CONFIG_I2S_DATA_PIN,
      .data_in_num = CONFIG_I2S_DATA_IN_PIN,
  };
  err += i2s_set_pin(SPEAKER_I2S_NUMBER, &tx_pin_config);
  if (mode != MODE_MIC)
  {
    err += i2s_set_clk(SPEAKER_I2S_NUMBER, 16000, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
  }
  i2s_zero_dma_buffer(SPEAKER_I2S_NUMBER);
}
// ---------------------------------------------------------------
void task1(void *pvParameters)
{
  bool receivedData;
  if (xQueueReceive(xQueue1, &receivedData, 0) == pdTRUE)
  {
    if (receivedData == 1)
    {
      SEcontrol();
    }
  }

  task1Handle = NULL;
  vTaskDelete(NULL);
}
// ---------------------------------------------------------------
void multi_task_setup()
{
  xTaskCreateUniversal(
      task1,        // 作成するタスク関数
      "SE_task",    // 表示用タスク名
      8192,         // スタックメモリ量
      NULL,         // 起動パラメータ
      0,            // 優先度
      &task1Handle, // タスクハンドル
      0             // 実行するコア
  );
}
// ---------------------------------------------------------------
void MaskReveal_Sphere_setup()
{
  for (int i = 0; i <= 39; ++i)
  {
    myList.push_back(i);
  }
}
int getRandomValue(int min, int max)
{
  return min + rand() % (max - min + 1);
}

// ---------------------------------------------------------------
void MaskReveal_Sphere(int cardId)
{

  int randomIndex = getRandomValue(0, myList.size() - 1);
  int selectedValue = myList[randomIndex];

  // int x = random(360 - boxWidth);
  // int y = random(240 - boxHeight);

  int x = random(selectedValue % 8 * 40, selectedValue % 8 * 40 + 39);
  int y = random(selectedValue / 8 * 40, selectedValue / 8 * 40 + 39);

  spriteBase.fillScreen(BLACK); // 画面の塗りつぶし
  // spriteSub.fillScreen(BLACK);  // 画面の塗りつぶし

  spriteBase.drawJpgFile(SD, trump_set(cardId), 0, 17); // 絵をセット
  spriteSub.fillCircle(x, y, 60, RED);                                // 円を作成

  spriteSub.pushSprite(0, 17, RED); // REDを透明色にしてSubをBaseにプッシュ。カードの一部だけが見えるはず。
  spriteBase.pushSprite(0, 0);      // Baseを画面にプッシュ

  myList.erase(myList.begin() + randomIndex);
}
// ---------------------------------------------------------------
// カーテン的な感じにする
void MaskReveal_Rectangle(int cardId)
{

  int boxWidth = 3;    // 箱の幅
  int boxHeight = 320; // 箱の高さ

  int left_y = 100 - count * 3;
  int right_y = 103 + count * 3;

  spriteBase.fillScreen(BLACK); // 画面の塗りつぶし

  // if (count > 4)
  // {
  //   spriteBase.drawJpgFile(SD, "/real_trump/card_heart_01.jpg", 0, 17); // 絵をセット
  //   spriteSub.fillRect(0, left_y,  boxHeight, boxWidth, RED);       // 長方形
  //   spriteSub.fillRect(0, right_y, boxHeight, boxWidth, RED);       // 長方形
  //   spriteSub.pushSprite(0, 17, RED);                               // REDを透明色にしてSubをBaseにプッシュ。カードの一部だけが見えるはず。
  // }

  spriteBase.drawJpgFile(SD, trump_set(cardId), 0, 17); // 絵をセット
  spriteSub.fillRect(0, left_y, boxHeight, boxWidth, RED);            // 長方形
  spriteSub.fillRect(0, right_y, boxHeight, boxWidth, RED);           // 長方形
  spriteSub.pushSprite(0, 17, RED);                                   // REDを透明色にしてSubをBaseにプッシュ。カードの一部だけが見えるはず。

  spriteBase.pushSprite(0, 0); // Baseを画面にプッシュ

  count++;
}
// ---------------------------------------------------------------
String trump_set(int cardID)
{
  switch (cardID)
  {
  case 1:
    return "/real_trump/card_heart_01.jpg";
    break;
  case 10:
    return "/real_trump/card_heart_10.jpg";
    break;
  case 11:
    return "/real_trump/card_heart_11.jpg";
    break;
  case 12:
    return "/real_trump/card_heart_12.jpg";
    break;
  case 13:
    return "/real_trump/card_heart_13.jpg";
    break;
  case 0:
    return "/real_trump/card_heart_Joker.jpg";
    break;
  }
}
// ---------------------------------------------------------------
