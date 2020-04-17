#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <Adafruit_NeoPixel.h>
// https://github.com/pokiiio/EmotionalBlink
#include <EmotionalBlink.h>

#define JST     3600* 9

#define PIN         4
#define NUMPIXELS   1
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// HTML用バッファ
#define BUFFER_SIZE 5120
uint8_t buf[BUFFER_SIZE];

// APサーバーの設定
//const char* softap_ssid     = "5374gadget";
const char* softap_password = "12345678";
const char* settings = "/wifi_settings.txt";
const char* area_settings = "/area_settings.txt";
ESP8266WebServer server(80); //Webサーバの待ち受けポートを標準的な80番として定義します

// 燃やすごみ, 燃やさないごみ、資源, あきびん
enum GARBAGE {
  notgarbage,
  burnable,
  unburnable,
  recyclable,
  bottle,
} today;

// 動作モード：サーバー版、ローカルファイル版
enum MODE {
  mode_server,
  mode_spifs,
} mode_update;

bool updatedArea = false;
bool retryWifiConnect = false;

// Use web browser to view and copy
// SHA1 fingerprint of the certificate
const char fingerprint[] PROGMEM = "CC AA 48 48 66 46 0E 91 53 2C 9C 7C 23 2A B1 74 4D 29 9D 33"; // raw

// ゴミの日データ(ローカルファイル版)
const char* data_garbage = "/data_garbage.csv";

// ごみカレンダー用バッファ
#define BUFFER_SIZE_GMC 5120
char buf_gmc[BUFFER_SIZE_GMC];

// ごみカレンダー配列 (Table)
// 　次元 0: 月(0～11 = 1～12月)
//        1: 週(0～4  = 第1週～第5週)
//        2: 週(0～6  = 日曜日～土曜日)
char gomi_cal[12][5][7];

// ★★★★★設定項目★★★★★★★★★★
String ssid     = "********";       // 自宅のWiFi設定
String password = "********";
int start_oclock = 6;   // 通知を開始する時刻
int start_minute = 0;
int end_oclock   = 8;   // 通知を終了する時刻
int end_minute   = 30;
// ★★★★★★★★★★★★★★★★★★★

// ★★★★★設定項目（金沢市版）★★★★
// 以下のURLにあるエリア番号を入れる
//https://github.com/PhalanXware/scraped-5374/blob/master/save.json
int area_number = 0;    // 地区の番号（例：浅野 0, 浅野川 1）
// ★★★★★★★★★★★★★★★★★★★

void setup() {

  // シリアル設定
  Serial.begin(9600);
  Serial.println("");
  Serial.println("Start 5374gadget!!!");

  // NeoPixelのLEDの初期化
  pixels.begin();
  pixels.setPixelColor(0, pixels.Color(0, 0, 0));
  pixels.show();

  // 展示用デモ
#if 0
  while (1)
  {
    Blink.softly(&pixels, NUMPIXELS, 255, 0, 0, 2000);
    delay(500);
    Blink.softly(&pixels, NUMPIXELS, 32, 255, 0, 2000);
    delay(500);
    Blink.softly(&pixels, NUMPIXELS, 0, 255, 128, 2000);
    delay(500);
    Blink.softly(&pixels, NUMPIXELS, 255, 0, 255, 2000);
    delay(500);
  }
#endif

  // ファイルの読み出しテスト(WiFi接続でSPIFFSを使うので、先に初期化)
  SPIFFS.begin();

  // MACアドレスの表示
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());

  // AP+STAモードの設定
  WiFi.mode(WIFI_AP_STA);
  IPAddress myIP = WiFi.softAPIP();   // APとしてのIPアドレスを取得。デフォルトは 192.168.4.1
  Serial.print("AP IP address: ");
  Serial.println(myIP);


  byte mac[6];
  WiFi.macAddress(mac);
  String ssid = "5374gadget-";
  for (int i = 4; i < 6; i++) {
    ssid += String(mac[i], HEX);
  }
  Serial.println("SSID: " + ssid);
  WiFi.softAP(ssid.c_str(), softap_password);

  // APとして振る舞うためのSSIDとPW情報
  WiFi.softAP(softap_ssid, softap_password);

  // WiFi接続
  wifiConnect();
  delay(1000);

  // NTP同期
  configTime( JST, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");

  // SPIFS内のファイル表示
  listAllFiles();

  // ごみの日データの有無でサーバーモードとローカルファイルモードを切り替え
  if (SPIFFS.exists(data_garbage)) {
    // ゴミの日情報がSPIFSにある場合
    Serial.println("exist the DataGarbageFile.");
    Serial.println("mode_update = mode_spifs");
    mode_update = mode_spifs;
    csvFileRead_CalArraySet();    // ごみカレンダーファイルの読み出し・ごみカレンダー配列準備
  } else {
    // ゴミの日情報がSPIFSに無い場合
    Serial.println("not exist the DataGarbageFile.");
    Serial.println("mode_update = mode_server");
    mode_update = mode_server;
    today = notgarbage;           // 今日のデータの読み出し
    updateGarbageDay();
  }

  // サーバー機能
  if (mode_update == mode_server) {
    server.on("/", handleRoot);
  } else if (mode_update == mode_spifs) {
    server.on("/", handleRootSpifs);
  }
  server.on("/index.html",    handleRoot);
  server.on("/set-wifi.html", handleSetWifi);
  server.on("/set-area.html", handleSetArea);
  server.on("/set-time.html", handleSetTime);     // 時刻の設定画面
  // 処理部
  server.on("/settingWiFi",   handleSettingWiFi);
  server.on("/settingArea",   handleSettingArea);
  server.on("/settingTime",   handleSettingTime);
  server.onNotFound(handleNotFound);                // エラー処理
  server.begin();
  Serial.println("HTTP server started");

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

  // 時間のチェック(24H表記)
  if (2000 < (tm->tm_year + 1900))  // サーバー同期が正しく行われているかどうかで処理を除外
  {
    if ((start_oclock < tm->tm_hour) && (tm->tm_hour < end_oclock))
    {
      if (mode_update == mode_server) {
        onLed();
      }
      if (mode_update == mode_spifs) {
        ledColorControlWithGarbage(tm->tm_mon, (tm->tm_mday - 1) / 7, tm->tm_wday);
      }
    }
    else if (start_oclock == tm->tm_hour)
    { // 開始時刻の分の判定
      if (start_minute <= tm->tm_min)
      {
        if (mode_update == mode_server) {
          onLed();
        }
        if (mode_update == mode_spifs) {
          ledColorControlWithGarbage(tm->tm_mon, (tm->tm_mday - 1) / 7, tm->tm_wday);
        }
      }
    }
    else if (end_oclock == tm->tm_hour)
    { // 終了時刻の分の判定
      if (end_minute >= tm->tm_min)
      {
        if (mode_update == mode_server) {
          onLed();
        }
        if (mode_update == mode_spifs) {
          ledColorControlWithGarbage(tm->tm_mon, (tm->tm_mday - 1) / 7, tm->tm_wday);
        }
      }
    }
  }

  // サーバーモードの場合
  if (mode_update == mode_server) {

    // 夜中の0時にゴミの日情報をリセット
    if (tm->tm_hour == 0)
    {
      if ((tm->tm_min == 0))
      {
        today = notgarbage;
      }
    }

    // 夜中の３時にデータを更新
    if (tm->tm_hour == 3)
    { // 当日の捨てれるゴミ情報をアップデート
      if ((tm->tm_min == 0) || (tm->tm_min == 20) || (tm->tm_min == 40))
      {
        wifiConnect();
        updateGarbageDay();
        delay(70000); // 余裕を見て、70秒後に変更
      }
    }
  }

  // 地域選択が更新されたらデータを更新
  if (updatedArea)
  { // 地域選択が更新された場合
    updatedArea = false;
    wifiConnect();
    updateGarbageDay();
  }

  // WiFi設定が更新されたら設定値を更新し、WiFi接続を試みる
  if (retryWifiConnect)
  {
    retryWifiConnect = false;
    wifiConnect();
    updateGarbageDay();
  }

  // STAモードで接続出来ていない場合
  if (WiFi.status() != WL_CONNECTED) {
    Blink.softly(&pixels, NUMPIXELS, 32, 32, 32, 500);
  } else {
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();
  }

  // Webサーバの接続要求待ち
  server.handleClient();

  // 時間待ち
  delay(100);
}

void wifiConnect() {

  // WiFiファイルの読出し
  File f = SPIFFS.open(settings, "r");
  if (!f) {
    // 設定ファイルが無い場合
    Serial.println("not exist the WiFiSettingFile.");
  } else {
    // 設定ファイルがある場合
    Serial.println("exist the WiFiSettingFile.");
    ssid = f.readStringUntil('\n');
    password = f.readStringUntil('\n');
    ssid.trim();
    password.trim();
    Serial.println("SSID: " + ssid);
    //Serial.println("PASS: " + password);
  }
  f.close();

  // 地域情報の読み出し
  File f_area = SPIFFS.open(area_settings, "r");
  if (!f_area) {
    // 設定ファイルが無い場合
    Serial.println("not exist the AreaSettingFile.");
  } else {
    // 設定ファイルがある場合
    Serial.println("exist the AreaSettingFile.");
    String stringArea = f_area.readStringUntil('\n');
    stringArea.trim();
    area_number = stringArea.toInt();
    Serial.print("area: ");
    Serial.println(area_number);
  }
  f_area.close();



  //WiFi接続開始
  WiFi.begin(ssid, password);
  Serial.print("Connecting to " + String(ssid));

  //接続を試みる(15秒)
  for (int i = 0; i < 30; i++) {
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

void readHtml(String filename)
{
  // HTMLファイルの読出し
  File htmlFile = SPIFFS.open(filename, "r");
  if (!htmlFile) {
    Serial.println("Failed to open index.html");
  }
  size_t size = htmlFile.size();
  if (size >= BUFFER_SIZE) {
    Serial.print("File Size Error:");
    Serial.println((int)size);
  } else {
    Serial.print("File Size OK:");
    Serial.println((int)size);
  }
  memset(buf, 0, sizeof(buf));
  htmlFile.read(buf, size);
  htmlFile.close();
}

void handleRoot() {
  Serial.println("Accessed handleRoot");
  readHtml("/index.html");
  server.send(200, "text/html", (char *)buf);
}

void handleRootSpifs() {
  Serial.println("Accessed handleRootSpifs");
  readHtml("/index_spifs.html");
  server.send(200, "text/html", (char *)buf);
}

void handleSetWifi() {
  readHtml("/set-wifi.html");
  server.send(200, "text/html", (char *)buf);
}

void handleSetArea() {
  readHtml("/set-area.html");
  server.send(200, "text/html", (char *)buf);
}

void handleSetTime() {
  readHtml("/set-time.html");
  server.send(200, "text/html", (char *)buf);
}

// 時間の設定
void handleSet() {

  // HTMLファイルの読出し
  File htmlFile = SPIFFS.open("/set.html", "r");
  if (!htmlFile) {
    Serial.println("Failed to open index.html");
  }
  size_t size = htmlFile.size();
  if (size >= BUFFER_SIZE) {
    Serial.print("File Size Error:");
    Serial.println((int)size);
  } else {
    Serial.print("File Size OK:");
    Serial.println((int)size);
  }
  htmlFile.read(buf, size);
  htmlFile.close();

  server.send(200, "text/html", (char *)buf);
}

// 設定処理
void handleSettingWiFi()
{
  // SSID/PASSの読み込み
  ssid = server.arg("ssid");
  password = server.arg("pass");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("PASS: ");
  Serial.println(password);

  // ファイルへの保存
  Serial.println("Write SettingsFile.");
  File f = SPIFFS.open(settings, "w");
  f.println(ssid);
  f.println(password);
  f.close();

  // WiFiの再接続フラグを上げる
  retryWifiConnect = true;

  // レスポンス処理（設定値が確認できるようにここはハードコーディング）
  String html = "";
  html += "<h1>WiFi Settings</h1>";
  html += "SSID : " + ssid + "<br>";
  html += "PASS : " + password + "<br>";
  html += "<br>";
  html += "<form method='GET' action='/index.html'>";
  html += "<button style=\"width:40%; padding: 10px\">HOME</button></form><br>";
  server.send(200, "text/html", html);
}

void handleSettingArea()
{
  // 校下・地区の更新
  String stringArea = server.arg("area");
  area_number = stringArea.toInt();
  Serial.print("area: ");
  Serial.println(area_number);

  // ファイルへの保存
  Serial.println("Write SettingsFile.");
  File f = SPIFFS.open(area_settings, "w");
  f.println(area_number);
  f.close();

  // 地域のアップデートフラグを上げる
  updatedArea = true;

  // レスポンス処理
  handleRoot();
}

void handleSettingTime()
{
  // レスポンス処理
  handleRoot();
}

// アクセスのエラー処理
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void onLed(void) {
  // 点灯パターン
  if (today == burnable) {
    // 燃やすごみ（赤）
    Blink.softly(&pixels, NUMPIXELS, 255, 0, 0, 3000);
    delay(500);
  } else if (today == unburnable) {
    // 燃やさないごみ（紫）
    Blink.softly(&pixels, NUMPIXELS, 255, 0, 255, 3000);
    delay(500);
  } else if (today == recyclable) {
    // 資源ごみ（緑）
    Blink.softly(&pixels, NUMPIXELS, 32, 255, 0, 3000);
    delay(500);
  } else if (today == bottle) {
    // あきびん（エメラルドグリーン）
    Blink.softly(&pixels, NUMPIXELS, 0, 255, 128, 3000);
    delay(500);
  }
}

// ゴミの日情報のアップデート
//   mode_server ：サーバーにアクセスしてアップデート
//   mode_spifs  ：SFIFSファイルをベースにアップデート
void updateGarbageDay(void) {

  // サーバーモード
  if (mode_update == mode_server) {

    // ゴミ情報の読み出し
    std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
    client->setFingerprint(fingerprint);
    HTTPClient https;

    // "area"とJSONファイルのNo.のずれはここで吸収する
    String url = "https://raw.githubusercontent.com/PhalanXware/scraped-5374/master/save_" + String(area_number + 1) + ".json";
    Serial.print("connect url :");
    Serial.println(url);

    Serial.print("[HTTPS] begin...\n");
    if (https.begin(*client, url)) {  // HTTPS

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

  // SPIFSモード
  if (mode_update == mode_spifs) {
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

// SPIFS内のファイルを表示
void listAllFiles() {
  Serial.println("------------------------------");
  Serial.println("SPIFS File");
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    Serial.print(dir.fileName());
    File f = dir.openFile("r");
    Serial.println(String(" ") + f.size());
  }
  Serial.println("------------------------------");
}


// csvファイルの読み出し・ごみカレンダー配列準備
boolean csvFileRead_CalArraySet() {
  // csvファイルの読出し
  File fp = SPIFFS.open(data_garbage, "r"); // 読み取り
  if (!fp) {
    Serial.println("Failed to open Gomi-Calender File");
    return (false);
  }
  while (fp.read((uint8_t *)buf_gmc, BUFFER_SIZE_GMC) == BUFFER_SIZE_GMC);
  fp.close();

  Serial.println("File Open complete !");
  //Serial.print(buf_gmc);

  // 読み取りデータをごみカレンダー配列（3次元配列）に格納
  unsigned int i = 0;
  for (unsigned int month = 0 ; month < 12 ; month++) {
    for (unsigned int week = 0 ; week < 5 ; week++) {
      for (unsigned int weekday = 0 ; weekday < 7 ; i++) {
        if ( isDigit(buf_gmc[i]) ) {
          gomi_cal[month][week][weekday] = buf_gmc[i];
          weekday++;
        }
      }
    }
  }

  /*
    //ごみカレンダー配列の内容確認
    Serial.println("");
    Serial.println("Calender Array Conversin Start !");
    //テスト書き出し
    for (unsigned int month = 0 ; month < 12 ; month++) {
      for (unsigned int week = 0 ; week < 5 ; week++) {
        for (unsigned int weekday = 0 ; weekday < 7 ; weekday++) {
          Serial.print(gomi_cal[month][week][weekday]);
          if (weekday < 6 )  Serial.print(",");
        }
        Serial.print("\n");
      }
    }
  */

  return (true);
}

// 回収ごみ種に応じたLED色表示制御
void ledColorControlWithGarbage(unsigned int month, unsigned int week, unsigned int weekday) {

  Serial.printf("ごみIndex: %c\n", gomi_cal[month][week][weekday]);

  switch (gomi_cal[month][week][weekday]) {
    case '0':
      // ごみ収集日でない
      pixels.setPixelColor(0, pixels.Color(0, 0, 0));
      pixels.show();
      break;
    case '1':
      // 『燃やすごみ』収集日
      Blink.softly(&pixels, NUMPIXELS, 255, 0, 0, 3000);
      delay(500);
      break;
    case '2':
      // 『燃やさないごみ』収集日
      Blink.softly(&pixels, NUMPIXELS, 0, 20, 235, 3000);
      delay(500);
      break;
    case '3':
      // 『資源』収集日
      Blink.softly(&pixels, NUMPIXELS, 0, 255, 0, 3000);
      delay(500);
      break;
    case '4':
      // 『びん』収集日（『古紙類』＠河北郡）
      Blink.softly(&pixels, NUMPIXELS, 128, 48, 0, 3000);
      delay(500);
      break;
    case '5':
      // （『粗大ごみ』＠河北郡）
      Blink.softly(&pixels, NUMPIXELS, 128, 132, 0, 3000);
      delay(500);
      break;
    default:
      // 未知の ごみIndex
      pixels.setPixelColor(0, pixels.Color(6, 6, 1));
      pixels.show();
      break;
  }
}
