#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <FS.h>
// https://github.com/pokiiio/EmotionalBlink
#include <EmotionalBlink.h>

#define JST     3600* 9

#define PIN         4
#define NUMPIXELS   1
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// HTML用バッファ
#define BUFFER_SIZE 10240
uint8_t buf[BUFFER_SIZE];

// APサーバーの設定
const char* softap_ssid     = "5374gadget";
const char* softap_password = "12345678";
ESP8266WebServer server(80); //Webサーバの待ち受けポートを標準的な80番として定義します

// 燃やすごみ, 資源ごみ, あきびん, 燃やさないごみ
bool today[4] = {false, false, false, false};

// Use web browser to view and copy
// SHA1 fingerprint of the certificate
const char fingerprint[] PROGMEM = "CC AA 48 48 66 46 0E 91 53 2C 9C 7C 23 2A B1 74 4D 29 9D 33"; // raw

// ★★★★★設定項目★★★★★★★★★★
const char* ssid     = "********";       // 自宅のWiFi設定
const char* password = "********";
int start_oclock = 7;   // 通知を開始する時刻
int start_minute = 0;
int end_oclock   = 8;   // 通知を終了する時刻
int end_minute   = 0;
// 以下のURLにあるエリア番号を入れる
//https://github.com/PhalanXware/scraped-5374/blob/master/save.json
int area_number = 3;    // 地区の番号（例：浅野 0, 浅野川 1, 木曳野 14）
// ★★★★★★★★★★★★★★★★★★★

void setup() {

  // シリアル設定
  Serial.begin(9600);
  Serial.println("");

  // NeoPixelのLEDの初期化
  pixels.begin();
  pixels.setPixelColor(0, pixels.Color(0, 0, 255));
  pixels.show();

  // AP+STAモードの設定
  WiFi.mode(WIFI_AP_STA);
  IPAddress myIP = WiFi.softAPIP();   // APとしてのIPアドレスを取得。デフォルトは 192.168.4.1
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // APとして振る舞うためのSSIDとPW情報
  WiFi.softAP(softap_ssid, softap_password);

  // WiFi接続
  wifiConnect();
  delay(1000);

  // NTP同期
  configTime( JST, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");

  // サーバー機能
  server.on("/", handleRoot);
  server.on("/index.html", handleRoot);
  // 処理部
  server.onNotFound(handleNotFound);        // エラー処理
  server.begin();
  Serial.println("HTTP server started");

  // ファイルの読み出しテスト
  SPIFFS.begin();

  // ゴミ情報の読み出し
  // 夜中の１時に更新する
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
      Serial.println(https.getSize());

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = https.getString();
        Serial.println("HTTP_CODE_OK");
        //Serial.println(payload);
      }

      String html[10] = {"\0"};
      int index = split(payload, '\n', html);
      String garbageDays[4] = {"\0"};
      garbageDays[0] = html[5];
      garbageDays[1] = html[6];
      garbageDays[2] = html[7];
      garbageDays[3] = html[8];
      Serial.println(garbageDays[0].indexOf("今日"));
      Serial.println(garbageDays[1].indexOf("今日"));
      Serial.println(garbageDays[2].indexOf("今日"));
      Serial.println(garbageDays[3].indexOf("今日"));

      for (int i = 0; i < 4; i++)
      {
        if (garbageDays[i].indexOf("今日") > 0) {
          today[i] = true;
        } else {
          today[i] = false;
        }
      }

      Serial.println(today[0]);
      Serial.println(today[1]);
      Serial.println(today[2]);
      Serial.println(today[3]);

    } else {
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }
    https.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
}

void loop() {

  time_t t;
  struct tm *tm;
  static const char *wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};

  t = time(NULL);
  tm = localtime(&t);

  Serial.printf(" %04d/%02d/%02d(%s) %02d:%02d:%02d\n",
                tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                wd[tm->tm_wday],
                tm->tm_hour, tm->tm_min, tm->tm_sec);

  // 点灯パターン
  if (today[0] == true){
    // 燃やすごみ（赤）
    Blink.softly(&pixels, NUMLED, 255, 0, 0, 2000);
  } else if(today[1] == true){
    // 資源ごみ（緑）
    Blink.softly(&pixels, NUMLED, 0, 255, 0, 2000);
  }　else if(today[2] == true){
    // あきびん（エメラルドグリーン）
    Blink.softly(&pixels, NUMLED, 0, 0, 255, 2000);
  }　else if(today[3] == true){
    // 燃やさないごみ（紫）
    Blink.softly(&pixels, NUMLED, 255, 0, 255, 2000);
  }
  
  //  pixels.setPixelColor(0, pixels.Color(255,59,18));
  //  pixels.show();

  //  pixels.setPixelColor(0, pixels.Color(115,59,151));
  //  pixels.show();

  //  pixels.setPixelColor(0, pixels.Color(120,173,30));
  //  pixels.show();

  //  pixels.setPixelColor(0, pixels.Color(64,255,128));
  //  pixels.show();


  // 夜中の１時にデータを更新





  // Webサーバの接続要求待ち
  server.handleClient();

  //  pixels.setPixelColor(0, pixels.Color(0,0,0));
  //  pixels.show();
  delay(100);

}

void wifiConnect() {
  Serial.print("Connecting to " + String(ssid));

  //WiFi接続開始
  WiFi.begin(ssid, password);

  //接続状態になるまで待つ
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  //接続に成功。IPアドレスを表示
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
}

String getPageSource1(char host[]) {
  HTTPClient http;

  Serial.println(host);
  http.begin(host);
  int httpCode = http.GET();

  String result = "";

  if (httpCode < 0) {
    result = http.errorToString(httpCode);
  } else if (http.getSize() < 0) {
    result =  "size is invalid";
  } else {
    Serial.println("getString");
    result = http.getString();
  }

  http.end();
  return result;
}

String getPageSource2(char host[]) {
  WiFiClientSecure client;

  if ( !client.connect(host, 443) ) {
    return String("");
  }

  client.print(String("GET ") + "/" +
               " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");
  client.println();

  delay(1000);

  String body = "";

  while (client.available()) {
    body += client.readStringUntil('\r');
  }

  return body;
}


void handleRoot() {
  // HTTPステータスコード(200) リクエストの成功
  Serial.println("Accessed handleRoot");

  // HTMLファイルの読出し
  File htmlFile = SPIFFS.open("/index.html", "r");
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
void handleSetting() {
  String temp_start = server.arg("start");
  String temp_end = server.arg("end");
  Serial.print("time_start:  ");
  Serial.println(temp_start);
  Serial.print("time_end:  ");
  Serial.println(temp_end);

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
