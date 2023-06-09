# 4DSV
 
4次元ストリートビュー用のビュワーアプリです。OpenSiv3Dで作られました。  
4次元ストリートビュー用ではありますが、１つの全方位画像もしくは動画を見ることもできます。  
実行するためには
OpenSiv3Dをインストールし、Main.cppをコピーペーストしてビルドしてください。  
以下では機能とその使い方について説明します。
 
 
 
## ファイルオープン

**Control/Command + O**　を押すとファイルダイアログが開きます。


画像や動画、または４次元ストリートビュー用の設定ファイル　.4dsv　ファイルを選択してください。
.4dsvファイルの中身は次のようにします。
```bash
1行目　動画群があるディレクトリパス（絶対パスもしくは.4dsvファイルから見た相対パス）
2行目　動画の拡張子
3行目　x,y,zそれぞれの方向のカメラ数
4行目　初めに表示するカメラの位置
5行目　フレームレート
```

例(test.4dsv)
```bash
1行目　../
2行目　mp4
3行目　2 1 1
4行目　0 0 0
5行目　25
```
動画の読み込みには時間がかかります。

## それ以外のキーボード操作
#### カメラ切り替え
←/→　　　　　　　：画面に表示するカメラ位置を移動する (x方向)  
↓/↑　　　　　　　：画面に表示するカメラ位置を移動する (y方向)  
Comma/Period　　：画面に表示するカメラ位置を移動する (z方向)  

#### 動画再生関連
Space　　　　　　：動画の再生・停止  
Enter　　　　　　：1フレーム動画を前に進める  
L               ：ループ再生するかどうか  

#### その他
Escape/Q　　　　 ：アプリを終了させる  
F1　　　　　     ：OpenSiv3Dのライセンス表示  
F12　　　　　    ：スクリーンショット（実行ファイルと同じディレクトリにSrennShotというディレクトリが作られます。）  


## マウス操作
マウスドラッグによってこと視線方向を変えたり、動画の再生位置を変えます。
また、マウスホイールを回転させることで動画や画像を拡大して表示します。


