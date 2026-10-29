# 公開を待たずにローカルで表示する

同じCSS・同じ画面構造にサンプル文字列を入れた`preview.xml`を使います。
票数と残り時間は固定表示です。Metamod・Workshop購読・本番サーバーは使いません。

1. ZIPをすべて展開し、**Build-LocalPreview.cmd**を実行します。
   通常版のBuild-Windows.cmdではありません。
2. インストール先に次を入力し、BUILD OKを確認します。

```text
D:\SteamLibrary\steamapps\common\Counter-Strike Global Offensive
```

既存のrtv_hudアドオンを使います。新たな出力は
`game\csgo_addons\rtv_hud\panorama\layout\custom_game\rtv_hud\preview.vxml_c`です。
vote.xmlは変更せず、CSSは通常版と共通です。

3. SteamからCS2 Workshop Toolsを起動し、**rtv_hudを選択してToolsを起動**します。
   別アドオンでToolsを起動済みなら閉じて選び直してください。
4. Toolsと一緒に起動するCS2の開発者コンソール、または接続済みVConsoleで実行します。

```text
map de_dust2
```

5. 読み込み後、チームに入ってスポーンし、次を1行ずつ実行します。

```text
sv_cheats 1
ent_create custom_hud_layout { "targetname" "rtvhud_local_preview" "layout" "panorama/layout/custom_game/rtv_hud/preview.xml" }
```

コンソールを閉じると左側にROCK THE VOTEと6候補が出る想定です。
Hammerでマップを作成する作業は省略し、標準マップ上でエンティティを生成します。
このWindows実機での表示は未検証です。出ない場合はコマンド直後のログを送ってください。

再実行すると重複するため、消す場合・再表示の前はプレビューだけを削除します。

```text
ent_fire rtvhud_local_preview Kill
```

マップの再ロードでも消えます。
rtvhud_preview/rtvhud_voteはLinux版プラグイン専用なので、ここでは使いません。

## 出ない場合

- File not found / Failed loading resource: ビルド成功と、rtv_hudを選択したTools起動を確認。
- Unknown entity等: CS2とWorkshop Toolsの更新を確認。
- チートコマンドが拒否される: 本番サーバーではなくローカルのmap de_dust2で確認。
- エンティティの有無は`ent_find rtvhud_local_preview`で確認できます。

画面が出たらスクリーンショットを送ってください。文字サイズ・位置などを調整できます。
承認待ちのWorkshop投稿を更新する必要はありません。
後日Workshopを更新するとpreviewも梱包対象になり得ますが、サーバープラグインはvote.xmlのみを参照します。

参考:
- layoutキー: https://github.com/KZGlobalTeam/cs2kz-metamod/blob/v0.0.167/src/kz/hud/layout/entity.cpp
- CS2のent_createの使用例: https://github.com/girlglock/CS2-SourceTS-KZ-Script/blob/main/kz.cfg
- CS2本体のgame/csgo/gameinfo.giではRestrictFlatFileAddonsToToolsが有効です。
  通常起動でなくToolsでローカルアドオンを読み込みます。
