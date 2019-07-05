# 5374gadget

## 初期設定の項目（ファームウェアに書き込む場合）
* String ssid     = "********";       // 自宅のWiFi設定
* String password = "********";
* int start_oclock = 6;   // 通知を開始する時刻
* int start_minute = 0;
* int end_oclock   = 8;   // 通知を終了する時刻
* int end_minute   = 0;

## ファームウェアのアップデート方法
#### ESP8266のファームウェアの書き込み方法
* T.B.D
#### HTMLファイルのアップデート方法
esp-wroom-02(esp8266)のSPIFFS機能でフラッシュメモリにHTMLファイルを追加する
http://www.shangtian.tokyo/entry/2018/02/17/151330

## WEBブラウザからの設定
* SSID：5374gadget
* PASS：12345678
* URL：192.168.4.1

## その他
* WiFiの設定、地域の設定の順に設定して下さい
* WiFiに接続出来ていない場合は白く点滅します

