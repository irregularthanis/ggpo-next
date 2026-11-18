![](doc/images/ggpo_header.png)

[![Linux](https://github.com/irregularthanis/ggpo-next/actions/workflows/linux-compatibility.yml/badge.svg?branch=master)](https://github.com/irregularthanis/ggpo-next/actions/workflows/linux-compatibility.yml)
[![Windows](https://github.com/irregularthanis/ggpo-next/actions/workflows/windows-compatibility.yml/badge.svg?branch=master)](https://github.com/irregularthanis/ggpo-next/actions/workflows/windows-compatibility.yml)
[![MinGW](https://github.com/irregularthanis/ggpo-next/actions/workflows/mingw-compatibility.yml/badge.svg?branch=master)](https://github.com/irregularthanis/ggpo-next/actions/workflows/mingw-compatibility.yml)
[![Android](https://github.com/irregularthanis/ggpo-next/actions/workflows/android-compatibility.yml/badge.svg?branch=master)](https://github.com/irregularthanis/ggpo-next/actions/workflows/android-compatibility.yml)
[![macOS](https://github.com/irregularthanis/ggpo-next/actions/workflows/macos-compatibility.yml/badge.svg?branch=master)](https://github.com/irregularthanis/ggpo-next/actions/workflows/macos-compatibility.yml)
[![Cross-platform E2E](https://github.com/irregularthanis/ggpo-next/actions/workflows/cross-platform-e2e.yml/badge.svg?branch=master)](https://github.com/irregularthanis/ggpo-next/actions/workflows/cross-platform-e2e.yml)

## GGPOとは

従来の技術はプレイヤーの入力に遅延を織り込んで通信を行っており、その結果反応が遅く、ラグを感じるプレイ感になっていました。ロールバックネットワーキングは入力予測と投機的実行を行って、プレイヤーの入力を即座に送信するため、遅延を感じさせないネット環境をもたらします。ロールバックがあれば、タイミングや相手の動きや効果音に対する反応、指が覚えている入力、これらオフラインで行えた内容が、そのままオンラインでも行えます。GGPOネットワーキングSDKは、ロールバックネットワーキングを新作や発売されているゲームに極力簡単に組み込めるよう作られています。

これまでのGGPOについてさらに知りたい方は、http://ja.ggpo.net/ をご覧ください。

このリポジトリにコードやドキュメント、SDKのサンプルアプリケーションが収められています。

## SDKのインストールまたはビルド

SDKは、Windows（MSVCおよびMinGW-w64、x64とx86）、Linux、Android（arm64-v8a、armeabi-v7a、x86_64）、macOS 11以降（arm64とx86_64のユニバーサルバイナリ。macOS 15と26でテスト済み）をサポートしています。Vector WarはWindows（Win32/GDI）、Linux（SDL2）、Android（SDL2/タッチ操作）、macOS（SDL2）で動作します。iOSなど未対応のターゲットでは、構成時に明示的にエラーになります。

[Releases](https://github.com/irregularthanis/ggpo-next/releases)からテスト済みのSDK（WindowsのZIP、Linuxのパッケージ、AndroidおよびmacOSのtar.gz）を利用する方法と、ソースからCMakeでビルドする方法があります。アーカイブの選択、インストール、CMakeパッケージ、および統合手順については、英語版の[開発者ガイド](doc/DeveloperGuide.md#install-the-sdk)を参照してください。

macOS版Vector War（`.dmg`）は署名されていないため、初回起動時に許可が必要です。macOS 15以降では、システム設定の「プライバシーとセキュリティ」で「このまま開く」を選択します。macOS 11～14では、右クリックして「開く」を選択します。

## サンプルアプリケーション

[`src/apps/vectorwar`](src/apps/vectorwar)のVector Warは、GGPOを使って2～4人のプレイヤーを同期する小さなゲームです。引数なしで起動するとスタート画面が表示され、インターネット経由のマッチメイキング（リリース版では公開サーバーが入力済み）か、アドレス指定による直接対戦を選べます。コマンドラインでの直接対戦の引数は以下の通りです。

```
VectorWar <localport> <num players> ('local' | <remote ip>:<remote port>) for each player
```

1台のWindowsマシンで2～4プレイヤーのゲームを開始する例については、[`bin`](bin)ディレクトリにある.cmdファイルを参照してください。各プラットフォームのダウンロードと詳細は、英語版の[開発者ガイド](doc/DeveloperGuide.md#run-the-standalone-vector-war-sample)を参照してください。

## ライセンス

GGPOはMITライセンスの元で利用ができます。つまり、GGPOは商用、非商用のどちらでも無料で利用ができます。クレジットの掲載、帰属は必要ありませんが、してくださると大変うれしく思います。
