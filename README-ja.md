# RTV HUD 0.9.2 配布パッケージ

[English](README.md) | 日本語

Linux x86_64 CS2 用。SourceHook から KHook に5つのフックを移行した版です。
0.9.0 は 2026-09-12 にサーバーへの導入・読み込みとゲーム内の動作を確認しました。
0.9.2 は一覧を開く際の `NETWORK_DISCONNECT_OVERFLOW` 報告を受けた送信量対策版です。
一覧を最大24行＋前後移動2行のページ表示にし、プレイヤーごとに HUD を分離して本人だけに送信します。
閉じた HUD は破棄し、使用済みの一覧データを残しません。0.9.1 の管理者制限も維持しています。
Linux バイナリを再ビルドし、8件のテストが成功しています。
実サーバーでの切断再現・解消確認は未実施で、最新 CS2 との互換性を保証するものではありません。

## 開発方針

このプロジェクトは、主に **Vibe Coding** で開発しています。
自然言語での指示を通じてAIの支援を受け、対話的にコードの作成・改善を繰り返す開発手法を採用しています。

## 必要な環境

- KHook 対応の Metamod:Source（plugin API 18）。確認版: 2.0.0-dev+1467。
- KHook 対応の MultiAddonManager。確認版: 1.6。
- HUD の Workshop アドオン 3797394226。

旧 SourceHook / API 17 の Metamod では読み込めません。
Metamod 更新時には、併用プラグインも KHook 対応版に揃えてください。
今回の確認環境では CS2KZ 0.0.172、SQLMM 1.3.4.3 を使用し、
CounterStrikeSharp と ClientCvarValue を無効化しました。
RTV HUD 自体は CS2KZ・SQLMM・CounterStrikeSharp・Swiftly に依存しません。

## 内容

- game/csgo/: 配置用 Linux バイナリ、VDF、設定、364件のマップ一覧
- source/rtv-hud/: 対応ソース、テスト、ビルドスクリプト、Workshop 素材
- LICENSE / NOTICE.md / THIRD_PARTY/: ライセンスと由来
- release-manifest.json / SHA256SUMS.txt: 版情報とファイル照合用情報

admins.txt は配布用に空にしています。ソース内 deploy も同じ設定です。
サーバーの認証情報、管理者ID、DB、ログ、他プラグインのバイナリは含みません。
Workshop VPK とマップ本体は別途配信されます。
source/rtv-hud/bridge は旧連携用ソースで、0.9.2 の導入には使用しません。

## 導入

1. CS2 サーバーを停止し、既存のバイナリと設定をバックアップします。
2. Metamod と併用プラグインの KHook 対応を揃えます。
3. game/csgo/ のファイルを配置します。既存の admins.txt、maplist.txt、cfg は
   必要な差分だけ反映してください。空の admins.txt で既存管理者を上書きしないでください。
4. MultiAddonManager の mm_extra_addons に 3797394226 を追加します。
   既存のアドオンIDは保持してください。同梱設定は HUD 用IDのみを指定しています。
5. 起動してサーバーコンソールで meta version、meta list、rtvhud_status を実行します。
   RTV HUD 0.9.2、ready=1、browser_assets=1 を確認します（同梱一覧は maps=364）。
6. ゲーム内でマップ一覧・投票を確認します。

設定は rtvhud_required_percent=100、待機30秒、クールダウン30秒、投票30秒、
結果表示5秒、一覧タイムアウト120秒。勝利マップへの変更と終了前投票を有効にしています。
設定の詳細は cfg/rtv_hud.cfg を参照してください。
マップ一覧変更後は rtvhud_reload、管理者変更後は rtvhud_reload_admins、
設定変更後は exec rtv_hud.cfg を実行します。

`!map`・`!mapmenu`・`!mm`（`/` 形式も同様）と `rtvhud_map` は管理者専用です。
管理者は `game/csgo/addons/rtv_hud/admins.txt` に SteamID64 を1行に1件ずつ登録し、
サーバーコンソールで `rtvhud_reload_admins` を実行してください。空の場合は全員拒否します。
他プラグインの管理者権限とは独立しています。`!nominate`・`!nom` は誰でも利用できます。
一般プレイヤーには `CHANGE MAP` タブを非表示にし、確定ボタンは推薦用の `Nominate` と表示します。
画面を閉じる際は HUD 自体を破棄し、管理者一覧の再読み込み時には開いている画面を閉じます。
0.9.0 / 0.9.1 からはサーバーを停止して `game/csgo/addons/rtv_hud/bin/linuxsteamrt64/rtv_hud.so` を
差し替え、再起動してください。既存の管理者・マップ・設定ファイルはそのまま利用できます。
一覧内の `Next page >>` / `<< Previous page` で移動します。マップを途中で切り捨てず、
全件をページ経由で選べます。既存の Workshop 素材を使用するため再公開は不要です。

切断が続く場合は、切断直前のサーバーログ、CS2 の `version`、`meta version`、`meta list`、
`rtvhud_status` の出力を確認してください。0.9.2 では送信先フィルタに投票 HUD と同じ
既存のエンジン構造を使用しており、CS2 更新による構造変更の有無は実環境での確認が必要です。

## ソースからのビルド

Linux の C++17 コンパイラ、CMake、Make、Python 3、Git が必要です。
source/rtv-hud/ で bash bootstrap.sh を実行すると固定版の SDK / Metamod を取得します。
CS2 と同じホストでビルドするときは必ず CS2 を停止してから実行してください。
cs2server.service を使う環境では bash build-server.sh が停止・確認・単一ジョブでの
ビルド・テストを行います。systemd 操作権限が必要で、完了後もサーバーは停止状態です。
他の環境ではサービス固有の方法で停止を確認後、以下を実行します。

```sh
cmake -S . -B build-khook -DCMAKE_BUILD_TYPE=Release \
  -DSDK="$PWD/deps/reference/hl2sdk-cs2" \
  -DMMS="$PWD/deps/metamod-source"
cmake --build build-khook -j1
ctest --test-dir build-khook --output-on-failure
```

成果物は build-khook/rtv_hud.so です。既存8テストはすべて成功しています。

## Windows 環境での利用

### サーバーへの導入

**Windows ネイティブの CS2 サーバーには現在対応していません。**
同梱の `rtv_hud.so` は Linux 用で、Windows サーバーでは読み込めません。
ソースも Linux 固有のライブラリ・エンジンシグネチャ・ビルド設定を使用しているため、
Windows 用 DLL の作成にはコンパイラの変更だけでなく移植が必要です。

x86_64 の Windows PC で試す方法として、WSL2 内で Linux サーバーを動かす構成があります。
**RTV HUD の WSL2 上での動作は未検証です。以下は検証用の導入案であり、動作確認済みの手順ではありません。**

1. 管理者として開いた PowerShell で Ubuntu をインストールします。

   ```powershell
   wsl --install -d Ubuntu
   ```

   指示に従って再起動し、Ubuntu を開いて Linux ユーザーを作成します。
   PowerShell で `wsl --list --verbose` を実行し、Ubuntu がバージョン2であることを確認します。
   詳細は Microsoft の [WSL 導入ガイド](https://learn.microsoft.com/en-us/windows/wsl/install)を参照してください。
2. Ubuntu 内で SteamCMD と **Linux 版 CS2 専用サーバー**を導入します。
   [CS2 専用サーバーの導入ガイド](https://developer.valvesoftware.com/wiki/Counter-Strike_2/Dedicated_Servers)を参照し、
   サーバーは `~/cs2-server` など Linux 側のファイルシステムに配置してください。
   Windows 版 CS2 のインストール先では、この Linux プラグインを読み込めません。
3. 上記の要件に合う Linux 版 Metamod と MultiAddonManager をそのサーバーに導入します。
   サーバーを停止してから[導入](#導入)の手順に従い、Linux サーバー側の `game/csgo/` に配置します。
   Windows のエクスプローラーからは `\\wsl.localhost\Ubuntu\home\<Linuxユーザー名>\` で
   Ubuntu 側のファイルにアクセスできます。
4. Ubuntu 内で Linux 版 CS2 サーバーを起動し、サーバーコンソールで
   `meta version`、`meta list`、`rtvhud_status` を確認します。
   Windows の CS2 クライアントから接続して HUD と投票を確認してください。
   プレイヤー側にサーバープラグインを導入する必要はありません。
5. 別のPCから接続する場合は、使用するサーバーポートの UDP 通信を含め、
   WSL のネットワークと Windows / Hyper-V のファイアウォール、ルーターの設定を行います。
   Microsoft の [WSL ネットワークガイド](https://learn.microsoft.com/en-us/windows/wsl/networking)を参照してください。
   接続可否はプラグインの読み込みとは別に確認します。

Ubuntu 内でソースからビルドする場合は、`build-essential`、`cmake`、`make`、`python3`、`git` を
インストールし、Ubuntu のシェルで[ソースからのビルド](#ソースからのビルド)に従ってください。
成果物は Linux 用の `.so` です。`build-server.sh` は、前提となる `cs2server.service` を
構成している場合にのみ使用してください。

### Windows での HUD 素材のビルド

`source/rtv-hud/workshop/` のスクリプトは Workshop 用 HUD 素材のコンパイル用です。
サーバープラグインのビルド・導入は行いません。通常のサーバー導入ではアドオン
`3797394226` を使用するため、素材の再ビルドや公開は不要です。

1. CS2 Workshop Tools をインストールし、`rtv_hud` というアドオンを作成するか、既存のものを開きます。
2. パッケージ全体を展開し、`source/rtv-hud/workshop/Build-Live.cmd` を実行します。
3. 入力を求められたら、`game` と `content` を含む CS2 のインストール先を指定します。
   例: `D:\SteamLibrary\steamapps\common\Counter-Strike Global Offensive`
4. `BUILD OK` と `workshop/build-output/` 内の成果物を確認します。
   ローカルプレビューには `Build-LocalPreview.cmd` を使い、
   [プレビュー手順](source/rtv-hud/workshop/LOCAL-PREVIEW.md)に従ってください。

本パッケージでは Windows 上の素材ビルド・プレビューの検証記録はありません。
コンパイルによって Workshop へ自動公開されることはありません。
