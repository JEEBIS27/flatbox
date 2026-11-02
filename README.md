This is code for a USB arcade controller, meant to be used with RP2040-based boards.

It works with Flatbox [rev4](../hardware-rev4) and [rev5](../hardware-rev5). Precompiled binaries are included.

## Build (Linux/WSL)

Run:

```bash
just build
```

This runs `./build.sh` to build both firmware variants. You can also run the script directly.
On Nix hosts, missing build tools are supplied automatically through the pinned
`shell.nix` environment. The first build downloads the toolchain and requires network access.

The Pico SDK fetches its required `picotool` automatically, and the script checks for the host toolchain (`cmake`, `arm-none-eabi-gcc`, `g++`, and `ninja`/`make`) before configuring.
If `PICO_SDK_PATH` is not set, the script configures CMake with `-DPICO_SDK_FETCH_FROM_GIT=ON`.
Default build output directory is `build-local` (you can change it with `just build <build-dir>` or `./build.sh <build-dir>`).
This keeps new builds separate from the legacy build files tracked in this repository.

## マクロ定義と先行入力

マクロ関係は次の3つに分けています。

- `src/macro_definitions.inc`: 各マクロのステップ、開始条件、終了条件、入力抑制設定。
- `src/macro_player.h`: 共通のステップ再生・予約処理。ハードウェアに依存しません。
- `src/flatbox.cpp`: 入力読み取り、状態更新、通常入力の計算、最後にマクロ出力を適用。

### ステップの書き方

各行は `{ 継続フレーム数, 方向または処理, 追加ボタン, 通常入力の抑制設定 }` です。
後ろ2項目は省略できます。1フレームは内部60 Hzクロックの1周期です。

```cpp
static const MacroStep example_steps[] = {
    { 2_f, Stick::Down },
    { 2_f, Stick::Diagonal },
    { 1_f, Stick::Side, BButton },
    { 1_f, Stick::Neutral, Jump | BButton, BlockCpad },
};
static constexpr auto example = define_macro("Example", example_steps);
```

この定義は合計6フレームです。別途、合計時間・終了フレーム・タイマーを定義する必要はありません。
`Keep` は通常入力を通し、`Neutral` はスティックを中央にします。
`Diagonal` と `Side` は予約時に記録した左右方向を使います。
ボタンは通常入力に追加されます。通常ボタンを抑える場合は `BlockButtons` を指定します。

可変方向や分岐は `medium_movement`、`nil1_followup` のような名前付き処理を指定します。
NILの段階には `NilMovement`、`NilRelease` などの名前を付け、ジャンプも絶対フレーム値ではなく
`seek(NilMovement, 3)` のように「段階と、その中で省略するフレーム数」で指定します。

### 入力抑制と終了方法

`BlockCpad` は通常のC入力、`BlockButtons` は通常ボタン、`BlockL1` はSmashモードのL1を抑制します。
`CenterDirections` は通常方向の計算を中央にします。
マクロ全体に適用する設定は `define_macro` の第3引数、段階固有の設定は各行の第4項目です。
通常入力処理は経過フレーム数ではなく、この設定を参照します。

通常は最終行の継続時間が終わると完了します。
`Finish::HoldUntilRelease` を指定した入力補助マクロは、最終行に入ったらその出力を維持し、
登録した解除条件で終了します。最終行の時間はこの方式では消費しません。
Neutral attack / Neutral specialは入力を2フレーム出したあと、`Keep` 段階で解除を待ちます。
Yogaは最終方向を維持します。
左右・上下の保持カウンターもマクロ実行の有無にかかわらず60 Hzで進めます。

### 開始条件と先行入力

`macro_bindings` に定義・実行先・開始条件・解除条件・予約方式を登録します。
`TriggerMode::QueuedEdge` は先行入力対象、`HeldHelper` は独立した入力補助です。
予約対象は配列の上から優先して選び、同時入力ではDragon → NIL2 → NIL1の順です。

NIL1・NIL2・Dragonは実行中1件と次の予約1件を共有します。
動作中もクールタイム中も開始条件の成立への変化を検出し、短く押して離した予約も保持します。
新しい要求は古い予約を置き換えます。押しっぱなしでは連射しません。
Dragonは2 + 2 + 1フレームで完了し、予約された次のマクロへ進みます。
マクロモードを無効にすると、この実行状態と予約を消去します。
Neutral attack / Neutral special / Yogaは独立した入力補助で、この予約枠は使いません。

### 検証

```bash
c++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/macro_player_test.cpp -o /tmp/flatbox-macro-test
/tmp/flatbox-macro-test
c++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/macro_definitions_test.cpp -o /tmp/flatbox-definitions-test
/tmp/flatbox-definitions-test
c++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/neutral_direction_filter_test.cpp -o /tmp/flatbox-neutral-test
/tmp/flatbox-neutral-test
just build
```

ホストテストは継続時間、段階遷移、予約置換、同一マクロの連続実行、解除待ち、NILの分岐を検証します。
ゲーム上のタイミングは実機で確認してください。特にNIL1 → NIL2、NIL2 → NIL1、短い先行入力、
Dragonの連続入力、およびNILのC-up追加入力が確認対象です。

### Smashのニュートラルボタン

Smashでは旧↑位置（GPIO28）を共通ニュートラル修飾キー、旧NA位置（GPIO10）を↑にします。
方向配置はPro PCと同じ、↑10・←27・↓15・→26です。Normal PCの方向配置は変更していません。
ニュートラル単独では攻撃を出しません。押しながら通常の攻撃・必殺技ボタンを使います。
ニュートラルを押した時点で保持中の方向は無効にし、押下中に新しく押した方向は優先します。
新しく押した方向を離せば中央に戻ります。無効化された方向も離して押し直せば有効になります。
ニュートラルを離すと保持中の方向はすべて復帰します。同じ読み取り周期でのニュートラルと方向の新規押下は方向優先です。
右スティックはそのまま操作できます。
SmashではNeutral attack / Neutral specialの自動入力補助を使用しません。
ニュートラル押下中は新しいDragon・Yoga・NILの開始を抑制します。
既に実行中のマクロや予約は維持し、マクロの明示的な方向出力は通常入力より優先します。
NILの旧NA追加入力はニュートラル＋Aに変更します。他モードのNA動作は維持します。
