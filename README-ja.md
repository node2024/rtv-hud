# RTV HUD 0.9.0 配布パッケージ

[English](README.md) | 日本語

Linux x86_64 CS2 用。SourceHook から KHook に5つのフックを移行した版です。
2026-09-12 にサーバーへの導入・読み込みを確認し、利用者によるゲーム内の動作確認も完了しました。
同梱バイナリは確認済みの稼働バイナリと SHA-256 が一致します。今回の梱包では再ビルドしていません。

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
source/rtv-hud/bridge は旧連携用ソースで、0.9.0 の導入には使用しません。

## 導入・0.8.0からの更新

1. CS2 サーバーを停止し、既存のバイナリと設定をバックアップします。
2. Metamod と併用プラグインの KHook 対応を揃えます。
3. game/csgo/ のファイルを配置します。既存の admins.txt、maplist.txt、cfg は
   必要な差分だけ反映してください。空の admins.txt で既存管理者を上書きしないでください。
4. MultiAddonManager の mm_extra_addons に 3797394226 を追加します。
   既存のアドオンIDは保持してください。同梱設定は HUD 用IDのみを指定しています。
5. 起動してサーバーコンソールで meta version、meta list、rtvhud_status を実行します。
   RTV HUD 0.9.0、ready=1、browser_assets=1 を確認します（同梱一覧は maps=364）。
6. ゲーム内でマップ一覧・投票を確認します。

0.8.0 と同じ Workshop 素材を使用するため、既に導入済みならアセットの再公開は不要です。
設定は rtvhud_required_percent=100、待機30秒、クールダウン30秒、投票30秒、
結果表示5秒、一覧タイムアウト120秒。勝利マップへの変更と終了前投票を有効にしています。
設定の詳細は cfg/rtv_hud.cfg を参照してください。
マップ一覧変更後は rtvhud_reload、管理者変更後は rtvhud_reload_admins、
設定変更後は exec rtv_hud.cfg を実行します。

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
Workshop 配下の Windows 手順は HUD 素材のコンパイル用であり、Windows サーバー用
プラグインバイナリは本パッケージに含みません。
ソース内の0.8.0に関する記録はブラウザー素材の履歴です。導入要件はこのREADMEを参照してください。
