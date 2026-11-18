![](doc/images/ggpo_header.png)

&nbsp; _[![Appveyor build status](https://img.shields.io/appveyor/ci/pond3r/ggpo/master.svg?logo=appveyor)](https://ci.appveyor.com/project/pond3r/ggpo/branch/master)_

## GGPOとは

従来の技術はプレイヤーの入力に遅延を織り込んで通信を行っており、その結果反応が遅く、ラグを感じるプレイ感になっていました。ロールバックネットワーキングは入力予測と投機的実行を行って、プレイヤーの入力を即座に送信するため、遅延を感じさせないネット環境をもたらします。ロールバックがあれば、タイミングや相手の動きや効果音に対する反応、指が覚えている入力、これらオフラインで行えた内容が、そのままオンラインでも行えます。GGPOネットワーキングSDKは、ロールバックネットワーキングを新作や発売されているゲームに極力簡単に組み込めるよう作られています。

これまでのGGPOについてさらに知りたい方は、http://ja.ggpo.net/ をご覧ください。

このリポジトリにコードやドキュメント、SDKのサンプルアプリケーションが収められています。

## SDKのインストールまたはビルド

SDKは、Windows（MSVCおよびMinGW-w64、x64とx86）、Linux、Android（arm64-v8a、armeabi-v7a、x86_64）、macOS 11以降（arm64とx86_64のユニバーサルバイナリ。macOS 15と26でテスト済み）をサポートしています。Vector WarはWindows（Win32/GDI）、Linux（SDL2）、Android（SDL2/タッチ操作）、macOS（SDL2）で動作します。iOSなど未対応のターゲットでは、構成時に明示的にエラーになります。

[Releases](https://github.com/irregularthanis/ggpo-next/releases)からテスト済みのSDK（WindowsのZIP、Linuxのパッケージ、AndroidおよびmacOSのtar.gz）を利用する方法と、ソースからCMakeでビルドする方法があります。アーカイブの選択、インストール、CMakeパッケージ、および統合手順については、英語版の[開発者ガイド](doc/DeveloperGuide.md#install-the-sdk)を参照してください。

macOS版Vector War（`.dmg`）は署名されていないため、初回起動時に許可が必要です。macOS 15以降では、システム設定の「プライバシーとセキュリティ」で「このまま開く」を選択します。macOS 11～14では、右クリックして「開く」を選択します。

## サンプルアプリケーション

ソースディレクトリ内のVector Warには、GGPOを使った2つのクライアントを同期する単純なアプリケーションが含まれています。コマンドライン引数は以下の通りです。

```
vectorwar.exe  <localport>  <num players> ('local' | <remote ip>:<remote port>) for each player
```

2～4プレイヤーでのゲーム開始方法の例については、binディレクトリにある.cmdファイルを参照してください。

## ライセンス

GGPOはMITライセンスの元で利用ができます。つまり、GGPOは商用、非商用のどちらでも無料で利用ができます。クレジットの掲載、帰属は必要ありませんが、してくださると大変うれしく思います。
