# 5374gadget
![本体](https://github.com/yukima77/5374gadget/blob/images/main.JPG)

## 説明
このデバイスはデバイス自体が光ることで、今日がゴミの日であることを知らせ、その色によりゴミの４つの分類のいずれかを知らせるデバイスです

## 開発の動機
ゴミの日を忘れてゴミを捨て忘れてしまったり、ゴミの日をメールで知らせてくれるサービスを使用しても朝に慌ただしく出掛ける用意をしているとメールがあったことを忘れてゴミを捨て忘れてしまうということが良くあった為、その対応をして開発を進めています。

# 金沢市版（サーバー版）
## 概要
サーバー側にゴミの日データを用意し、通信を行うことでゴミの日を判断します。サーバー側の準備がされていれば、WEB経由で地域などの変更も可能な為、デバイスへの書き込み環境などが不要になります。

## 初めて使用する場合
1. 電源を入れて下さい
1. 本体が白く点滅するまでしばらくお待ち下さい（約15秒程度）
1. アクセスポイントモードが有効になっているので、以下のWEBブラウザからの設定を参考にSSID/パスワードを設定してください
1. 正しく設定できた場合は、白く点滅しなくなります
1. 以上で設定完了です。

## WEBブラウザからの設定
* SSID：5374gadget
* PASS：12345678
* URL：192.168.4.1

## 備考
* WiFiの設定、地域の設定の順に設定して下さい
* WiFiに接続出来ていない場合は白く点滅します

# かほく市版（ローカルファイル版）
## 概要
事前にゴミの日データを用意して、デバイス内にゴミの日データを格納する方式です。サーバー通信がない為、サーバー側の準備が必要ありません。一方、事前にゴミの日データをデバイス内に書き込む為、専用フォーマットのゴミの日データの準備及びデバイスへの書き込み環境が必要になります。

## 設定方法
1. data_kahokuフォルダからお住いの地区のゴミの日データ（data_garbage_xx.csv）をコピーして、dataフォルダに貼り付ける
1. ファイルの名前を「data_garbage.csv」に変更する
1. 以下に記載のHTMLファイルのアップデート方法を参考に、ゴミの日データもデバイス内に書き込む
1. あとは通常版と同じ方法でSSID/PASSの設定を行い、使用する（日時の同期が必要な為、WiFiとの接続は必須です）

＊ゴミの日データをデバイス内に保有する為、年度が切り替わるタイミングでゴミの日データの更新も必要になります

# 設置について
以下のUSB-ACアダプタを使用すると壁のコンセントに綺麗に設置することが可能です。
その他のACアダプタの場合は、USBの向きが合わないことがあります。
https://www.amazon.co.jp/dp/B00104S0QQ/

# 上級者向け

## ファームウェアのアップデート方法
#### ESP8266のファームウェアの書き込み方法
* T.B.D
#### HTMLファイルのアップデート方法
* esp-wroom-02(esp8266)のSPIFFS機能でフラッシュメモリにHMLファイルを追加する
* SPIFFSの容量は、32KBでOK
* Flash Size: "2M(128K SPIFFS)

http://www.shangtian.tokyo/entry/2018/02/17/151330
