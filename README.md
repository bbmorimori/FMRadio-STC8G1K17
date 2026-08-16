<meta name="google-site-verification" content="HcKcbeprjq9c-W4Y2lgaPtA0RZWGuCdxvmCY7M7M7wU" />

# STC8G1K17 FM Radio Firmware  
RDA5807M / 7-Segment LED Display

STC8G1K17 と RDA5807M を使用した **完全自作 FM ラジオファームウェア**です。  
AliExpress で販売されている FM ラジオ基板に対応し、  
周波数表示・音量表示・プリセット・自動選局・EEPROM 保存などを実装しています。

MIT ライセンスで公開しています。

> ⚠️ 注意  
> EEPROM（IAP）は書き換え回数に上限があります。  
> 本ファームウェアでは書き換え最適化を行っていないため、  
> 長期間の使用で IAP 領域が劣化する可能性があります。

---

## ✨ 主な機能

- 76.0MHz〜108.0MHz（日本ワイド FM バンド）
- 周波数の直接選局（100kHzステップ）
- SEEK（自動選局）
- SOFT SEEK（RSSI による簡易スキャン）
- プリセット登録（最大 10ch）
- EEPROM（IAP）への設定保存
- 音量調整（0〜15）
- スリープ（WKT で 500ms ごとに復帰）
- 7セグメント LED ×4 の多重化表示
- ボタン短押し / 長押し / リピート対応

---

## 🧩 対応ハードウェア

**STC8G1K17 + RDA5807M FM Radio Board**  
https://ja.aliexpress.com/item/1005010363871111.html
<img width="512" height="512" alt="pic1" src="https://github.com/user-attachments/assets/fd1a84fc-4354-43d7-917a-e0cfb246e4f9" />
### 基板構成

| 機能 | パーツ |
|------|--------|
| MCU | STC8G1K17 16pin　18.432MHz|
| FM チューナー | RDA5807M |
| 表示 | 7セグメント LED ×4 |
| ボタン | 4個（周波数UP/DOWN、音量UP/DOWN） |
| I2C | P5.4(SDA), P5.5(SCL) |
| 桁選択 | P1.0 / P1.1 / P1.6 / P1.7 |
|　SEG | P3.0〜P3.7 |
| 電源 | 5V（基板上で 3.3V に降圧） |

---

## 📡 ピンアサイン（基板と完全一致）

### 7セグメント桁選択（P1）

```
P1.0 → Digit1  
P1.1 → Digit2  
P1.6 → Digit3  
P1.7 → Digit4  
```

### 7セグメント LED セグメント（P3）

```
P3.0 → A  
P3.1 → B  
P3.2 → C / SW2  
P3.3 → D / SW1  
P3.4 → E / SW4  
P3.5 → F / SW3  
P3.6 → G  
P3.7 → DP  
```

> ※ セグメント端子とボタン端子が共用されているため、  
>   多重化タイミングでボタン入力をサンプリングしています。

### I2C（P5）

```
P5.4 → SDA  
P5.5 → SCL  
```

### 電源

```
VCC → 5V（基板内で 3.3V に変換）
GND → GND
```

---

## 🛠 開発環境（Development Environment）

本プロジェクトは以下の環境で開発されています。

### 使用ツール

- **SDCC 4.x**（メインコンパイラ）
- **FwLib_STC8**（STC8 系向け軽量ライブラリ）
- **PlatformIO**（SDCC 統合・ビルド管理）
- **STC-ISP**（UART ブートローダ書き込み）
- **USB-UART（CH340 / CP2102）**

---

# 🎮 操作方法（UI の説明）

本ラジオは **4つのボタン**で操作します。  
すべて **短押し / 長押し / リピート**に対応しています。

### 🔘 ボタン割り当て

| ボタン | 役割 |
|--------|------|
| SW1（P3.3） | 周波数 DOWN |
| SW2（P3.2） | 周波数 UP |
| SW3（P3.5） | 音量 DOWN |
| SW4（P3.4） | 音量 UP |

---

## 📻 周波数操作

### ● 短押し  
- **UP**：+0.1MHz  
- **DOWN**：−0.1MHz  

### ● 長押し  
- 高速ステップ（0.1MHz × リピート）

### ● SEEK（自動選局）
- **UP 長押し 1秒以上**：上方向へ SEEK  
- **DOWN 長押し 1秒以上**：下方向へ SEEK  

---

## 🔊 音量操作

### ● 短押し  
- **UP**：音量 +1  
- **DOWN**：音量 −1  

### ● 長押し  
- **DOWN**：ミュート  
- **UP**：高速音量調整（リピート）

---

## ⭐ プリセット（最大 20ch）

### ● プリセット呼び出し  
- **UP + DOWN を同時押し（短押し）**  
  → プリセットモードへ  
  → UP/DOWN でプリセット番号選択  
  → 数秒後に自動確定

### ● プリセット登録  
- 任意の周波数で **UP + DOWN を 2秒長押し**  
  → EEPROM に保存

---

## 📂 ディレクトリ構成

```
FMRadio-STC8G1K17/
├── src/
│   ├── main.c
│   ├── rda5807.c
│   ├── display.c
│   ├── buttons.c
│   ├── eeprom.c
│   └── util.c
├── include/
│   ├── stc8.h
│   ├── rda5807.h
│   ├── display.h
│   └── eeprom.h
├── build/
│   ├── firmware.hex
│   ├── firmware.map
│   └── firmware.mem
├── platformio.ini
└── README.md
```

---

## 📜 ライセンス

MIT License

```
