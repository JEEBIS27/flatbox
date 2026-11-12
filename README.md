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
Neutral attack / Neutral specialは中央で1フレーム待機し、ボタンを最低2フレーム出力します。
その後は通常のスティック操作に戻し、物理ボタンを保持している間はボタン出力だけを維持します。
最低2Fの押下段階と、解除までの保持段階は別の行で定義します。
自動のボタン解除段階・事後クリアランスはありません。短いタップも最低2フレーム出力します。
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
just build
```

ホストテストは継続時間、段階遷移、予約置換、同一マクロの連続実行、解除待ち、NILの分岐を検証します。
ゲーム上のタイミングは実機で確認してください。特にNIL1 → NIL2、NIL2 → NIL1、短い先行入力、
Dragonの連続入力、およびNILのC-up追加入力が確認対象です。

### NIL派生の先行入力

NIL1/2の実行開始から派生段階まで、NAまたはC-upの押下を予約できます。
短く押して離しても予約を保持し、派生段階に入るとその入力を5フレーム出力します。
派生開始前は最新の押下で予約を置き換えます。同時押下はC-UP優先です。
派生開始後は選択を固定し、途中で別の派生に切り替えません。
マクロ開始時に既に保持しているNA/C-upも受け付けます。
予約がなければ従来どおり派生と任意のリカバリーを省略します。
派生予約は実行中のマクロだけに属し、次のマクロの予約とは別枠です。
実行終了・切替・モード無効化で消去され、次の実行へ持ち越しません。
方向は従来どおり派生の実行時に押している方向を使用します。

Smashの↑・NA配置とNA/NB自動入力はニュートラル修飾キー導入前に戻しています。
↑はGPIO28、NAはGPIO10です。共通ニュートラル修飾キーとTap/Holdは使用しません。

### Dragon起動に使ったL1の消費

Dragonの開始要求（先行入力による予約を含む）が受理された時点でL1を保持していた場合、
その押下を消費し、L1を離すまで通常出力を抑制します。Dragon終了やキャンセルでも解除しません。
離して押し直せば通常どおり使えます。消費中のL1は攻撃系ボタンの判定からも除外します。
他モードで物理L1がR2に割り当てられている場合も、同じ物理押下を抑制します。
L1を使わずに起動したDragonは、その後に押したL1を消費しません。

### Dragon終了後の方向固定と復帰

L1起動専用の中央回帰は行いません。起動ボタンにかかわらず、Dragon終了後は最後の横方向を維持します。
方向キーの新規押下・押し直し、または左右同時押しから片側だけになる操作で固定を解除し、
中央を3F出力してから現在の方向入力へ復帰します。全方向キーまたはSJの解放でも同様に解除します。
一部の方向を離すだけでは固定を維持しますが、左右が片側に切り替わる操作は解除対象です。
終了時に全方向キーまたはSJを離している場合は固定しません。
L1の解放だけでは方向固定を解除せず、L1の消費は離すまで継続します。
次のマクロの開始とマクロモード無効化でも固定を解除します。
予約Dragonの中央段階はステップ定義を使用し、終了後の固定を重ねません。
Dragonの向きは保持時間カウンターではなく実際の左右キーから決定します。
予約中も左右が片側に確定したら向きを更新し、実行中のDragonの向きは維持します。
Dragon途中で方向操作があった場合は復帰要求を保持し、Dragon本体の全フレーム終了後に中央3Fを開始します。
方向を短く押して離しても要求は消えません。復帰先は3F経過時点の実際の方向入力です。
途中の方向操作だけでは次のDragonを予約せず、攻撃ボタンの新規押下では従来どおり予約できます。
次のDragonが予約されている場合は、その予約実行を優先して通常入力への復帰要求をリセットします。

固定解除前のNA/C-Downは中央3F→後ろ下2Fを挟んで実行します。
後ろ下は終了するDragonの向きと逆の斜め下です。途中で左右を変えてもこの基準は変わりません。
Dragon途中でもNA/C-Downの短いタップを予約でき、Dragonの全フレーム終了後に開始します。
NAはその後中央でAを最低2F押し、長押し時はAだけ保持してスティックを通常操作へ戻します。
C-Downはその後左スティック真下と右スティック真下を2F出力し、どちらも通常操作へ戻します。
物理C-Downを押し続けても2Fを延長せず、離すまでその物理押下を消費します。
中央・後ろ下の準備中は通常攻撃ボタンとCスティックの出力を抑えます。
NA/C-Downの予約と次マクロの予約は最新の要求で置き換え、同時NA/C-DownはC-Downを優先します。
解除済みの状態で押すNA/C-Downは、通常の入力処理を使用します。

```bash
c++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/dragon_neutral_hold_test.cpp -o /tmp/flatbox-dragon-neutral-test
/tmp/flatbox-dragon-neutral-test
```

### TILTEの倒し量

TILTE中は方向入力を即座に最大70%の倒し量に制限します。時間経過や速度による移動はありません。
方向変更も即座に反映し、方向解放で中央、TILTE解放で通常入力に戻ります。
設定は `src/tilt_settings.h` の `Magnitude = 0.70` と `MaxVertical = 0.60` です。
上下方向のY成分はそれぞれ60%を上限とし、量子化時は中央側に切り捨てます。
斜めもベクトル全体で70%以下、元から弱い入力はそのまま維持します。
通常入力とNIL1に適用し、Dragon・NIL2・Yogaの固定出力とDragon後の方向固定は維持します。
値はUSB出力座標での比率であり、ゲーム内入力量との一致やしゃがみ・はじき防止は未保証です。

```bash
c++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/tilt_output_test.cpp -o /tmp/flatbox-tilt-test
/tmp/flatbox-tilt-test
```

### NAとCスティックの優先順位

NAとC方向ボタンが重なった場合はCスティックを優先します。
NA補助が既に動作中でもC方向を押すと解除し、NAによる方向中央化とC入力抑制を取り除きます。
C方向を離した後もNAを保持しているだけでは再発動せず、NAを離して押し直すと再び使えます。
NIL派生でもNAとC-UPの同時入力・C-UP保持中のNA入力はC-UPを優先します。
B押下・他のマクロ・モード・反対方向同時押しによる既存のC入力抑制は変更しません。

### 入力安定化・二重発動防止

`src/input_settings.h` のbool定数 `DebounceEnabled` でチャタリング対策を切り替えます。
`true`（初期値）は有効、`false` は無効です。切り替え後は再ビルド・書き込みが必要です。
無効でも1周期内の入力スナップショットと押下消費・先行入力の制御は維持します。
有効時、物理入力は `DebounceUs = 5000`（5ms）だけ押下・解放が
安定してから確定し、同じ読み取り周期では全処理が同じ入力状態を参照します。
短い接点の揺れを先行入力として予約しないための処理で、通常入力にも最大約5msの遅延が加わります。

NIL起動のB/Y、NB起動のY、NA起動のNAは、その押下を離すまで消費済みとして扱います。
マクロ終了後に保持中のB/Yが通常出力として再送されることを防ぎます。
先行入力は、ボタンを安定して離し、押し直すことで従来どおり予約できます。
SJなど他の条件だけが切り替わっても、保持中のB/Yから新たなNILは起動しません。
Dragonも方向履歴の条件変化だけでは再予約せず、関連する物理入力の変化が必要です。
NA/NBのニュートラル期間中は通常Y出力やNAからの追加A出力を重ねず、
明示した最低2フレームの押下と、その後の長押しだけを出力します。
NIL/Dragon開始時にはNA/NB補助を解除します。

```bash
c++ -std=c++11 -Wall -Wextra -Werror -Isrc tests/input_filter_test.cpp -o /tmp/flatbox-input-test
/tmp/flatbox-input-test
```

### 攻撃ボタンによるマクロ再予約

Dragon成立後はSJ・下・左右どちらか一方・TILTE未押下を保持したまま、
攻撃ボタンを押し直すたび次のDragonを要求できます。別の攻撃ボタンの新規押下も受け付けます。
方向キーを押し直す必要はなく、成立済みの間は方向履歴の条件も再要求しません。
条件キーを外すと成立済み状態を解除し、次の初回成立には従来の方向履歴条件が必要です。
長押しだけでは繰り返しません。NIL1/2もSJなどの条件を保持したままB/Yの押し直しで予約できます。
予約枠は引き続き最新1件で、押下回数分を列として蓄積する方式ではありません。

L1 Dragon後の方向固定中でも、新しく成立したDragonが始まる際には中央固定を解除します。

### Dragonの予約実行の開始段階

通常のDragonは下2F→斜め下2F→横1Fです。
予約されたDragonは中央1F→前2Fを加えてから、同じ下2F→斜め下2F→横1Fを実行します。
連続時の出力は「下→斜め下→横→中央1F→前2F→下→斜め下→横」となります。
中央・前の段階は通常ボタンを抑制します。前方向は次のDragonの要求時に記録した左右方向です。
定義はステップ列を共通化し、通常開始だけ `DragonDown` へ飛ばします。
通常は5F、予約実行は追加3Fを含む8Fです。NILなど別マクロから予約実行する場合も同じです。
