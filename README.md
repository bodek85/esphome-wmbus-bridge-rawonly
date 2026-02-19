# ESPHome wM-Bus Bridge (RAW-only)
RF → HEX → MQTT → wmbusmeters (Home Assistant)

To repo zawiera **lekki komponent do ESPHome**, którego zadanie jest proste: **odebrać telegram wM-Bus z radia i oddać go dalej**.
Dekodowanie (zamiana na wartości/liczniki) robi **wmbusmeters po stronie Home Assistant / Linux**.

## Co to robi / czego nie robi

✅ odbiera telegramy wM-Bus z radia (m.in. **SX1262**, **SX1276** / pliki `transceiver_*.cpp`)  
✅ rozpoznaje tryb link-layer **T1** lub **C1** i składa ramkę do postaci „przyjaznej” do dalszej obróbki  
✅ pozwala opublikować odebrane ramki jako **HEX** na MQTT (przez `on_frame`)  
✅ ma **diagnostykę**: raporty + zdarzenia „dropped” na osobnym topicu MQTT

❌ nie dekoduje liczników na ESP (brak driverów xmq, brak parsowania pól)  
❌ nie tworzy encji HA ani „kombajnu” – to ma być radio+transport

W praktyce: ESP działa jak **gateway RF → MQTT**, a HA robi całą „mądrą” robotę.

---

## Wymagania

- ESPHome (testowane u Ciebie na **2026.2.0** / ESP32‑S3)
- Broker MQTT (np. Mosquitto w HA)
- Radio:
  - **SX1262** (np. Heltec WiFi LoRa 32 V4.x)
  - **SX1276** (moduły/boardy LoRa)

---

## Jak to działa (w skrócie)

1) ESP odbiera pakiet z radia  
2) wykrywa tryb **T1** albo **C1**  
3) próbuje złożyć ramkę; jeśli się uda – odpala `on_frame` (ty decydujesz co dalej)  
4) `wmbusmeters` na HA subskrybuje MQTT i dekoduje telegramy

W logach widzisz np. „Have data … mode: T1 A”.【1414:3†logs_heltec_run.txt†L1-L6】【1414:8†logs_heltec_run.txt†L11-L22】

---

## Instalacja (external_components)

W konfiguracji ESPHome dodaj:

```yaml
external_components:
  - source: github://Kustonium/esphome-wmbus-bridge-rawonly@main
    components: [wmbus_radio]
    refresh: 0s
```
【1414:1†48159577-a515-4eac-92f2-4291a96ece3b.md†L50-L60】

---

## Przykładowa konfiguracja (SX1262 / Heltec V4)

Poniżej sensowny „szkielet” (u Ciebie działa na takich pinach). Kluczowe rzeczy:
- `rx_gain` (BOOSTED / POWER_SAVING) – tryb czułości SX1262
- `dio2_rf_switch`/`has_tcxo` – zależnie od płytki
- `diagnostic_topic` – gdzie leci diagnostyka
- `on_frame` – tu publikujesz HEX na MQTT

```yaml
wmbus_radio:
  id: wmbus_radio_1
  radio_type: SX1262

  cs_pin: GPIO8
  reset_pin: GPIO12
  busy_pin: GPIO13
  irq_pin:
    number: GPIO14
    mode: input

  # diagnostyka (patrz sekcja niżej)
  diagnostic_topic: "wmbus/diag/heltec"

  # SX1262 tuning
  dio2_rf_switch: true
  rx_gain: BOOSTED        # albo POWER_SAVING
  has_tcxo: true

  on_frame:
    then:
      - mqtt.publish:
          topic: "wmbus_bridge/telegram"
          payload: !lambda |-
            return frame->as_hex();
```

Uwaga: w YAML możesz wpisać `rx_gain: boosted`, ale pod spodem i tak mapuje się to na enum
`BOOSTED/POWER_SAVING` (ESPHome robi `upper=True`).【1414:16†__init__.py†L19-L24】

---

## Diagnostyka (MQTT)

Komponent potrafi publikować JSON‑y na `diagnostic_topic`:

### 1) Summary (co X sekund)
Przykład:
```json
{"event":"summary","truncated":0,"dropped":7,"dropped_by_reason":{"too_short":0,"decode_failed":7,"dll_crc_strip_failed":0,"unknown_preamble":0,"l_field_invalid":0,"unknown_link_mode":0,"other":0}}
```
W logach zobaczysz też info, że poszedł summary:  
„DIAG summary published to wmbus/diag/heltec …”【1414:3†logs_heltec_run.txt†L10-L12】【1414:5†logs_heltec_run.txt†L1-L3】

### 2) Dropped (gdy pakiet nie przeszedł składania/CRC)
Przykład z Twoich logów:
- `reason=decode_failed` w trybie **T1**【1414:8†logs_heltec_run.txt†L18-L20】
- `reason=dll_crc_strip_failed` w trybie **C1**【1414:7†logs_heltec_run.txt†L2-L4】

Dla `dropped` (jeśli włączysz publikację raw) wpadnie też:
- RSSI
- `raw_got` (ile bajtów przyszło z radia)
- `raw` (HEX)

To jest po to, żebyś mógł:
- ocenić jakość RF (RSSI, częstotliwość dropów),
- zebrać przykładowe ramki do analizy,
- odsiać „śmieci”/zakłócenia od realnych telegramów.

**Ważne:** `dropped` to nie jest „telegram od licznika, który na pewno ma sens”.
To jest „coś odebrane radiowo, ale nie przeszło weryfikacji” – często zakłócenia albo uszkodzona ramka.

---

## T1 / C1 / T2 – jak jest w tym repo

- W logach i w kodzie konfiguracji repo działa i raportuje **T1** oraz **C1**.【1414:3†logs_heltec_run.txt†L1-L6】【1414:7†logs_heltec_run.txt†L2-L4】
- W dostarczonych materiałach nie widać obsługi/rozpoznawania **T2** (brak takich logów, brak odniesień w plikach konfiguracyjnych).

Jeśli kiedyś trafisz na realny T2 (i pokażesz surowy HEX), wtedy ma sens dopinać wykrywanie/obsługę.

---

## FAQ

### Czy to „dekoduje licznik” na ESP?
Nie. ESP tylko odbiera i przekazuje dalej. Dekoduje **wmbusmeters** na HA.【1414:1†48159577-a515-4eac-92f2-4291a96ece3b.md†L69-L75】

### Czy mogę usunąć katalog `wmbus_common`?
Jeśli ESPHome nie kompiluje go (bo nie jest już podpinany przez loader/manifest), to samo istnienie katalogu w repo nie szkodzi.
Realnie liczy się to, co jest importowane przez komponent (`__init__.py` + pliki źródłowe, które loader bierze).【1414:16†__init__.py†L3-L7】【1414:16†__init__.py†L9-L15】

---

## Atrybucja i licencja

- inspiracja/baza: `SzczepanLeon/esphome-components`
- dekoder po stronie HA: `wmbusmeters/wmbusmeters`

GPL-3.0-or-later – patrz `LICENSE` i `NOTICE`.
