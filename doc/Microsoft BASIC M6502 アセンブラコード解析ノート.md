# Microsoft BASIC M6502 アセンブラコード解析ノート

## 1. 基本構成

### プラットフォーム設定 (REALIO)
- 0: PDP-10 シミュレーション
- 1: MOS TECH KIM-1
- 2: Ohio Scientific (OSI)
- 3: Commodore PET
- 4: Apple II

### 主要な設定フラグ
- INTPRC=1: 整数配列サポート
- ADDPRC=1: 追加精度サポート
- LNGERR=0: 短いエラーメッセージ
- TIME=0: 時計機能なし
- EXTIO=0: 外部I/Oなし
- DISKO=0: セーブ/ロード機能なし
- ROMSW=1: ROM版

### メモリレイアウト
- LINLEN=72: 端末行長
- BUFLEN=72: 入力バッファサイズ
- STKEND=511: スタック終端
- ROMLOC=^O20000: ROM開始アドレス

## 2. 重要なデータ構造 (Page Zero Variables)

### 制御フラグ
- CHARAC: 区切り文字
- ENDCHR: もう一つの区切り文字
- COUNT: 汎用カウンター
- DIMFLG: DIM文処理フラグ
- VALTYP: 型指示子 (0=数値, 1=文字列)
- INTFLG: 整数フラグ
- DORES: 予約語処理フラグ
- SUBFLG: 添字変数許可フラグ
- INPFLG: INPUT/READ処理フラグ

### メモリポインター
- TXTTAB: プログラムテキスト開始ポインター
- VARTAB: 単純変数領域開始ポインター
- ARYTAB: 配列テーブル開始ポインター
- STREND: 使用中ストレージ終端

### 文字列処理
- TEMPPT: 一時記述子ポインター
- LASTPT: 最後に使用された文字列一時ポインター
- TEMPST: 一時記述子ストレージ

### 端末処理
- CHANNL: チャンネル番号
- TRMPOS: 端末キャリッジ位置
- LINWID: 行幅
- LINNUM: 行番号ストレージ
- BUF: 入力バッファ

## 3. 主要セクション構造

1. SWITCHES,MACROS - 設定とマクロ
2. PAGE ZERO - ゼロページ変数
3. RAM CODE - RAM実行コード
4. DISPATCH TABLES - ディスパッチテーブル
5. STORAGE MANAGEMENT - ストレージ管理
6. ERROR HANDLER - エラーハンドラー
7. LIST COMMAND - LISTコマンド
8. FOR STATEMENT - FOR文
9. STATEMENT FETCHER - 文取得
10. BASIC COMMANDS - BASIC各種コマンド
11. FORMULA EVALUATION - 式評価
12. VARIABLE SEARCHING - 変数検索
13. ARITHMETIC ROUTINES - 算術ルーチン
14. STRING FUNCTIONS - 文字列関数
15. FLOATING POINT MATH - 浮動小数点演算
16. TRIGONOMETRIC FUNCTIONS - 三角関数
17. SYSTEM INITIALIZATION - システム初期化

## 4. C言語移植への考慮事項

### データ型マッピング
- 6502の8ビット値 → uint8_t
- 16ビットアドレス → uint16_t
- 浮動小数点 → float/double
- 文字列 → char*

### メモリ管理
- Page Zero変数 → グローバル変数構造体
- 動的メモリ → malloc/free
- ガベージコレクション → 独自実装

### 制御フロー
- JMP/JSR → 関数呼び出し
- 条件分岐 → if文
- ループ → while/for文

