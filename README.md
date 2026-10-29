# racegame

鈴鹿サーキットを走る、3D レースゲームです。車は 1 台です。最終的な目標は、ドリフトを最大限気持ちよくすることです。

コースの線形は OpenStreetMap（relation 284570、Suzuka International Racing Course）、高さは国土地理院の標高タイルです。

- 地図データ: © OpenStreetMap contributors
- 標高: 出典 国土地理院

画像は `assets/graphics/`、音声は `assets/sound/` に置きます。コースデータは `assets/track/suzuka.txt` です。

## 開き方

Cursor または VS Code でこのフォルダを開き、**Reopen in Container** を選びます。初回はイメージのビルドに数分かかります。

## 操作

キーボードとゲームパッドは、一般的な 3D レースゲームと同じです。

| 操作 | キーボード | ゲームパッド |
|---|---|---|
| アクセル | W / ↑ | RT（R2）。十字キー上でも可 |
| ブレーキ / 後退 | S / ↓ | LT（L2）。十字キー下でも可 |
| ステアリング | A D / ← → | 左スティック、十字キー左右 |
| サイドブレーキ | Space | A（×） |
| リスタート | R | Back（View / Share） |
| カメラ | C | Y（△） |
| ポーズ | P | Start（Menu / Options） |
| 操作表示 | H | |

画面は追従、近め、遠めの 3 つです。ギアはオートマです。

## ドリフト

車の動きは `src/domain/Car.cpp` にあります。

- `Car::stepTire`：前後 2 輪の自転車モデルです。タイヤの滑り角から横向きの力を求めます（Pacejka の簡易式）。
  アクセルを踏むほど、またサイドブレーキを引くと後輪のグリップが減り、車体の後ろが外へ流れます。
- `Car::stepGrip`：横滑りしない動きです。低速でタイヤの力の計算が不安定になるときに使います。
- アーケード向けの補助：滑っている向きへカウンターステアを自動で少し当てます。
  ドリフト中は、入力で決めた角度へ車体の回転を寄せます。

ドリフト中の操作は次のとおりです。

| やりたいこと | 操作 |
|---|---|
| ドリフトに入る | 曲がりながらサイドブレーキ、または速度が出ている状態でアクセル全開＋ステア全開 |
| 角度を深くする | 曲がる向きへステアを切る |
| 角度を浅くする | 逆へステアを切る |
| ドリフトを終える | アクセルを戻す |

調整用の数値は、`Car.cpp` の先頭に定数としてまとめてあります。

## ブラウザ版

VS Code では、実行とデバッグで「Chrome で racegame をデバッグ」を選んで F5 を押すと、ビルド・サーバーの起動・Chrome の起動まで自動で行います。

```bash
emcmake cmake -B build-web
cmake --build build-web
python3 -m http.server 8080
```

`http://localhost:8080/build-web/racegame.html`

## Linux 版

```bash
cmake -B build-native
cmake --build build-native
./build-native/racegame
```

画面は `http://localhost:6080`（パスワード `vscode`）です。コンテナ内では音は鳴りません。

ロジックだけの確認:

```bash
./build-native/racegame --self-test
```
