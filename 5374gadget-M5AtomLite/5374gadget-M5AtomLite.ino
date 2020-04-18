#include "M5Atom.h"
#include <WiFi.h>
#include <HTTPClient.h>
#define JST     3600* 9

// 燃やすごみ, 燃やさないごみ、資源, あきびん
enum GARBAGE {
  notgarbage,
  burnable,
  unburnable,
  recyclable,
  bottle,
} today;

/*
// orinal color
#define COLOR_NOTGARBAGE 0xffffff // white
#define COLOR_BURNABLE   0xff0000 // red
#define COLOR_UNBURNABLE 0xff00ff // purple
#define COLOR_RECYCLABLE 0x00ff00 // green
#define COLOR_BOTTLE     0x00ff80 // emerald green
#define COLOR_IDLE       0x00ff00 // green
*/

// modified color: M5Atom libraryのバグ?(RとGが逆)
#define COLOR_NOTGARBAGE 0xffffff // white
#define COLOR_BURNABLE   0x00ff00 // red
#define COLOR_UNBURNABLE 0x00ffff // purple
#define COLOR_RECYCLABLE 0xff0000 // green
#define COLOR_BOTTLE     0xff0080 // emerald green
#define COLOR_IDLE       0xff0000 // green


// ★★★★★設定項目★★★★★★★★★★
const char* ssid     = "xxxxxxxx";       // 自宅のWiFi設定
const char* password = "xxxxxxxx";
int start_oclock = 6;   // 通知を開始する時刻
int start_minute = 0;
int end_oclock   = 9;   // 通知を終了する時刻
int end_minute   = 0;
// 以下のURLにあるエリア番号を入れる
//https://github.com/PhalanXware/scraped-5374/blob/master/save.json
int area_number = 30;    // 地区の番号（例：浅野 0, 浅野川 1）
// ★★★★★★★★★★★★★★★★★★★

// the setup routine runs once when M5Stack starts up
void setup() {

  M5.begin(true, false, true);
  delay(10);
  M5.dis.drawpix(0, 0x000000);

  // シリアル設定
  Serial.begin(115200);
  Serial.println("");

  // WiFi接続
  wifiConnect();
  delay(1000);

  // NTP同期
  configTime( JST, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");

  // 今日のデータの読み出し
  today = notgarbage;
  updateGarbageDay();

#if 0 // テスト用
  while (1) {
    // 燃やすごみ（赤）
    M5.dis.drawpix(0, COLOR_BURNABLE);
    delay(500);
    M5.dis.drawpix(0, 0x000000);
    delay(500);
    // 燃やさないごみ（紫）
    M5.dis.drawpix(0, COLOR_UNBURNABLE);
    delay(500);
    M5.dis.drawpix(0, 0x000000);
    delay(500);
    // 資源ごみ（緑）
    M5.dis.drawpix(0, COLOR_RECYCLABLE);
    delay(500);
    M5.dis.drawpix(0, 0x000000);
    delay(500);
    // あきびん（エメラルドグリーン）
    M5.dis.drawpix(0, COLOR_BOTTLE);
    delay(500);
    M5.dis.drawpix(0, 0x000000);
    delay(500);
  }
#endif

}

void loop() {
  time_t t;
  struct tm *tm;
  static const char *wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};

  t = time(NULL);
  tm = localtime(&t);

  /*
    Serial.printf(" %04d/%02d/%02d(%s) %02d:%02d:%02d\n",
                  tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                  wd[tm->tm_wday],
                  tm->tm_hour, tm->tm_min, tm->tm_sec);
  */

  // STAモードで接続出来ていない場合
  if (WiFi.status() != WL_CONNECTED) {
    M5.dis.drawpix(0, 0xffffff);
    wifiConnect();
  }

  // 時間のチェック(24H表記)
  if ((start_oclock < tm->tm_hour) && (tm->tm_hour < end_oclock))
  {
    onLed();
  }
  else if (start_oclock == tm->tm_hour)
  { // 開始時刻の分の判定
    if (start_minute <= tm->tm_min)
    {
      onLed();
    }
    else Idle();
  }
  else if (end_oclock == tm->tm_hour)
  { // 終了時刻の分の判定
    if (end_minute >= tm->tm_min)
    {
      onLed();
    }
    else Idle();
  }
  else Idle();
  
  // 夜中の３時にデータを更新
  if (tm->tm_hour == 3)
  { // 当日の捨てれるゴミ情報をアップデート
    if ((tm->tm_min == 0) || (tm->tm_min == 20) || (tm->tm_min == 40))
    {
      updateGarbageDay();
      delay(70000); // 余裕を見て、70秒後に変更
    }
  }

  // 時間待ち
  delay(100);
  M5.update();
}

void Idle()
{
  // 待機中は生存確認のために、短く緑フラッシュ(5秒周期)
  M5.dis.drawpix(0, COLOR_IDLE);
  delay(10);
  M5.dis.drawpix(0, 0x000000);
  delay(5000 - 110);
}

void wifiConnect() {
  Serial.print("Connecting to " + String(ssid));

  //WiFi接続開始
  WiFi.begin(ssid, password);

  //接続を試みる(30秒)
  for (int i = 0; i < 60; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      //接続に成功。IPアドレスを表示
      Serial.println();
      Serial.print("Connected! IP address: ");
      Serial.println(WiFi.localIP());
      break;
    } else {
      Serial.print(".");
      delay(500);
    }
  }

  // WiFiに接続出来ていない場合
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("");
    Serial.println("Failed, Wifi connecting error");
  }

}

void onLed(void) {
  // 点灯パターン
  if (today == burnable) {
    // 燃やすごみ（赤）
    M5.dis.drawpix(0, COLOR_BURNABLE);
    delay(500);
    M5.dis.drawpix(0, 0x000000);
    delay(500);
  } else if (today == unburnable) {
    // 燃やさないごみ（紫）
    M5.dis.drawpix(0, COLOR_UNBURNABLE);
    delay(500);
    M5.dis.drawpix(0, 0x000000);
    delay(500);
  } else if (today == recyclable) {
    // 資源ごみ（緑）
    M5.dis.drawpix(0, COLOR_RECYCLABLE);
    delay(500);
    M5.dis.drawpix(0, 0x000000);
    delay(500);
  } else if (today == bottle) {
    // あきびん（エメラルドグリーン）
    M5.dis.drawpix(0, COLOR_BOTTLE);
    delay(500);
    M5.dis.drawpix(0, 0x000000);
    delay(500);
  } else {
    // 収集日以外は、生存確認のために短く白フラッシュ
    M5.dis.drawpix(0, COLOR_NOTGARBAGE);
    delay(10);
  }
}

// ゴミの日の情報のアップデート
void updateGarbageDay(void) {
  // ゴミ情報の読み出し
  HTTPClient https;

  // "area"とJSONファイルのNo.のずれはここで吸収する
  String url = "https://raw.githubusercontent.com/PhalanXware/scraped-5374/master/save_" + String(area_number + 1) + ".json";
  Serial.print("connect url :");
  Serial.println(url);

  Serial.print("[HTTPS] begin...\n");
  if (https.begin(url)) {  // HTTPS
    //if (https.begin(*client, url)) {  // HTTPS

    Serial.print("[HTTPS] GET...\n");
    // start connection and send HTTP header
    int httpCode = https.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
      //Serial.println(https.getSize());

      // file found at server
      String payload;
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        payload = https.getString();
        Serial.println("HTTP_CODE_OK");
        //Serial.println(payload);
      }

      String html[10] = {"\0"};
      int index = split(payload, '\n', html);
      String garbageDays = {"\0"};
      garbageDays = html[5];
      Serial.println(garbageDays);

      if (garbageDays.indexOf("今日") > 0) {
        if (garbageDays.indexOf("燃やすごみ") > 0) {
          today = burnable;
        } else if (garbageDays.indexOf("燃やさないごみ") > 0) {
          today = unburnable;
        } else if (garbageDays.indexOf("資源") > 0) {
          today = recyclable;
        } else if (garbageDays.indexOf("あきびん") > 0) {
          today = bottle;
        } else {
          today = notgarbage;
        }
      } else {
        today = notgarbage;
      }

      Serial.print("今日は、");
      Serial.println(today);

    } else {
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }
    https.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
}

// 文字列の分割処理
int split(String data, char delimiter, String *dst) {
  int index = 0;
  int arraySize = (sizeof(data) / sizeof((data)[0]));
  int datalength = data.length();
  for (int i = 0; i < datalength; i++) {
    char tmp = data.charAt(i);
    if ( tmp == delimiter ) {
      index++;
      if ( index > (arraySize - 1)) return -1;
    }
    else dst[index] += tmp;
  }
  return (index + 1);
}
