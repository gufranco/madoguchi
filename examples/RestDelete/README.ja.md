# RestDelete

4件の固定テーブルから項目を削除するRESTエンドポイントで、Ethernetシールドを載せたArduino Uno R4 Minimaが応答します。`DELETE /api/items/{id}`は1回目に`204 No Content`を返し、それ以降は`404`を返します。

[English](README.md) | 日本語 | [简体中文](README.zh-Hans.md) | [繁體中文（香港）](README.zh-HK.md)

## このサンプルの内容

| 領域 | 作ったもの |
|:-----|:-----------|
| レイヤー | 境界づけられたコンテキスト`items`を1つ持ち、`domain`、`application`、`adapters`に分けています。ドメインはArduinoのヘッダーを一切インクルードしません。 |
| ポート | `ItemRepositoryPort`は`insert`、`find`、`erase`を名指しします。 |
| ユースケース | `ItemCatalog::remove`は`kRemoved`または`kMissing`を返します。空いたidは、次に`add`が割り当てるidになります。 |
| アダプター | `mado::SlotTable`上の`RamItemRepository`。`ItemsController`は結果を`204`または`404`に対応づけます。 |
| コンポジションルート | [`RestDelete.ino`](RestDelete.ino)が「lamp」と「fan」を投入し、カードをマウントし、両方をルーターに渡します。 |
| 契約 | [`openapi.json`](openapi.json)から[`api.generated.h`](src/http/api.generated.h)のルートとパスのid解析を生成します。 |
| テスト | 4ファイルのホストテスト23件と、カバレッジ100パーセントのページテスト2件。 |
| ルール | JPL D-60411とPower of Ten。ヒープなし、仮想関数なし、関数ポインターなし、関数あたり2.06個のアサーション。 |

## アーキテクチャ

```mermaid
graph TB
    Client[Browser or curl] --> Server[mado::Server]
    subgraph Http["src/http"]
        Router[api::Router]
        Generated[api.generated.h: route_path_id]
    end
    subgraph Adapters["items/adapters"]
        Controller[ItemsController: remove]
        Ram[RamItemRepository]
    end
    subgraph Application["items/application"]
        Catalog[ItemCatalog: add, find, remove]
        Port[ItemRepositoryPort]
    end
    subgraph Domain["items/domain"]
        Entity[Item, is_item_id]
    end
    Server --> Router --> Controller
    Router --> Card[(SD card pages)]
    Controller --> Generated
    Controller --> Catalog --> Port
    Ram -. satisfies .-> Port
    Catalog --> Entity
```

コントローラーはJSONを書かないので、ワイヤー構造体への対応づけも持ちません。

## プロジェクト構成

```text
RestDelete/
  RestDelete.ino                           コンポジションルート
  openapi.json                             API契約
  src/http/api.generated.h                 生成
  src/http/api.router.h                    削除のルート、それ以外のパスはカードのページ
  src/items/domain/item.entity.h           Item、ItemDraft、idの範囲と名前のルール
  src/items/application/item_repository.port.h
  src/items/application/item_catalog.service.h   add、find、remove
  src/items/adapters/ram_item.repository.h
  src/items/adapters/items.controller.h
  tests/                                   ホストテスト
  SD Card/                                 ページ
```

## APIエンドポイント

| メソッド | エンドポイント | 説明 |
|:---------|:---------------|:-----|
| DELETE | `/api/items/{id}` | 項目1から4を削除し、ボディなしの`204`を返す |
| GET | `/api/openapi.json` | カードから読んだOpenAPIドキュメント |
| GET | その他のパス | カード上のページ |

| エラーの条件 | ステータス | 詳細 |
|:-------------|:----------:|:-----|
| idが空き、0、4より大きい、先頭にゼロがある、または数値でない | 404 | `no item has this id` |
| `/api/items`へのDELETE | 405 | `Allow`がそのパスで使えるメソッドを並べる |

## 設計上の判断

### 2回目の削除はエラーではなく404

DELETEの効果は冪等です。2回目の呼び出しはテーブルを1回目と同じ状態のまま残します。それでも`404`を返すのは、項目がすでになく、クライアントが特定のidを求めたからです。テスト`AnswersNotFoundTheSecondTime`がこの線を守ります。

### ポートが新しく名指しするのはeraseだけ

このサンプルのユースケースは、初期データのために`insert`を、挿入の後に`find`を、そして`erase`を呼びます。ポートはこの3つを並べるので、RestDelete向けのリポジトリは`store`を備える必要がありません。

### idは意図して2回確かめる

コントローラーの`requested_id`は不正なパスを拒否し、`ItemCatalog::remove`は範囲をもう一度確かめます。2回目の確認は比較1回分のコストで、テストのようなHTTPアダプター以外の呼び出し元に対してもユースケースを安全に保ちます。

## トレードオフ

| できること | 今はしない理由 | このままのリスク |
|:-----------|:---------------|:-----------------|
| 復元ルート付きの論理削除 | 動詞のサンプルの範囲外 | 削除した項目は次の起動まで戻らない |
| テーブルをカードに保存する | 永続化はRestApiが示す | 削除した初期データは起動のたびに戻る |
| DELETEの認証 | ベアラートークンはRestApiが示す | ネットワーク上の誰でも削除できる |

## 前提

- ネットワークは信頼できる。認証もTLSもない。
- 同時に扱うクライアントは1つ。

## テスト

```bash
cmake -S ../../tests -B build && cmake --build build --target mado_example_restdelete
ctest --test-dir build -R '^RestDelete\.' --output-on-failure
```

| ファイル | テスト数 | 対象 |
|:---------|---------:|:-----|
| [`item.entity.test.cpp`](tests/item.entity.test.cpp) | 9 | 名前のルール、idの範囲、項目の生成 |
| [`item_catalog.service.test.cpp`](tests/item_catalog.service.test.cpp) | 6 | add、満杯のテーブル、find |
| [`item_removal.test.cpp`](tests/item_removal.test.cpp) | 4 | 削除、2回目の削除、範囲外のid、空いたidの再利用 |
| [`items.api.test.cpp`](tests/items.api.test.cpp) | 4 | `204`、2回目の`404`、不正なid、コレクションへの`405` |

`cd "SD Card" && pnpm run test`はカバレッジ100パーセントで2件のページテストを実行します。

## 前提条件

| ツール | バージョン | 用途 |
|:-------|:-----------|:-----|
| Arduino Uno R4 Minima | | ボード |
| microSDスロット付きのW5100、W5500、ENC28J60シールド | | ネットワークとカード |
| arduino-cli | 1.5.1 | スケッチのビルド |
| arduino:renesas_unoコア | 1.6.0 | ボードパッケージ |
| EthernetとSdFat | 2.0.2と2.3.0 | ネットワークとカードのライブラリ |
| Node.jsとpnpm | 24と12.15.0 | ページのビルド |

## はじめに

```bash
arduino-cli lib install Ethernet@2.0.2 SdFat@2.3.0
arduino-cli compile --fqbn arduino:renesas_uno:minima --library ../.. .
arduino-cli upload --fqbn arduino:renesas_uno:minima --port /dev/cu.usbmodem1101 .
cd "SD Card" && pnpm install && pnpm run build
```

`SD Card/dist`をカードにコピーしてから：

```bash
curl -i -X DELETE http://192.168.1.177/api/items/2
```

## 技術スタック

| 分類 | 技術 |
|:-----|:-----|
| ボード | Arduino Uno R4 Minima：フラッシュ262,144バイト中73,856バイト、RAM 32,768バイト中5,904バイト |
| HTTP | Madoguchi、ヒープを使わないC++11 |
| カード | SdFat 2.3.0 |
| ネットワーク | Ethernet 2.0.2 |
| ホストテスト | googletest 1.18.0、CMake |
| ページ | TypeScript 7.0.2、Vite 8.3.5、CDNから読み込むPico CSS 2.1.1、Vitest 5.0.5 |
| 解析 | clang-tidy 23.1.3、cppcheck 2.22.0、clang-format 23.1.3、Biome 2.6.3 |
