![](doc/images/ggpo_header.png)

[![Linux](https://github.com/irregularthanis/ggpo-next/actions/workflows/linux-compatibility.yml/badge.svg?branch=master)](https://github.com/irregularthanis/ggpo-next/actions/workflows/linux-compatibility.yml)
[![Windows](https://github.com/irregularthanis/ggpo-next/actions/workflows/windows-compatibility.yml/badge.svg?branch=master)](https://github.com/irregularthanis/ggpo-next/actions/workflows/windows-compatibility.yml)
[![MinGW](https://github.com/irregularthanis/ggpo-next/actions/workflows/mingw-compatibility.yml/badge.svg?branch=master)](https://github.com/irregularthanis/ggpo-next/actions/workflows/mingw-compatibility.yml)
[![Android](https://github.com/irregularthanis/ggpo-next/actions/workflows/android-compatibility.yml/badge.svg?branch=master)](https://github.com/irregularthanis/ggpo-next/actions/workflows/android-compatibility.yml)
[![macOS](https://github.com/irregularthanis/ggpo-next/actions/workflows/macos-compatibility.yml/badge.svg?branch=master)](https://github.com/irregularthanis/ggpo-next/actions/workflows/macos-compatibility.yml)
[![Cross-platform E2E](https://github.com/irregularthanis/ggpo-next/actions/workflows/cross-platform-e2e.yml/badge.svg?branch=master)](https://github.com/irregularthanis/ggpo-next/actions/workflows/cross-platform-e2e.yml)

([English](README.md))

> **注:** この日本語版はAIで翻訳したもので、日本語ネイティブによるレビューは受けていません。英語版と内容が異なる場合は、[英語版](README.md)が正となります。

## GGPO NEXTとは

このリポジトリは、[オリジナルのGGPO](https://github.com/pond3r/ggpo) SDKライブラリの（もしかすると一時的な）フォークです。ここにあるものはすべて先人たちの成果の上に成り立っています。評価や寄付などは、こちらではなくオリジナルへお願いします。

### なぜ作ったのか
このフォークが現在の形で存在する理由は次のとおりです。
 - ランタイム、APIリファレンス、ビルドシステムを文書化する。
 - Windows以外のプラットフォームにも対応する。
 - きちんとしたリリースパイプラインを用意する。

### オリジナルのGGPOからの変更点

**プラットフォーム**

| プラットフォーム | SDK | Vector Warサンプル | CIでのテスト環境 |
| --- | --- | --- | --- |
| Windows、MSVC | x64、x86 | Win32/GDI | `windows-2022` |
| Windows、MinGW-w64 | x64、x86 | — | `windows-2022` |
| Linux | x86_64（GCCまたはClang） | SDL2 | Ubuntu 24.04 |
| Android | arm64-v8a、armeabi-v7a、x86_64、API 21以上 | タッチ操作対応のSDL2（APK） | ARMおよびx86_64ランナー、API 21と35のエミュレーター |
| macOS | 11以降、arm64 + x86_64のユニバーサル | SDL2アプリ | macOS 15と26、Apple SiliconとIntel |

すべてのプラットフォームは、小さなOSアダプターの背後にある1つのポータブルなコアを共有し、同じ公開APIと通信フォーマットを使い、互いに対戦できます。CIでは、Linux、Windows（MSVC）、Android、macOSのあらゆる順序の組み合わせ（同じプラットフォーム同士も含む）を、実際のネットワーク越しに2台のホストで対戦させます。さらにMinGWと他のプラットフォームとの対戦、およびデプロイ済みのサーバーを経由した4プラットフォームでのライブ対戦も行います。

**SDKとパッケージ**

- すべてのプラットフォームに対して1つのCMakeプロジェクト。どのSDKにも共有ライブラリと静的ライブラリの両方が含まれ、1つの`GGPO::GGPO` CMakeパッケージから利用できます。
- タグごとにテスト済みのリリースパッケージ：WindowsのZIP（MSVCとMinGW）、LinuxのDEB、RPM、tar.gz、複数ABI対応のAndroid SDK、ユニバーサルなmacOS SDK、そしてすぐに実行できるVector Warのビルド（Windows、Linux、AndroidのAPK、macOSの`.dmg`）。いずれもアップロード前に展開またはインストールして検証され、`SHA256SUMS`が付きます。
- オリジナルのWindows ABI、エクスポート一覧、パックされた通信レイアウトは凍結されており、変更のたびに検証されます。

**インターネット対戦（Vector War）**

- スタート画面と、2～4人用のマッチメイキング（ロビーコードは任意）。小さなUDPの[マッチメイキング兼リレーサーバー](services/matchmaking)を使います。
- UDPホールパンチングと、プレイヤーの組ごとに自動で選ばれるリレーへのフォールバック。ゲーム内の凡例に、各プレイヤーのプラットフォームと経路が表示されます。
- クロスプラットフォームの決定性：すべてのプラットフォームで一貫した浮動小数点設定と固定の三角関数テーブルを使い、CPU、コンパイラー、数学ライブラリが異なっても同期を保ちます。
- DuckDNSの名前を持つOCI無料枠サーバー用のTerraform。デプロイのたびに、そのサーバーを経由した実際の4プラットフォーム対戦で検証されます。

**テストとCI**

- プラットフォームごとの互換性ワークフロー、ネイティブのクロスプラットフォームE2Eマトリクス、エミュレートしたNATトポロジー、毎週のサニタイザー実行（ASanとUBSan）、そして変更の影響を受けるものだけを実行する変更ルーティング。[CIワークフロー](doc/architecture/CIWorkflows.md)（英語）を参照してください。

**ドキュメント**

- [アーキテクチャ](doc/architecture/README.md)、完全な[APIリファレンス](doc/architecture/API.md)、全プラットフォーム向けの[開発者ガイド](doc/DeveloperGuide.ja.md)、[テストアーキテクチャ](doc/architecture/Testing.md)、[CIワークフロー](doc/architecture/CIWorkflows.md)。開発者ガイド以外は英語のみです。

### アップストリームへマージする予定はありますか？
おそらく。十分に成熟したら連絡するつもりです。オリジナルのメンテナー次第です。

### これはバイブコーディングですか？
いいえ。ただしAIの支援を受けています。

  
  
  
---



## GGPOとは

従来の技術はプレイヤーの入力に遅延を織り込んで通信を行っており、その結果反応が遅く、ラグを感じるプレイ感になっていました。ロールバックネットワーキングは入力予測と投機的実行を行って、プレイヤーの入力を即座に送信するため、遅延を感じさせないネット環境をもたらします。ロールバックがあれば、タイミングや相手の動きや効果音に対する反応、指が覚えている入力、これらオフラインで行えた内容が、そのままオンラインでも行えます。GGPOネットワーキングSDKは、ロールバックネットワーキングを新作や発売されているゲームに極力簡単に組み込めるよう作られています。

これまでのGGPOについてさらに知りたい方は、http://ja.ggpo.net/ をご覧ください。

このリポジトリにコードやドキュメント、SDKのサンプルアプリケーションが収められています。

アーキテクチャの内部構造は[`doc/architecture`](doc/architecture/README.md)に記載されています。[公開APIリファレンス](doc/architecture/API.md)は、エクスポートされたすべての関数、コールバック、型、イベント、戻り値を網羅しています（いずれも英語）。ビルド、インストール、アプリケーションへの組み込みについては[開発者ガイド](doc/DeveloperGuide.ja.md)を参照してください。

## SDKのインストールまたはビルド

SDKは、Windows（MSVCおよびMinGW-w64、x64とx86）、Linux、Android（arm64-v8a、armeabi-v7a、x86_64）、macOS 11以降（arm64とx86_64のユニバーサルライブラリ1つ。macOS 15と26でテスト済み）をサポートしています。Vector WarはWindows（Win32/GDI）、Linux（SDL2）、Android（SDL2/タッチ操作）、macOS（SDL2）で動作します。iOSなど未対応のSDKターゲットでは、構成時に明示的にエラーになります。

次のどちらかを選んでください。

- **リリースSDK：** テスト済みのWindowsのZIP、Linux x86-64のパッケージ、複数ABI対応のAndroidのtar.gz、ユニバーサルなmacOSのtar.gzを[Releases](https://github.com/irregularthanis/ggpo-next/releases)からダウンロードします。アプリケーション開発者にとって最短の方法です。
- **ソースからのビルド：** WindowsではCMakeを直接、LinuxではリポジトリのUbuntu 24.04 Dockerイメージを、AndroidではCMakeとAndroid NDKを、macOSではCMakeとXcodeを使います。

どのSDKにも共有ライブラリと静的ライブラリの両方が含まれます。アプリケーションは`GGPO::GGPO`（デフォルトは共有）をリンクし、`-DGGPO_USE_STATIC=ON`で静的を選びます。インストール、CMakeの有無それぞれでの各形式のリンク、実行時の配置、ソースビルドのコマンドについては[開発者ガイド](doc/DeveloperGuide.ja.md#install-the-sdk)を参照してください。

## サンプルアプリケーション

[`src/apps/vectorwar`](src/apps/vectorwar)のVector Warは、GGPOを使って2～4人のプレイヤーを同期する小さなゲームです。引数なしで起動するとスタート画面が表示され、インターネット経由のマッチメイキング（リリース版では公開サーバーが入力済み）か、アドレス指定による直接対戦を選べます。コマンドラインでの直接対戦は次のとおりです。

```
VectorWar <localport> <num players> ('local' | <remote ip>:<remote port>) for each player
```

[`bin`](bin)にある`.cmd`ファイルで、1台のWindowsマシン上で2人、3人、4人のゲームを開始できます。リリースのダウンロード、プラットフォームごとの注意点、マッチメイキングのコマンドラインについては[開発者ガイド](doc/DeveloperGuide.ja.md#run-the-standalone-vector-war-sample)を参照してください。

## ライセンス

GGPOはMITライセンスの元で利用ができます。つまり、GGPOは商用、非商用のどちらでも無料で利用ができます。クレジットの掲載、帰属は必要ありませんが、してくださると大変うれしく思います。
