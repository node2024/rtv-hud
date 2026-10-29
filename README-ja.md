# RTV HUD 0.9.4 配布パッケージ

[English](README.md) | 日本語

Linux x86_64 CS2 用。SourceHook から KHook に5つのフックを移行した版です。
0.9.0 は 2026-09-12 にサーバーへの導入・読み込みとゲーム内の動作を確認しました。
0.9.4 では、`admins.txt` 登録者がチャットから `!rtvhud_reload` でマップ一覧を再読み込みできます。
`!rtv` の投票はマップ最大5件＋「現在のマップを1時間延長」の6択です。推薦枠も5件に揃えています。
継続が勝った場合、残り時間に60分を追加し、同じマップで続行します。既に制限時間を過ぎた場合は
その時点から60分、無制限マップでは無制限のまま継続します。RTV中に行われた次マップ予約も解除します。
マップ終了前の自動投票は従来どおり最大6マップで、継続の選択肢は手動RTVに追加しています。
0.9.3 は 2026-10-23 の S2SDK 修正を取り込み、CS2 更新後のエンジン仮想関数のずれに対応しました。
旧SDKでは SteamID 取得が AppID（730 / 0x2DA）を返す位置を呼んでいました。
サーバーサービス、スキーマ、エンティティシステムと送信先の構造体参照も現行SDKへ移行しています。
0.9.2 の `NETWORK_DISCONNECT_OVERFLOW` 対策は維持しています。
一覧を最大24行＋前後移動2行のページ表示にし、プレイヤーごとに HUD を分離して本人だけに送信します。
閉じた HUD は破棄し、使用済みの一覧データを残しません。0.9.1 の管理者制限も維持しています。
Linux バイナリを再ビルドし、仮想関数の呼び出し位置を含む9件のテストが成功しています。
実サーバーでの起動・ゲーム内動作の確認は未実施です。
ページャー撤去は全件一括送信による切断再発の懸念があるため見送っています。

## 開発方針

このプロジェクトは、主に **Vibe Coding** で開発しています。
自然言語での指示を通じてAIの支援を受け、対話的にコードの作成・改善を繰り返す開発手法を採用しています。

## 必要な環境

- 最新CS2更新に対応した KHook 版 Metamod:Source（plugin API 18）。
  本版のビルドには commit `170058a2f0dbd58106b7c76bfe88a94672295356` を使用しています。
- KHook 対応の MultiAddonManager。確認版: 1.6。
- HUD の Workshop アドオン 3797394226。

旧 SourceHook / API 17 の Metamod では読み込めません。
Metamod 更新時には、併用プラグインも KHook 対応版に揃えてください。
0.9.0 の過去の確認環境では Metamod 2.0.0-dev+1467、CS2KZ 0.0.172、SQLMM 1.3.4.3 を使用し、
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
source/rtv-hud/bridge は旧連携用ソースで、0.9.4 の導入には使用しません。
ソースリポジトリでは配置用ファイルは source/rtv-hud/deploy/ にあり、配布ZIPでは game/csgo/ としても収録します。

## 導入

1. CS2 サーバーを停止し、既存のバイナリと設定をバックアップします。
2. Metamod と併用プラグインを、最新CS2更新と KHook に対応した版に揃えます。
3. game/csgo/ のファイルを配置します。既存の admins.txt、maplist.txt、cfg は
   必要な差分だけ反映してください。空の admins.txt で既存管理者を上書きしないでください。
4. MultiAddonManager の mm_extra_addons に 3797394226 を追加します。
   既存のアドオンIDは保持してください。同梱設定は HUD 用IDのみを指定しています。
5. 起動してサーバーコンソールで meta version、meta list、rtvhud_status を実行します。
   RTV HUD 0.9.4、ready=1、browser_assets=1 を確認します（同梱一覧は maps=364）。
6. ゲーム内でマップ一覧・投票を確認します。

設定は rtvhud_required_percent=100、待機30秒、クールダウン30秒、投票30秒、
結果表示5秒、一覧タイムアウト120秒。勝利マップへの変更と終了前投票を有効にしています。
設定の詳細は cfg/rtv_hud.cfg を参照してください。
マップ一覧変更後は rtvhud_reload、管理者変更後は rtvhud_reload_admins、
設定変更後は exec rtv_hud.cfg を実行します。
`admins.txt` 登録者はチャットの `!rtvhud_reload` / `/rtvhud_reload`、またはクライアントコンソールの
`rtvhud_reload` でもマップ一覧を再読み込みできます。結果・拒否理由を本人のチャットに表示します。
投票中・マップ変更待ち・次マップ予約済みの場合は拒否します。ファイル不正時は元の一覧を維持します。
このコマンドはマップ一覧の再読み込み用です。管理者一覧やcfgには上記の個別コマンドを使ってください。

`!map`・`!mapmenu`・`!mm`（`/` 形式も同様）と `rtvhud_map` は管理者専用です。
管理者は `game/csgo/addons/rtv_hud/admins.txt` に SteamID64 を1行に1件ずつ登録し、
サーバーコンソールで `rtvhud_reload_admins` を実行してください。空の場合は全員拒否します。
他プラグインの管理者権限とは独立しています。`!nominate`・`!nom` は誰でも利用できます。
一般プレイヤーには `CHANGE MAP` タブを非表示にし、確定ボタンは推薦用の `Nominate` と表示します。
画面を閉じる際は HUD 自体を破棄し、管理者一覧の再読み込み時には開いている画面を閉じます。
0.9.0〜0.9.3 からはサーバーを停止して `game/csgo/addons/rtv_hud/bin/linuxsteamrt64/rtv_hud.so` を
差し替え、再起動してください。既存の管理者・マップ・設定ファイルはそのまま利用できます。
一覧内の `Next page >>` / `<< Previous page` で移動します。マップを途中で切り捨てず、
全件をページ経由で選べます。既存の Workshop 素材を使用するため再公開は不要です。

切断が続く場合は、切断直前のサーバーログ、CS2 の `version`、`meta version`、`meta list`、
`rtvhud_status` の出力を確認してください。起動失敗時にはクラッシュログも必要です。

## ソースからのビルド

Linux の C++20 コンパイラ、CMake、Make、Python 3、Git が必要です。
source/rtv-hud/ で bash bootstrap.sh を実行すると固定版の SDK / Metamod を取得します。
CS2 と同じホストでビルドするときは必ず CS2 を停止してから実行してください。
cs2server.service を使う環境では bash build-server.sh が停止・確認・単一ジョブでの
ビルド・テストを行います。systemd 操作権限が必要で、完了後もサーバーは停止状態です。
他の環境ではサービス固有の方法で停止を確認後、以下を実行します。

```sh
cmake -S . -B build-khook -DCMAKE_BUILD_TYPE=Release \
  -DSDK="$PWD/deps/s2sdk" \
  -DMMS="$PWD/deps/metamod-source"
cmake --build build-khook -j1
ctest --test-dir build-khook --output-on-failure
```

成果物は build-khook/rtv_hud.so です。SDKありのビルドでは既存8テストとSDK ABIテストを実行します。
古い HL2SDK や 2026-10-23 の修正を含まない S2SDK は設定時に拒否します。

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
