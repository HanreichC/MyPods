<div align="center">

# MyPods

### AirPods-Magie. Jetzt auch auf Linux.

Deckel auf. Akku da. Musik läuft.<br>
Das Gefühl von AirPods am Mac, für Linux und Windows. Für AirPods, Beats und fast jeden anderen Kopfhörer.

<br>

[![Lizenz: GPL-3.0](https://img.shields.io/badge/Lizenz-GPL--3.0-blue.svg)](LICENSE)
![Plattform: Linux | Windows](https://img.shields.io/badge/Plattform-Linux%20%7C%20Windows-lightgrey.svg)
![Qt 6.9+](https://img.shields.io/badge/Qt-6.9%2B-41cd52.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599c.svg)

<br>

![MyPods mit AirPods Pro](docs/images/hero.png)

</div>

> [!NOTE]
> MyPods ist ein persönliches Projekt in aktiver Entwicklung und läuft unter CachyOS (Arch-basiert).
> Getestet ist es mit **AirPods Max** und **Parrot Zik 2.0**. Die Unterstützung für AirPods Pro 3 ist
> aus der Protokollforschung entstanden, aber **noch nicht an echter Hardware getestet**, genauso wenig
> wie die übrigen Modelle weiter unten. Funktionen mit dem Vermerk *ungeprüft* gehören in dieselbe Kategorie.

> [!WARNING]
> **Dieses Projekt ist vibe-coded.** Große Teile des neuen Codes hat ein KI-Assistent geschrieben, von Hand
> durchgesehen, auf der bestehenden MagicPods-Codebasis. Rechne mit Ecken und Kanten und lies den Code,
> bevor du dich darauf verlässt.

<br>

**Inhalt:** [Funktionen](#deckel-auf-da-sind-sie) · [Geräte](#nicht-nur-airpods) · [So funktioniert's](#so-funktionierts) · [Technische Daten](#technische-daten) · [Installation](#herunterladen-installieren-fertig) · [Windows](#auch-unter-windows) · [Einrichtung](#einmal-einrichten-nahtlos-wechseln) · [Bedienung](#so-bedienst-du-mypods) · [Konfiguration](#feinschliff-in-der-configtoml) · [Selbst bauen](#selbst-bauen) · [Tests](#geprüft-immer-wieder) · [API](#für-entwickler-die-api) · [Grenzen](#was-noch-nicht-geht) · [Hilfe](#wenns-hakt) · [Sicherheit](#sicherheit-und-datenschutz) · [Herkunft](#herkunft-und-lizenz)

<br>

## Deckel auf. Da sind sie.

<div align="center">

![Das Popup beim Öffnen des Case](docs/images/popup.png)

</div>

Öffne das Case neben deinem Computer, und das vertraute Popup erscheint, mit Animation und dem Akku von links, rechts und Case. Genau wie am Mac. MyPods liest dafür die BLE-Advertisements, die deine AirPods ständig senden. Deshalb ist das Popup sofort da, noch bevor die AirPods überhaupt verbunden sind. Ein Klick auf **Verbinden**, und der Ton landet in deinen Ohren.

<br>

## Jedes Prozent zählt.

Der Akku kommt aufs Prozent genau, für links, rechts und das Case, samt Ladezustand. MyPods entschlüsselt ihn aus dem Advertisement und liest ihn zusätzlich über AAP. Das Tray-Symbol zeigt ihn als Ring, rot, wenn es knapp wird. Zeig mit der Maus darauf, und MyPods verrät dir, wie lange der Akku ungefähr noch hält, sobald es genug Werte gesammelt hat.

Fällt ein AirPod oder die Max auf 10 %, meldet sich eine **Benachrichtigung**. Sie kommt erst wieder, nachdem geladen wurde oder der Stand über 20 % liegt. Und weil MyPods den genauen Wert unter Linux an BlueZ weitergibt, zeigen ihn auch das Bluetooth-Applet deines Desktops und UPower.

<br>

## Ruhe. Oder die Welt.

**Aus**, **Transparenz**, **Adaptiv** und **Geräuschunterdrückung**: dieselben Modi wie am Mac, dazu der Regler für den adaptiven Modus von 0 bis 100. Du legst fest, welche Modi Drücken und Halten durchschaltet und ob **Aus** dazugehört. **Geräuschunterdrückung mit einem AirPod** gibt es auch.

Bei AirPods Pro 2 und 3 kommen **Reduzierung lauter Geräusche** und die **angepasste Transparenz** dazu: Verstärkung, Balance, Klang, Umgebungsgeräuschreduzierung und Gesprächsverstärkung, über den ATT-Kanal der AirPods. *Ungeprüft an Hardware.*

**Konversationserkennung** senkt die Lautstärke, sobald du sprichst, standardmäßig auf 30 % ihres Pegels, einstellbar, und holt sie danach zurück. Ein Schalter, und der aktuelle Zustand ist immer zu sehen.

<br>

## Vom iPhone zum Laptop. Und zurück.

Starte am Computer einen Player, ob Browser, Spotify oder irgendetwas mit MPRIS, und MyPods holt die AirPods von deinem iPhone: verbindet sie bei Bedarf, führt Apples Smart-Routing-Übergabe aus, schaltet A2DP ein und macht sie zur Standardausgabe. Spielt das iPhone wieder, pausiert der Computer und wechselt zurück auf seine Lautsprecher. Während eines Anrufs auf dem anderen Gerät übernimmt er nie. Genau wie **„Mit diesem Mac verbinden: Automatisch“**.

Trägst du die AirPods, während ein anderes Gerät sie hat, bleiben die Lautsprecher still: Was hier spielt, geht auf eine stumme Ausgabe, bis die AirPods es abspielen. Nimm sie heraus oder wähle eine Ausgabe, und die Lautsprecher sind zurück. Das Banner auf dem iPhone nennt deinen Computer beim Bluetooth-Namen.

Wie am Mac zeigt die Geräteseite **„Wiedergabe auf iPhone – Hierher holen“**. Lieber von Hand? **„Bei letzter Verbindung mit diesem Computer“** gibt es auch, dann wechselst du nur selbst.

Dafür muss sich BlueZ einmal als Apple-Gerät melden, siehe [Einrichtung](#einmal-einrichten-nahtlos-wechseln).

<br>

## Nimm sie raus. Die Musik wartet.

Nimmst du einen AirPod heraus, pausiert die Wiedergabe. Setzt du ihn wieder ein, geht es weiter, über MPRIS. Der Schalter dafür liegt in den AirPods selbst. **Pausieren beim Einschlafen** und die Richtung der **Digital Crown** (AirPods Max) gibt es für die AirPods, die sie melden.

<br>

## Deine AirPods. Ganz nach dir.

Was du am Mac in den Einstellungen deiner AirPods findest, schreibt MyPods direkt in die AirPods, für die Modelle, die es melden: **Drückgeschwindigkeit**, **Dauer für Drücken und Halten**, **Lautstärke durch Streichen** samt Streichlänge, **Tonlautstärke**, **personalisierte Lautstärke**, **Stummschaltung und Anruf beenden** und das **Mikrofon**: automatisch, immer links oder immer rechts.

Gib deinen AirPods einen neuen **Namen**. Er wird in den AirPods gespeichert, also sieht ihn jedes gekoppelte Gerät. Modellnummer, Seriennummer und Firmware stehen im Abschnitt **Gerät**, wie in der Info-Ansicht am Mac.

Mehrere Kopfhörer verbunden? Die zuletzt verbundenen sind die aktiven. Ein Klick in der Seitenleiste wählt andere, und Tray und Popup ziehen mit.

<br>

## Klingt nach dir.

<div align="center">

![Equalizer, Klangfarbe und 3D-Audio](docs/images/sound.png)

</div>

Die Presets aus Apple Music sind da: Akustik, Bass Booster, Klassik, Rock, Vocal Booster und mehr, sauber vor die AirPods gelegt, ohne Übersteuern. Oder du stellst zehn Bänder selbst ein (**Manuell**). Die Kurve zeigt dir, was mit deinem Klang passiert.

- **Klangfarbe:** ein Regler macht alles wärmer oder heller, Bass runter und Höhen rauf (oder umgekehrt), um bis zu 6 dB, 1 kHz bleibt.
- **Kopfhörerkorrektur:** gemessene [AutoEQ](https://github.com/jaakkopasanen/AutoEq)-Filter bringen das Modell auf das Harman-Ziel, für AirPods 1–4, Pro, Pro 2, Max und die meisten Beats (noch nicht Pro 3 oder Max 2, die hat noch niemand gemessen).
- **Deine eigene Korrektur:** eine `ParametricEQ.txt` von AutoEQ oder Equalizer APO (Einstellung `eqFile`) ersetzt die eingebaute, für deine eigene Messung, ein anderes Ziel oder Modelle, die noch niemand gemessen hat.
- **Crossfeed** (im Stil von bs2b) für Stereomusik ohne 3D-Audio: Jedes Ohr hört ein wenig vom Bass des anderen Kanals, wie bei Lautsprechern. Mono bleibt unberührt.
- **Gehörrichtige Lautstärke** (ISO 226): Je leiser du hörst, desto mehr Bass kommt zurück, live mit der Lautstärke.
- **Hörprofil:** ein Audiogramm pro Ohr (sechs Schwellen von 250 Hz bis 8 kHz) wird zu einem eigenen EQ für jedes Ohr (Half-Gain-Regel, höchstens 20 dB). Wie Apples Kopfhöreranpassungen.
- **Ohne Effekte vergleichen (A/B):** schaltet jeden Effekt ab, behält aber die Vorverstärkung, damit beide Seiten gleich laut spielen und lauter nie als besser durchgeht.
- **Limiter mit Vorausschau** am Ende der Kette (mit `swh-plugins`): 3D-Audio spielt damit 7 dB lauter, und Kopfdrehungen über 45° übersteuern nicht mehr (gemessen: −1,0 dBFS Spitze, wo die Kette ohne ihn +0,9 dBFS erreichte).
- **Hörgerät:** derselbe Schalter wie am iPhone, sobald dort der Hörtest eingerichtet ist.

<br>

## Klang, der dich umgibt.

**3D-Audio**: Aus, Fest oder **Kopfbewegung folgen**. Eine PipeWire-Filterkette mit HRTF (libmysofa) setzt den Klang in den Raum, so entzerrt, dass zentrierter Klang neutral bleibt (innerhalb von 0,75 dB von 31 Hz bis 8 kHz, gemessen). Die Kopfhaltung kommt direkt aus den AirPods.

Für Filme gibt es **Surround (7.1)**: Die Kette nimmt 5.1 und 7.1 an und stellt sieben virtuelle Lautsprecher auf (ITU-Anordnung, Seiten bei 90°, hinten bei 135°), alle mit Kopfverfolgung; der LFE geht auf beide Ohren. Stereo-Player mischt PipeWire dann hoch. So wie Apples 3D-Audio für Dolby Atmos.

<br>

## Ehrlich bis zum letzten Bit.

Lossless und Hi-Res aus Apple Music? Über Bluetooth gibt es das nicht: AirPods sprechen nur AAC, und das ist verlustbehaftet. Was MyPods stattdessen hat:

- **Signalweg** auf der Geräteseite sagt dir, ob das, was spielt, bitgenau ankommt, und wenn nicht, warum.
- **Hi-Res-Wiedergabe** lässt Kabelkopfhörer und USB-DACs mit der Rate der Quelle spielen, bis 192 kHz.
- **Höchste Bluetooth-Qualität** stellt AAC auf seine beste variable Bitrate und LDAC auf 990 kbit/s (Details unter [Bluetooth-Klangqualität](#bluetooth-klangqualität)).
- Die **Codec-Anzeige** verrät, welcher Codec gerade läuft.

<br>

## Ein Klick. Und es spielt dort.

Klick links in der Seitenleiste auf deine Kopfhörer, und sie sind die, die du steuerst, und der Ton spielt dort. Ein kleines Lautsprecher-Symbol zeigt dir, wo gerade gespielt wird. Wechselst du die Ausgabe woanders, in der Taskleiste oder in den Toneinstellungen deines Desktops, zieht MyPods mit.

Im Tray-Popup steht unter **Ausgabe** jede Ausgabe, Lautsprecher, Monitor und Kopfhörer, die aktive mit Haken. Ein Klick, und es spielt dort. Kopfhörer bringen dabei ihre Effekte mit. Kopfhörer an der Buchse eines Monitors markierst du einmal unter *Einstellungen → Kopfhörer an diesen Ausgängen*, denn Monitore verraten niemandem, ob etwas eingesteckt ist. Danach sind sie Kopfhörer wie alle anderen, mit eigenem Equalizer.

<br>

## Alles in der Menüleiste.

<div align="center">

![Das Tray-Popup](docs/images/tray.png)

</div>

Ein Klick auf das Tray-Symbol, und direkt unter der Leiste öffnet sich das Popup, wie das AirPods-Modul im Kontrollzentrum: Akku, Geräuschkontrolle, Konversationserkennung, **Hierher holen**, was gerade läuft mit Play, Pause und Skip, die Lautstärke und die Ausgabe. Ein Doppelklick öffnet das ganze Fenster.

<br>

## Dein iPhone spielt mit.

Hörst du mit dem iPhone über seine eigenen Kopfhörer, zeigt und steuert das Tray-Popup, was dort läuft: Titel, Interpret, Cover, Play, Pause und Skip, den Stern aus Apple Music (Favorit) und die Lautstärke des iPhones in seinen 16 Stufen, markiert mit **„Wiedergabe auf <iPhone>“**. Das läuft über den Apple Media Service (BLE), genau wie bei einer Apple Watch. Das Cover kommt aus der iTunes Search API, die dafür Interpret und Titel bekommt. Die Bluetooth-Seite listet deine gekoppelten iPhones und ob sie verbunden sind.

<br>

## Hell. Oder dunkel.

<div align="center">

![MyPods im dunklen Erscheinungsbild](docs/images/dark.png)

</div>

MyPods ist aus Glas, wie Liquid Glass: Seitenleiste, Popups und Menüs schweben über dem Inhalt. Hell, dunkel oder automatisch, wie dein System. Das Tray-Symbol passt sich eigens an. Neben jeder Einstellung steht ein (i), das erklärt, was sie tut. Und MyPods spricht Deutsch und Englisch (Qt Linguist).

Außerdem dabei: Autostart, ein Steam-Deck- und gamescope-Modus aus MagicPods, und unter *Über* eine **Diagnose** mit den Protokollen, dem Zustand von Equalizer APO (Windows) und deinem iPhone.

<br>

## Deine Tasten. Deine Regeln.

`mypods --action <name>` ändert die verbundenen Kopfhörer über den Daemon und beendet sich, ohne ein Fenster zu öffnen. Leg es in den Kurzbefehl-Einstellungen deines Desktops auf eine Taste (KDE: *Systemeinstellungen → Tastatur → Kurzbefehle → Neu hinzufügen → Befehl*, GNOME: *Einstellungen → Tastatur → Eigene Tastenkombinationen*); das klappt unter X11 und Wayland gleichermaßen.

| Aktion | Macht |
|--------|-------|
| `noise-next` | Nächster Modus der Geräuschkontrolle, den die Kopfhörer bieten |
| `noise-off`, `noise-anc`, `noise-transparency`, `noise-adaptive` | Genau dieser Modus |
| `conversation-awareness` | Konversationserkennung an/aus |
| `move-here` | AirPods vom iPhone holen („Hierher holen“) |
| `eq-next` | Nächstes Equalizer-Preset |
| `effects-bypass` | Mit und ohne Effekte vergleichen (A/B) an/aus |
| `output-next` | Auf der nächsten Ausgabe spielen (Lautsprecher, Monitor, Kopfhörer), mit oder ohne verbundene Kopfhörer |

Läuft der Daemon nicht oder können die Kopfhörer es nicht, endet der Befehl mit Status 1 und einer Meldung. Unter Windows bringt MyPods eigene Kürzel mit, siehe [Auch unter Windows](#auch-unter-windows).

<br>

## Nicht nur AirPods.

### Parrot Zik 2.0

Gesteuert über die eigene RFCOMM-XML-API des Kopfhörers. Einfach koppeln; MyPods erkennt ihn an seiner Service-UUID.

- Akkustand und Ladezustand
- Geräuschkontrolle: ANC und Straßenmodus, jeweils normal oder maximal. **Aus** gibt es bewusst nicht, denn das schaltet den Zik 2 stumm.
- Kopferkennung (automatische Pause)
- Concert-Hall-Effekt mit Raumgröße (still, Wohnzimmer, Jazzclub, Konzertsaal) und Winkel (30°–180°)
- Equalizer auf dem DSP des Kopfhörers, mit denselben Presets wie bei den AirPods oder deinen eigenen 5 Bändern (**Eigene**); er bleibt im Kopfhörer
- Smart Audio Tune, ANC bei Anrufen, Sprachansagen, automatische Verbindung, automatisches Ausschalten

### Samsung Galaxy Buds

Akku und Geräuschkontrolle, geerbt von MagicPodsCore (Galaxy Buds, Buds+, Live, Pro, Buds2, Buds2 Pro, FE, Core, Buds3, Buds3 Pro, Buds3 FE, Buds4, Buds4 Pro), dazu Ohrerkennung: Bud heraus pausiert, wieder hinein spielt weiter (abschaltbar, lokal gespeichert; nicht bei den Buds Core).

### Beoplay E8 (1. Generation)

Unter Windows liest MyPods über Bluetooth LE die Geräteinformationen und den Zustand der Transparenz. Was der E8 verrät, steht in [docs/beoplay-e8.md](docs/beoplay-e8.md).

### Jedes andere Headset

Jedes gekoppelte Gerät mit Freisprechprofil (oder nur A2DP) zeigt seinen Akku, sofern es ihn über HFP meldet, und den aktiven Bluetooth-Codec. Und es bekommt die Effekte, die auf diesem Computer laufen: Equalizer, Klangfarbe, Crossfeed, gehörrichtige Lautstärke, Hörprofil, eine Kopfhörerkorrektur aus deiner eigenen `ParametricEQ.txt` (`eqFile`) und unter Linux 3D-Audio ohne Kopfverfolgung und den Signalweg. Genauso Kabelkopfhörer an Buchse oder USB und Ausgänge, die du als Kopfhörer markierst (die Buchse eines Monitors).

### Unterstützte Geräte

| Familie | Modelle | Stand |
|---------|---------|-------|
| AirPods Max | Max (`0x200A`), Max USB-C (`0x201F`), Max 2 (`0x202D`) | **Getestet** (AirPods Max). Der Popup-Auslöser der Max 2 ist *ungeprüft*. |
| AirPods Pro | AirPods Pro 3 (A3063 / A3064 / A3065, Modell-ID `0x2027`) | Umgesetzt, *nicht an Hardware getestet* |
| Andere AirPods | AirPods 1–4, AirPods 4 ANC, AirPods Pro, Pro 2, Pro 2 USB-C | Vom geerbten AAP-Stack erkannt, von diesem Projekt nicht getestet |
| Beats | Powerbeats Pro / Pro 2 / 3 / 4 / Fit, Beats Fit Pro, Studio Buds / Buds+, Studio Pro, Studio 3, Solo 3 / Pro / 4 / Buds, Flex, BeatsX | Vom geerbten AAP-Stack erkannt, von diesem Projekt nicht getestet |
| Parrot | Zik 2.0 | **Getestet** |
| Samsung | Galaxy-Buds-Reihe (siehe oben) | Von MagicPodsCore geerbt, von diesem Projekt nicht getestet |
| Bang & Olufsen | Beoplay E8 (1. Generation) | Geräteinfos und Transparenz-Zustand, nur Windows |
| Allgemein | Jedes Freisprech- (HFP) oder A2DP-Headset, Kabelkopfhörer | Akku (wenn gemeldet), Codec, Effekte auf diesem Computer |

Linux bekommt jede Funktion. Windows bekommt den Teil, der ohne Kerneltreiber geht, siehe [Auch unter Windows](#auch-unter-windows).

<br>

## So funktioniert's.

Apple-Kopfhörer sprechen über zwei getrennte Kanäle mit ihrem Gerät, und die meisten Tools nutzen nur einen davon. MyPods nutzt beide.

```
                 ┌──────────────────────── AirPods ────────────────────────┐
                 │                                                         │
   BLE-Advertisements (Continuity,                     L2CAP, PSM 0x1001
   „Proximity Pairing“, Typ 0x07)                      (AAP / „AACP“)
   ohne Verbindung                                     braucht eine klassische Verbindung
                 │                                                         │
                 ▼                                                         ▼
 ┌───────────────────────────────── mypodscore (Daemon) ─────────────────────────────────┐
 │  Deckelzustand, grober Akku,      │  genauer Akku, ANC, Ohrerkennung, Einstellungen,    │
 │  AES-entschlüsselter genauer      │  IRK/ENC-Schlüsseltausch, Smart Routing (Übergabe), │
 │  Akku, RPA-Prüfung → Popup        │  Kopfverfolgung                                     │
 │                                                                                          │
 │  BlueZ (D-Bus) · PulseAudio/PipeWire · MPRIS (D-Bus) · PipeWire-Filterkette (EQ, HRTF)   │
 │  Einstellungen in ~/.config/mypods/config.toml                                           │
 └───────────────────────────────────────┬──────────────────────────────────────────────────┘
                                         │ WebSocket, JSON, Port 2020
                                         ▼
                        ┌──────── mypods (Qt 6 / QML) ────────┐
                        │ Tray-Symbol, Geräteseiten,             │
                        │ Einstellungen, Deckel-Popup            │
                        │ startet den Daemon beim Start          │
                        └────────────────────────────────────────┘
```

1. **BLE-Advertisements.** AirPods senden ständig Apple-Herstellerdaten (`0x004C`). Die Proximity-Pairing-Nachricht enthält das Modell, den Deckelzähler, die Bits für im Ohr und im Case, grobe Akku-Nibbles und einen 16 Byte großen, mit AES-128 verschlüsselten Block mit dem genauen Akku. Deshalb erscheint das Popup sofort, noch bevor die AirPods verbunden sind. Unter Linux pausiert der Scan, solange AirPods verbunden sind: AAP liefert dann dasselbe, und die klassische Inquiry des Scans kann A2DP stottern lassen.
2. **AAP über L2CAP.** Sobald verbunden, öffnet der Daemon einen L2CAP-Socket auf PSM `0x1001`, schickt den Handshake und abonniert die Benachrichtigungen. Ab dann schicken die AirPods Akku, Geräuschkontrolle, Ohrerkennung und mehr von selbst und nehmen Einstellungen an. Schließen die AirPods den Kanal, während die Bluetooth-Verbindung bleibt, öffnet der Daemon ihn wieder (bis zu fünfmal pro Verbindung). AirPods Pro 2 und 3 führen ein paar Einstellungen (Reduzierung lauter Geräusche, angepasste Transparenz) als GATT-Characteristics, die der Daemon über einen zweiten L2CAP-Kanal liest und schreibt (ATT, PSM `0x1F`).
3. **Schlüssel.** Um *deine* AirPods unter wechselnden BLE-Adressen (RPA) zu erkennen und den Akkublock zu entschlüsseln, braucht MyPods den IRK und den ENC-Schlüssel. Es fragt die AirPods bei jeder Verbindung über AAP danach und speichert sie, wenn sie sich ändern. Ganz ohne iCloud.
4. **Umschalten.** Die AirPods reichen Apples Smart-Routing-Nachrichten zwischen ihren Quellen weiter. MyPods nimmt an diesem Austausch teil und beobachtet MPRIS-Player, um zu entscheiden, wann es übernimmt.
5. **Effekte.** Equalizer, Kopfhörerkorrektur, Crossfeed, gehörrichtige Lautstärke, Hörprofil und 3D-Audio laufen als PipeWire-Filterketten-Sink (`mypods_fx`) vor den Kopfhörern: eine Vorverstärkung pro Eingangskanal, dann 3D-Audio oder Crossfeed, dann die Filter pro Ohr, dann der Limiter. Die Vorverstärkung nimmt genau so viel weg, wie die Kette anheben kann (aus den summierten Filterkurven berechnet), so übersteuert nichts. Kopfverfolgung und Lautstärke (für die gehörrichtige Lautstärke) ändern die laufende Kette über `pw-cli`. Die Kette läuft mit der Rate des Graphen, mit Hi-Res-Wiedergabe also mit der der Quelle. Startet PipeWire neu, verbindet sich der Daemon wieder und setzt eine neue Kette vor die Kopfhörer, die dann spielen.
6. **Was auf dem iPhone läuft.** Die Oberfläche spricht direkt mit einem gekoppelten iPhone, über den Apple Media Service (AMS), den BLE-GATT-Dienst, den eine Apple Watch für dasselbe nutzt. Das iPhone schickt Titel, Interpret, Wiedergabezustand und Lautstärke und nimmt Play/Pause, Skip und Lautstärkeschritte an. Unter Linux meldet die Oberfläche bei BlueZ ein LE-Advertisement an, das nach AMS fragt, und das iPhone verbindet sich damit; unter Windows verbindet sich die Oberfläche selbst mit dem iPhone. Der Daemon ist daran nicht beteiligt.

Eine ausführliche Herleitung der Protokolle und Entscheidungen steht in [docs/PLAN.md](docs/PLAN.md).

<br>

## Technische Daten

| | |
|---|---|
| **Plattform** | Linux (entwickelt und getestet unter CachyOS mit KDE Plasma) und Windows (MSI, ohne Kerneltreiber) |
| **Oberfläche** | Qt 6 / QML im Glas-Design, hell, dunkel oder automatisch, Deutsch oder Englisch |
| **AirPods** | Deckel-Popup, Akku aufs Prozent, Geräuschkontrolle, Ohrerkennung, Konversationserkennung, automatisches Umschalten mit dem iPhone, alle Einstellungen, die die AirPods melden |
| **Klang** | Apple-Music-Presets, 10-Band-EQ, Klangfarbe, AutoEQ-Korrektur, Crossfeed, gehörrichtige Lautstärke, Hörprofil, A/B, Limiter |
| **3D-Audio** | Fest oder mit Kopfverfolgung, Surround 7.1, HRTF über libmysofa (Linux) |
| **Ausgabe** | Wählbar in Seitenleiste und Tray, Signalweg, Hi-Res bis 192 kHz (Linux), AAC-VBR und LDAC 990 kbit/s |
| **iPhone** | Was läuft, Cover, Steuerung und Lautstärke über den Apple Media Service |
| **Geräte** | AirPods, Beats, Parrot Zik 2.0, Galaxy Buds, Beoplay E8, jedes HFP-/A2DP-Headset, Kabelkopfhörer |
| **Schnittstelle** | WebSocket-JSON-API auf `127.0.0.1:2020` |
| **Kern** | Daemon und Oberfläche auf Basis von MagicPodsCore und MagicPodsLinux, C++20 |
| **Lizenz** | GPL-3.0 |

<br>

## Herunterladen. Installieren. Fertig.

### AppImage (jede Distribution)

Lade `MyPods-x86_64.AppImage` aus dem [neuesten Release](https://github.com/HanreichC/MyPods/releases/latest) und starte es mit `chmod +x MyPods-x86_64.AppImage && ./MyPods-x86_64.AppImage`. Qt steckt mit drin; BlueZ, PipeWire und libmysofa kommen weiter vom System. Jeder Push auf `main` ist ein Release (`vX.Y.N`, die Version, die die App zeigt).
Selbst bauen: `podman run --rm -v "$PWD:/workspace:Z" -w /workspace docker.io/library/ubuntu:22.04 ./appimage.sh`

### Arch-Paket

[packaging/arch/PKGBUILD](packaging/arch/PKGBUILD) baut ein Paket `mypods-git` aus dem neuesten Commit auf GitHub (wie im AUR), nicht aus deinem lokalen Stand. Es installiert nach `/usr/lib/mypods`, mit `mypods` und `mypodscore` in `/usr/bin`, einem Startmenü-Eintrag und dem systemd-User-Dienst des Daemons.

```bash
cd packaging/arch && makepkg -si
systemctl --user enable --now mypods-core.service   # einmal pro Benutzer

# deinstallieren
systemctl --user disable --now mypods-core.service
sudo pacman -R mypods-git
```

### Aus dem Quellcode (Arch-basiert)

```bash
git clone https://github.com/HanreichC/MyPods.git
cd MyPods
./install.sh
```

`install.sh` baut MyPods im Podman-Container und installiert es für den aktuellen Benutzer. Kein root nötig.

| Was | Wo |
|-----|----|
| Oberfläche | `~/.local/opt/mypods/mypods` |
| Daemon | `~/.local/opt/mypods/modules/mypodscore` |
| Startmenü-Eintrag | `~/.local/share/applications/app.mypods.desktop` |
| Autostart (startet versteckt im Tray) | `~/.config/autostart/app.mypods.desktop` |
| Daemon als systemd-User-Dienst | `~/.config/systemd/user/mypods-core.service` |
| Symbol | `~/.local/share/icons/mypods.png` |

Der Daemon läuft als systemd-User-Dienst, unabhängig vom Tray-Programm: Ohrerkennung, automatisches Umschalten und die Effekte laufen weiter, wenn du das Tray-Programm beendest, und systemd startet den Daemon nach einem Absturz neu. Sein Protokoll steht in `journalctl --user -u mypods-core` und in `~/.local/state/mypods/core.log`. Ohne systemd (und im AppImage) startet das Tray-Programm den Daemon selbst. Ohne Bluetooth-Adapter beendet sich der Daemon, und systemd (oder das Tray-Programm) startet ihn wieder, bis einer auftaucht; er nimmt den ersten Adapter von BlueZ, der nicht `hci0` heißen muss. Dasselbe passiert, wenn der Adapter verschwindet oder bluetoothd neu startet oder abstürzt.

Zum Aktualisieren führst du das Skript einfach noch einmal aus. Es stoppt die laufende Instanz, installiert neu und startet wieder. Eine Installation von früher, als MyPods noch MagicPods hieß, räumt es dabei gleich mit weg.

**Deinstallieren**

```bash
systemctl --user disable --now mypods-core.service
pkill -x mypods; pkill -x mypodscore
rm -rf ~/.local/opt/mypods \
       ~/.local/share/applications/app.mypods.desktop \
       ~/.config/autostart/app.mypods.desktop \
       ~/.config/systemd/user/mypods-core.service \
       ~/.local/share/icons/mypods.png
systemctl --user daemon-reload
rm -rf ~/.config/mypods ~/.local/state/mypods   # optional: Einstellungen, gespeicherte AirPods-Schlüssel, Protokolle
```

### Das brauchst du

**Zur Laufzeit**

- Linux mit **BlueZ** und **PipeWire** (mit `pipewire-pulse`, denn der Daemon nutzt libpulse)
- **Qt 6.9 oder neuer**: `qt6-base`, `qt6-declarative`, `qt6-websockets`, `qt6-svg`
- `libpulse`, `openssl`, `systemd-libs`
- `libmysofa` (nur für 3D-Audio; die Standard-HRTF liegt unter `/usr/share/libmysofa/default.sofa`)
- Optional: `swh-plugins` für den Limiter mit Vorausschau (LADSPA `fast_lookahead_limiter_1913.so`, gesucht in `$LADSPA_PATH` oder `/usr/lib/ladspa`). Ohne ihn spielt 3D-Audio 7 dB leiser, um sauber zu bleiben.
- Ein Player mit MPRIS für automatische Pause, Umschalten und die Wiedergabe im Tray-Popup (Browser, Spotify, mpv mit `mpv-mpris`, …)
- `pactl` (Teil von `libpulse`) für den Lautstärkeregler im Tray-Popup
- Für die Wiedergabe des iPhones: das iPhone mit diesem Computer gekoppelt und ein Adapter mit LE-Advertising. MyPods wirbt für den Apple Media Service, und das iPhone verbindet sich von selbst über LE. Die Kopplung muss auch die LE-Schlüssel liefern (das iPhone leitet sie aus einer klassischen Kopplung ab); Intel-Adapter tun das, manche Realtek-Adapter und USB-Dongles nicht.
- Optional: `layer-shell-qt` (kommt mit KDE Plasma), damit das Tray-Popup unter Wayland unter der Leiste sitzt
- Ein System-Tray. KDE Plasma, Cinnamon, XFCE und die meisten Leisten haben einen; GNOME braucht die Erweiterung *AppIndicator and KStatusNotifierItem Support*. Ohne Tray öffnet sich MyPods als Fenster, und der Daemon arbeitet im Hintergrund weiter.
- Für den Akku in den Bluetooth-Einstellungen des Systems: ein BlueZ mit der Battery-Provider-API (aktuelle Versionen; ältere brauchen `bluetoothd --experimental`). Alles andere geht auch ohne.

**Zum Bauen**

- **Podman**: Der Build läuft in einem Container, auf deinem System wird nichts installiert.

Der Container basiert auf Arch Linux, damit die Programme zu einem Arch-basierten System passen (Arch, CachyOS, EndeavourOS, …). Auf anderen Distributionen baust du nativ (siehe [Selbst bauen](#selbst-bauen)).

Auf Arch-basierten Systemen sind das die Laufzeitpakete:

```bash
sudo pacman -S --needed bluez pipewire pipewire-pulse libpulse openssl \
    qt6-base qt6-declarative qt6-websockets qt6-svg libmysofa swh-plugins layer-shell-qt
```

<br>

## Auch unter Windows.

Lade `MyPods-x64.msi` aus dem [neuesten Release](https://github.com/HanreichC/MyPods/releases/latest) und installiere es. Es landet in `C:\Program Files\MyPods`, legt einen Startmenü-Eintrag an und startet MyPods beim Anmelden versteckt im Tray. Die Einstellungen liegen in `%APPDATA%\mypods\config.toml`, die Protokolle in `%LOCALAPPDATA%\MyPods\logs`. Gibt es eine neuere Version, sagt MyPods es dir unter *Über* (abschaltbar unter *Einstellungen → Allgemein → Nach Updates suchen*); das MSI aktualisiert an Ort und Stelle und beendet ein laufendes MyPods vorher.

Windows lässt nur Kerneltreiber L2CAP-Kanäle öffnen, und jede AirPods-Einstellung reist über L2CAP (AAP). MyPods bringt keinen Bluetooth-Treiber mit, also macht es unter Windows, was ohne Treiber geht:

| Funktion | Windows |
|----------|---------|
| Geräteliste, verbinden/trennen, Bluetooth an/aus | Ja (WinRT, Bluetooth-Audiotreiber) |
| Deckel-Popup mit Animation | Ja, aus den BLE-Advertisements |
| AirPods-Akku in App und Tray | Ja, aus den Advertisements: 10-%-Schritte, 1 % mit importierten Schlüsseln |
| Ohrerkennung (Pause/Weiter) | Ja, aus den Advertisements; an/aus wird lokal gespeichert |
| Tray-Popup: Wiedergabe, Medientasten, Lautstärke | Ja (System-Mediensitzungen, Core Audio) |
| Die Musik des iPhones im Tray-Popup: Titel, Cover, Play/Pause/Skip, seine Lautstärke | Ja, über den Apple Media Service (BLE), mit dem iPhone in den Bluetooth-Einstellungen von Windows gekoppelt. Windows verbindet sich selbst mit dem iPhone, kein Advertising nötig. |
| Parrot Zik 2.0, Galaxy Buds (RFCOMM) | Ja, alle Funktionen |
| Beoplay E8 (1. Generation) | Ja: Geräteinfos und Transparenz-Zustand über LE |
| HFP-Akku anderer Headsets | Ja |
| Geräuschkontrolle, Konversationserkennung, Drück-/Streich-Einstellungen, Mikrofon, Hörgerät | Nein, braucht AAP |
| Reduzierung lauter Geräusche, angepasste Transparenz | Nein, braucht einen ATT-Kanal über L2CAP |
| Akku in den Bluetooth-Einstellungen des Systems | Nein, Windows nimmt keine Werte von Anwendungen |
| Automatisches Umschalten mit dem iPhone („Hierher holen“) | Nein, braucht AAP |
| Equalizer, Kopfhörerkorrektur, Hörprofil, gehörrichtige Lautstärke, Crossfeed | Ja, für Bluetooth- und Kabelkopfhörer, über die Engine von Equalizer APO, die das MSI mitbringt und auf die Kopfhörerausgänge legt. Kopfhörer, die später dazukommen, fragen einmal nach Administratorrechten. Ein selbst installiertes Equalizer APO wird so genutzt, wie es ist. |
| Kopfhörer an der Buchse eines Monitors | Ja, einmal markiert unter *Einstellungen → Kopfhörer an diesen Ausgängen*: Monitore sagen Windows nicht, ob etwas eingesteckt ist |
| Ausgabe wählen (Seitenleiste, *Ausgabe* im Tray-Popup) | Ja, die Standardausgabe von Windows für Musik, Anrufe und Systemklänge |
| Tastenkürzel | Ja, unter *Einstellungen → Allgemein*: Strg+Alt+Umschalt+N Geräuschkontrolle, +O nächste Ausgabe, +H Hierher holen |
| 3D-Audio | Nein, bisher nur Linux |
| Signalweg, Hi-Res-Wiedergabe, höchste Bluetooth-Qualität | Nein, die Audio-Engine von Windows wählt das Format selbst |
| Codec-Anzeige und -Wechsel | Nein, Windows legt beides nicht offen |

**Deine AirPods erkennen.** Unter Linux holt MyPods den IRK und den ENC-Schlüssel der AirPods über AAP. Ohne AAP hält es die AirPods deines gekoppelten Modells im Umkreis von etwa einem Meter für deine, was durcheinanderkommt, wenn ein zweites Paar desselben Modells direkt neben dir liegt. Nutzt du MyPods auch unter Linux, kopiere die Zeilen `irk` und `enc` aus der Tabelle deiner AirPods in `~/.config/mypods/config.toml` in dieselbe Tabelle in `%APPDATA%\mypods\config.toml` (der Tabellenname ist die Adresse der AirPods). Dann prüft MyPods sie richtig und zeigt den genauen Akku.

<br>

## Einmal einrichten. Nahtlos wechseln.

Damit dein iPhone und dieser Computer die AirPods untereinander weiterreichen, muss sich BlueZ als Apple-Gerät melden. Sonst beziehen iPhone und AirPods den Computer nicht in die Übergabe ein. Alles andere funktioniert auch ohne diesen Schritt.

```bash
sudo sed -i '/^\[General\]/a DeviceID = bluetooth:004C:0000:0000' /etc/bluetooth/main.conf
sudo systemctl restart bluetooth
```

Verbinde die AirPods danach einmal neu. `install.sh` erinnert dich, wenn die Zeile fehlt.

> [!TIP]
> Die Apple-DeviceID schaltet auf Seiten der AirPods weitere Funktionen frei, etwa das Multipoint-Verhalten bei der Übergabe.

<br>

## So bedienst du MyPods.

1. **Koppel deine Kopfhörer** wie gewohnt mit dem System. Bei AirPods hältst du die Taste am Case, bis das Licht weiß blinkt, und koppelst dann über die Bluetooth-Einstellungen deines Desktops oder `bluetoothctl`.
2. **Starte MyPods** aus dem Startmenü (beim Anmelden startet es auch von selbst). Die Oberfläche startet den Daemon selbst.
3. **Öffne das Case** neben deinem Computer. Das Popup erscheint mit Animation und Akkuständen.
4. **Klick auf das Tray-Symbol** für das Popup: Akku, Geräuschkontrolle, Konversationserkennung, was läuft mit Mediensteuerung, die Lautstärke und *Ausgabe*: jede Ausgabe, die aktive mit Haken, ein Klick spielt dort. Zeigst du auf das Symbol, siehst du den Akku und, sobald MyPods es abschätzen kann, wie lange er ungefähr hält. Es schließt sich bei einem zweiten Klick, mit Esc oder einem Klick daneben. Ohne verbundene Kopfhörer öffnet ein Klick stattdessen das Fenster, außer ein gekoppeltes iPhone spielt: Dann zeigt das Popup die Musik des iPhones, und sein Lautstärkeregler ändert die Lautstärke des iPhones.
5. **Doppelklick auf das Tray-Symbol** (oder **MyPods öffnen** im Popup) für das ganze Fenster. Ein Klick auf Kopfhörer in der Seitenleiste macht sie zu denen, die du steuerst, und spielt den Ton dort; ein Lautsprecher-Symbol zeigt, welche gerade spielen. Dort findest du Ohrerkennung, automatisches Umschalten, 3D-Audio, Equalizer und alle Einstellungen deines Geräts. Ein Rechtsklick öffnet das Kontextmenü mit Verbinden/Trennen und Beenden. Das (i) neben einer Einstellung erklärt, was sie tut.

Befehlszeile:

```bash
mypods --hidden            # minimiert im Tray starten (nutzt der Autostart)
mypodscore --version       # Version des Daemons
mypodscore --selftest      # die eingebauten Byte-Prüfungen, ohne Hardware
```

Startest du `mypods` ein zweites Mal, holt es das bestehende Fenster nach vorn, statt eine weitere Instanz zu starten.

<br>

## Feinschliff in der config.toml.

Der Daemon speichert die Einstellungen als TOML:

```
${XDG_CONFIG_HOME:-~/.config}/mypods/config.toml
```

Das meiste stellst du in der Oberfläche ein. Globale Optionen stehen in der Tabelle `[mypods]` (früher `[magicpods]`; MyPods zieht sie beim ersten Start einmal um):

| Schlüssel | Standard | Bedeutung |
|-----------|----------|-----------|
| `animation` | `true` | Zeigt das Deckel-Popup. Der BLE-Scan dahinter läuft nur, solange AirPods gekoppelt sind und keine verbunden ist (unter Windows auch verbunden, die Advertisements sind alles, was es hat). Automatisches Umschalten nutzt denselben Scan, um zu wissen, ob du die AirPods trägst; mit „Automatisch“ läuft er also auch weiter, wenn das Popup aus ist — siehe [Wenn's hakt](#wenns-hakt). |
| `logLevel` | Info | Wie ausführlich der Daemon protokolliert (Debug-Builds protokollieren immer auf Debug-Stufe). |
| `hiRes` | `false` | Hi-Res-Wiedergabe (Einstellungen): die PipeWire-Drop-ins unter [Bluetooth-Klangqualität](#bluetooth-klangqualität). Nur Linux. |
| `headphoneOutputs` | leer | Als Kopfhörer markierte Ausgänge (Einstellungen), ein Sink pro Zeile. |

Einstellungen pro Gerät (etwa die gespeicherten Schlüssel `irk`/`enc`, der Umschaltmodus, 3D-Audio und Equalizer) stehen in einer Tabelle, die nach dem Gerät heißt. Löschst du die Schlüssel, fragt MyPods bei der nächsten Verbindung neu danach. Diese hier setzt du nur von Hand:

| Schlüssel | Standard | Bedeutung |
|-----------|----------|-----------|
| `sofa` | KEMAR aus libmysofa | Pfad zu einer SOFA-Datei (etwa einer persönlichen HRTF) für 3D-Audio. Bass-Umgehung und klangliche Korrektur sind auf die Standard-KEMAR abgestimmt und fallen bei anderen Dateien weg. |
| `eqFile` | keine | Pfad zu einer `ParametricEQ.txt` (AutoEQ, Equalizer APO), die die eingebaute Kopfhörerkorrektur ersetzt; „Kopfhörerkorrektur“ schaltet dann diese. Ihr Preamp wird ignoriert (die Vorverstärkung wird berechnet). Dateien mit Abschnitten pro Kanal werden abgelehnt. |
| `loudnessReference` | `90` | Gehörrichtige Lautstärke: Phon bei 100 % Lautstärke. Mit einem Schallpegelmesser gemessen, kalibriert er die Kompensation für deine Kopfhörer; höher heißt mehr Bass bei gleicher Lautstärke. |

### Bluetooth-Klangqualität

AirPods und Beats sprechen nur SBC und AAC, also ist AAC das Beste, was es gibt (LDAC und aptX unterstützen die Kopfhörer nicht), und Bluetooth-Audio ist immer verlustbehaftet. Der AAC-Encoder von PipeWire arbeitet standardmäßig mit konstanter Bitrate, und LDAC passt seine Bitrate der Verbindung an. **Höchste Bluetooth-Qualität** auf der Geräteseite stellt diese Kopfhörer auf die beste variable AAC-Bitrate (nur Kopfhörer, die VBR anbieten, nehmen sie an) und auf LDACs 990 kbit/s.

**Hi-Res-Wiedergabe** (Einstellungen) lässt die Ausgabe der Abtastrate der Quelle folgen, bis 192 kHz, statt alles auf 48 kHz umzurechnen, und gibt dem, was noch umgerechnet wird, den besten Resampler von PipeWire. Das hilft Kabelkopfhörern und USB-DACs; Bluetooth setzt seine eigene Rate. Die Effektkette läuft ebenfalls mit der Rate der Quelle. **Signalweg** auf der Geräteseite zeigt, ob das, was spielt, bitgenau ankommt, und wenn nicht, warum (Effekte, der Bluetooth-Codec, Umrechnung, weniger Bits, die Lautstärke einer App).

Sonst lässt MyPods die PipeWire-Konfiguration des Systems in Ruhe. Diese beiden Optionen sind Drop-in-Dateien, die es beim Einschalten schreibt und beim Ausschalten entfernt; danach startet es die zugehörigen Dienste neu, der Ton setzt also kurz aus:

| Option | Dateien unter `~/.config` |
|--------|---------------------------|
| Hi-Res-Wiedergabe | `pipewire/pipewire.conf.d/60-mypods-hires.conf`, `pipewire/pipewire-pulse.conf.d/60-mypods-hires.conf`, `pipewire/client.conf.d/60-mypods-hires.conf` |
| Höchste Bluetooth-Qualität | `wireplumber/wireplumber.conf.d/60-mypods-bluetooth.conf` (WirePlumber 0.5) |

Beim Deinstallieren bleiben sie liegen; lösch sie von Hand, wenn du die Optionen eingeschaltet hattest.

<br>

## Selbst bauen.

### Mit Podman (empfohlen)

Nichts landet global auf deinem System; die Werkzeuge leben im Dev-Container ([.devcontainer/Dockerfile](.devcontainer/Dockerfile)).

```bash
podman build -t mypods-dev -f .devcontainer/Dockerfile .devcontainer
podman run --rm -v "$PWD:/workspace" -w /workspace mypods-dev \
    sh -c 'cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)'
```

Ergebnis:

- `build/mypods`: die Oberfläche
- `build/modules/mypodscore`: der Daemon

Starte `./build/mypods` auf deinem System. Die Oberfläche sucht den Daemon in `modules/` neben sich und startet ihn.

Für einen Debug-Build nimmst du `-DCMAKE_BUILD_TYPE=Debug`. Das schaltet Debug-Protokolle ein und führt die Selbsttests beim Start des Daemons aus.

### Windows

Visual Studio 2022 oder neuer mit dem C++-Workload, Qt 6.9 für MSVC (mit Qt WebSockets) und vcpkg. Aus einer *Developer PowerShell*:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_PREFIX_PATH=C:\Qt\6.9.3\msvc2022_64 `
    -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
cmake --build build
build\modules\mypodscore.exe --selftest
```

vcpkg installiert OpenSSL, libuv und zlib aus [vcpkg.json](vcpkg.json). Das MSI baut [.github/workflows/windows.yml](.github/workflows/windows.yml) mit WiX 5 aus [packaging/windows/MyPods.wxs](packaging/windows/MyPods.wxs).

### In VS Code / VSCodium

Das Repository bringt eine Dev-Container-Definition mit ([.devcontainer/devcontainer.json](.devcontainer/devcontainer.json)). Mit einer Dev-Container-Erweiterung wählst du **Devcontainer: Open Folder in container**. Der Container nutzt das Netzwerk des Hosts und dessen System-D-Bus-Socket, so kann der Daemon von drinnen mit BlueZ sprechen.

### Nativ

Auf einer anderen Distribution als Arch installierst du die Entsprechungen der Pakete aus dem Dockerfile (CMake ≥ 3.22, ein C++20-Compiler, pkg-config, Qt 6.9+ samt Qt-Linguist-Werkzeugen, BlueZ-Header, libpulse, OpenSSL, libsystemd) und führst dieselben zwei CMake-Befehle direkt aus. Alle anderen Bibliotheken (sdbus-c++, uWebSockets, uSockets, nlohmann/json, toml++) liegen im Repository und bauen offline.

<br>

## Geprüft. Immer wieder.

MyPods folgt einer einfachen Regel: Jedes Stück nicht-triviale Logik hat genau eine ausführbare Prüfung, ganz ohne Test-Framework. Bis auf zwei laufen alle ohne Hardware.

```bash
# Alles, was keine Hardware braucht, in einem Rutsch (die CI führt es vor dem AppImage-Build aus)
sh tools/run_checks.sh build

# Daemon: AAP-Akku/ANC, BLE-Advertisements, Smart-Routing-Pakete, Steuerbefehle, ATT-Einstellungen,
# Effektkette, Signalweg, Einstellungsdatei samt Umzug von [magicpods], Ereignisse über Threads hinweg,
# Galaxy-Buds- und Parrot-Zik-Protokoll. Lohnt sich auch in einem Build mit -fsanitize=address,undefined.
./build/modules/mypodscore --selftest

# Emulierte AirPods Max durch den echten Audiopfad (3D-Audio, Kopfverfolgung, EQ, Klangfarbe, Limiter,
# gehörrichtige Lautstärke, Hörprofil, A/B, 7.1, Routing, ein Neustart des Soundservers, Hi-Res mit einer
# 96-kHz-Quelle und der Signalweg, die Wahl der Ausgabe), und emulierte AirPods Pro 3, die den Ton mit
# einem iPhone hin- und herreichen (echte MPRIS-Player, die Smart-Routing-Nachrichten des iPhones, ein
# falscher BlueZ-Adapter) und jede Einstellung anbieten, die sie melden. Braucht PipeWire, kein Bluetooth;
# schaltet die Standardausgabe kurz auf einen falschen Kopfhörer-Sink. Am besten im Container: Er startet
# PipeWire neu, schreibt die Hi-Res-Drop-ins und belegt org.bluez auf dem System-Bus des Containers.
./build/modules/mypodscore --emulate-airpods
# Dasselbe im Dev-Container, mit eigenem PipeWire (lässt den Ton des Hosts in Ruhe)
podman run --rm -v "$PWD:/workspace" -w /workspace <dev image> sh tools/emulate_in_container.sh build

# Der BLE-Decoder im Mitschnitt-Werkzeug, gegen bekannte Mitschnitte
python3 tools/sniff.py --selftest

# Jeder Text der Oberfläche in jeder Sprache übersetzt
python3 tools/check_translations.py
```

`run_checks.sh` baut und startet außerdem die eigenständigen Prüfungen für den ANC- und Akku-Pfad, die Akku-Warnung, die Restlaufzeit, die Tastenkürzel-Aktionen, die Werte der iPhone-Wiedergabe, die Exec-Zeile des Menüeintrags, den Picker, das Deckel-Popup und die (i)-Erklärungen ([core/src/tests/AncSelfCheck.cpp](core/src/tests/AncSelfCheck.cpp), [core/src/tests/BatterySelfCheck.cpp](core/src/tests/BatterySelfCheck.cpp), [ui/tests/LowBatteryCheck.cpp](ui/tests/LowBatteryCheck.cpp), [ui/tests/BatteryEstimateCheck.cpp](ui/tests/BatteryEstimateCheck.cpp), [ui/tests/ActionsCheck.cpp](ui/tests/ActionsCheck.cpp), [ui/tests/AmsCheck.cpp](ui/tests/AmsCheck.cpp), [ui/tests/DesktopCheck.cpp](ui/tests/DesktopCheck.cpp), [ui/tests/tst_picker.qml](ui/tests/tst_picker.qml), [ui/tests/tst_popup.qml](ui/tests/tst_popup.qml), [ui/tests/tst_info.qml](ui/tests/tst_info.qml)); jede Datei nennt auch ihren eigenen Build-Befehl.

Zwei Prüfungen brauchen Hardware, weil das Verhalten, das sie absichern, nur an einem echten Adapter existiert:

```bash
# Mit laufendem MyPods: schlägt fehl, wenn eine Discovery offen gehalten wird, obwohl kein Apple-Gerät
# gekoppelt ist; genau das hindert Bluetooth-LE-Mäuse und -Tastaturen am Wiederverbinden
sh tools/check_no_idle_discovery.sh

# Mit laufendem MyPods: die Kopfhörer in bluetoothctl entfernen, das hier starten, dann koppeln;
# besteht, sobald der Core eine Geräteliste mit ihnen schickt, ohne Neustart
python3 tools/check_new_pairing.py AA:BB:CC:DD:EE:FF
```

### Mitschnitte von echter Hardware

[tools/sniff.py](tools/sniff.py) zeichnet Apple-BLE-Advertisements über BlueZ und D-Bus auf und dekodiert sie. Ganz ohne root.

```bash
python3 tools/sniff.py                                         # live ansehen
python3 tools/sniff.py --log captures/pro3.txt --seconds 120   # in eine Datei aufzeichnen
python3 tools/sniff.py --all                                   # auch andere Apple-Nachrichtentypen zeigen
```

Während es läuft, öffnest und schließt du den Deckel ein paar Mal und nimmst die Pods heraus und setzt sie wieder ein. Die Mitschnitte bestätigen Modell-IDs und das Verhalten des Deckelzählers und werden zu Testvektoren für den Parser.

AirPods Max haben kein Lade-Case. Bei ihnen ist der Popup-Auslöser das obere Nibble von Byte 8 (`== 8`) statt des Case-Zustands.

<br>

## Für Entwickler: die API.

Oberfläche und Daemon sind getrennte Prozesse, die über einen WebSocket auf Port `2020` JSON sprechen. So kannst du eigene Oberflächen, Skripte oder Integrationen schreiben.

```bash
# Beispiel mit websocat
echo '{"method":"GetAll"}' | websocat -n1 ws://127.0.0.1:2020/
```

Methoden: `GetAll`, `GetDevices`, `ConnectDevice`, `DisconnectDevice`, `GetActiveDeviceInfo`, `SetActiveDevice`, `SetCapabilities`, `GetOutputs`, `SetOutputHeadphones`, `GetPlaybackOutputs`, `SetPlaybackOutput`, `NextPlaybackOutput`, `GetDefaultBluetoothAdapter`, `EnableDefaultBluetoothAdapter`, `DisableDefaultBluetoothAdapter`, `GetSettingsAll`, `GetSettings`, `GetSetting`, `SetSetting`. Der Daemon schickt außerdem jede Änderung (Fähigkeiten, Verbindungszustand, aktives Gerät, Ausgabe, Adapterzustand, Einstellungen, Popup-Auslöser) an jeden verbundenen Client.

Die vollständige Referenz mit Beispielen für Anfragen und Antworten steht in [docs/core-api-reference.md](docs/core-api-reference.md).

<br>

## Was (noch) nicht geht.

- **Allgemeine HRTF.** 3D-Audio nutzt die allgemeine KEMAR-HRTF aus libmysofa. Apple personalisiert sie aus einem Scan deiner Ohren. Eine persönliche SOFA-Datei lässt sich pro Gerät setzen (`sofa`, siehe [Konfiguration](#feinschliff-in-der-configtoml)). Unter 250 Hz umgeht der Bass die HRTF, deren unteres Ende unbrauchbar ist. 3D-Audio spielt mit Limiter 3 dB leiser, ohne ihn etwa 10 dB (dann kann sehr lautes Material noch übersteuern, wenn du den Kopf mehr als 45° vom Bildschirm wegdrehst); dreh die Kopfhörer zum Ausgleich lauter. Die 3 dB sind eine Abwägung: Sehr laute Master bringen den Limiter zum Arbeiten. 7.1 bekommt 3 dB mehr, und seine Center-, Seiten- und hinteren Lautsprecher nutzen die klangliche Korrektur des vorderen Paars.
- **Gehörrichtige Lautstärke und Hörprofil sind nicht kalibriert.** Welcher Hörpegel einer Lautstärke entspricht, hängt von den Kopfhörern ab (`loudnessReference`, siehe [Konfiguration](#feinschliff-in-der-configtoml)). Das Hörprofil nutzt die Half-Gain-Regel, keine Anpassformel wie NAL-NL2, und hat keinen eingebauten Hörtest: Ein unkalibrierter Tontest kann kein dB HL liefern. Trag ein Audiogramm von einer Hörakustikerin, einem Hörakustiker oder aus einer Hörtest-App ein.
- **Kalibrierung der Kopfverfolgung.** Die Auswertung des Kopfverfolgungs-Streams ist eine Heuristik aus LibrePods und noch nicht an echter Hardware kalibriert.
- **Keine Übernahme für Ton ohne MPRIS.** Spiele und Systemklänge lösen das automatische Umschalten bewusst nicht aus, wie am Mac.
- **Die Übergabe mit dem iPhone** braucht die Apple-DeviceID in BlueZ (siehe [Einrichtung](#einmal-einrichten-nahtlos-wechseln)).
- **Nicht umgesetzt:** Herzfrequenz (AirPods Pro 3), Wo ist? und der Case-Lautsprecher. Diese Teile von Apples Protokoll sind öffentlich nicht entschlüsselt. Kopfgesten (Nicken oder Schütteln, um einen Anruf anzunehmen) auch nicht: Die AirPods liefern nur die Kopfbewegung, das Telefon deutet sie, und ein Linux-Desktop hat keinen Anruf anzunehmen.
- **Einstellungen der AirPods Pro 2/3 über ATT** (Reduzierung lauter Geräusche, angepasste Transparenz) folgen LibrePods und sind nicht an Hardware geprüft, auch nicht die Wertebereiche der Transparenz-Regler. Der Hörgerät-Schalter braucht den Hörtest von einem iPhone; ohne ihn bleibt er verborgen.
- **Popup bei Verbindung (Linux).** Der BLE-Scan pausiert, solange AirPods verbunden sind; öffnest du das Case von AirPods, die noch mit diesem Computer verbunden sind, kommt kein Popup. Sind sie getrennt, oder verbindest du sie gerade, kommt es.
- **Binärkompatibilität.** Was `install.sh` baut, ist für Arch-basierte Systeme.
- **Platz des Tray-Popups.** Unter Wayland dürfen Apps ihre Fenster nicht selbst platzieren, deshalb ist das Popup eine Layer-Shell-Fläche oben rechts: richtig für eine Leiste oben, falsch für eine unten. Das AppImage ist ohne `layer-shell-qt` gebaut, dort platziert der Compositor das Popup. Unter X11 öffnet es am Mauszeiger, für eine Leiste an jedem Rand.
- **Die Wiedergabe des iPhones** bietet, was AMS hat: kein Stummschalten, kein Stöbern oder Auswählen von Titeln, kein Weg festzustellen, ob ein Titel schon Favorit ist (der Stern zeigt, was in MyPods geklickt wurde), und die Lautstärke nur in den 16 Stufen des iPhones. AMS schickt kein Cover; es wird über Interpret und Titel gesucht und fehlt bei Titeln, die der iTunes-Katalog nicht kennt. Die Musik des iPhones erscheint, wenn am Computer nichts spielt oder nichts geladen ist. Unter Linux ist sie gegen einen Ersatz für bluetoothd getestet, noch nicht mit einem echten iPhone.
- **Doppelklick im Tray** sind zwei Klicks innerhalb des Doppelklick-Intervalls des Systems, denn das Tray-Protokoll von KDE (StatusNotifierItem) kennt keinen. Das Popup öffnet sich beim ersten Klick und macht beim zweiten dem Fenster Platz.
- **Restlaufzeit** ist eine grobe Schätzung: eine Gerade seit dem letzten Laden, frühestens nach 20 Minuten und 3 % Verbrauch. Akkus fallen gegen Ende schneller, und Werte in 10-%-Schritten (AirPods ohne AAP, unter Windows) lassen sie springen.

<br>

## Wenn's hakt.

**Wo sind die Protokolle?**
Der Daemon schreibt `core.log`, die Oberfläche `ui.log`, in `~/.local/state/mypods` (Linux) oder `%LOCALAPPDATA%\MyPods\logs` (Windows); *Über → Diagnose → Protokolle* öffnet den Ordner. Über 1 MB wandert eine Datei beim nächsten Start nach `.1`. Die Diagnose sagt auch, ob Equalizer APO da ist (Windows) und ob das gekoppelte iPhone erreichbar ist.

**Das Popup erscheint nicht.**
Prüf, ob die Einstellung `animation` an ist und Bluetooth läuft. Starte `python3 tools/sniff.py` und öffne das Case; zeigt sich nichts, empfängt der Adapter die Advertisements nicht. Das Popup braucht auch die Schlüssel IRK/ENC, die geholt werden, wenn sich die AirPods mit MyPods verbinden. Unter Linux gibt es kein Popup, solange die AirPods verbunden sind (siehe [Was (noch) nicht geht](#was-noch-nicht-geht)).

**Meine Bluetooth-Maus oder -Tastatur verbindet sich nicht mehr von selbst.**
Das ist im BLE-Scan des Deckel-Popups behoben: Er setzt keinen Discovery-Filter nur für LE mehr. Eine reine LE-Discovery scannt ohne Lücke, und der Kernel kommt nie dazu, das Allowlist-Auto-Connect abzuschließen, das gekoppelte Bluetooth-LE-Geräte brauchen; sie bleiben also getrennt, solange MyPods scannt. Der Standard-Transport verschachtelt BR/EDR-Inquiry mit dem LE-Scan und lässt genau diese Lücken, ohne messbare Einbuße bei den Advertisements. Um den Scan ganz abzuschalten, schalte das Popup aus (`animation = false`) und stell das automatische Umschalten auf „Bei letzter Verbindung mit diesem Computer“.

**Die Musik stottert, während die AirPods spielen.**
Die klassische Inquiry des BLE-Scans konkurriert bei vielen Adaptern mit A2DP, deshalb pausiert MyPods den Scan unter Linux, solange AirPods verbunden sind. Stottert es trotzdem, such in `journalctl --user -u mypods-core` nach „BLE scan started“, während sie verbunden sind, und prüf, ob ein anderes Programm einen Bluetooth-Scan laufen lässt.

**Geräuschkontrolle und Einstellungen fehlen.**
Sie brauchen eine aktive klassische Verbindung (A2DP/HFP). Verbinde die AirPods zuerst. Tauchen sie dann immer noch nicht auf, stell sicher, dass kein anderes Werkzeug (etwa LibrePods) den L2CAP-Kanal hält; das Protokoll des Daemons sagt dann „control channel unavailable“. Der Ton läuft weiter, und die nächste Verbindung versucht es erneut. Schließen die AirPods den Kanal von selbst, öffnet der Daemon ihn wieder; nach fünf Versuchen in einer Verbindung protokolliert er „control channel keeps closing“ und wartet auf die nächste Verbindung.

**Der Akku erscheint nicht in den Bluetooth-Einstellungen des Systems.**
Das Protokoll des Daemons sagt „BlueZ takes no battery levels from MyPods“, wenn BlueZ die Battery-Provider-API fehlt; ältere BlueZ-Versionen bieten sie nur mit `bluetoothd --experimental`. Ein Headset, das seinen Akku schon über HFP meldet (über PipeWire), behält diesen Wert.

**Automatisches Umschalten klappt nicht.**
Prüf die Zeile `DeviceID = bluetooth:004C:0000:0000` in `/etc/bluetooth/main.conf`, starte Bluetooth neu und verbinde die AirPods neu. Umschalten fällt auch aus, solange auf dem anderen Gerät ein Anruf läuft, und nur MPRIS-Player lösen es aus.

**Die Oberfläche erreicht den Daemon nicht.**
Vielleicht belegt etwas anderes Port 2020 auf `127.0.0.1`, oder ein alter Daemon läuft noch: `systemctl --user restart mypods-core` (oder `pkill -x mypodscore` ohne den Dienst) und MyPods neu starten. `journalctl --user -u mypods-core` zeigt das Protokoll des Daemons.

**Die Über-Seite zeigt verschiedene Versionen.**
Jeder Build hat eine Version, `MAJOR.MINOR.<Commits seit MAJOR.MINOR gesetzt wurde>` (etwa `1.0.5`), dieselbe auf der Über-Seite, im MSI, im Arch-Paket und im Release-Titel. „MyPods Core“ ist der Daemon, der tatsächlich läuft. Weicht er von der App ab, läuft noch ein Daemon einer anderen Installation, typischerweise ein alter User-Dienst `mypods-core`: `systemctl --user cat mypods-core` zeigt, welches Programm er startet; starte ihn neu oder entferne die alte Kopie.

**Die Musik des iPhones erscheint nicht im Tray-Popup.**
Das iPhone muss mit diesem Computer gekoppelt sein und etwas spielen. Unter Linux zeigt `journalctl --user -b | grep iPhone` „no LE advertisement“, wenn der Adapter nicht werben kann, und `bluetoothctl info <iPhone-Adresse>` sollte den Dienst `89d3502b-0f36-433a-8ef4-c502ad55f8dc` listen, solange das iPhone in der Nähe ist. Taucht er nie auf, hat die Kopplung keine LE-Schlüssel geliefert (manche Realtek-Adapter und USB-Dongles): Entferne das iPhone auf beiden Seiten und koppel es neu, vom Computer aus. Unter Windows erscheint das iPhone in *Einstellungen → Bluetooth und Geräte*; dort zu koppeln genügt. Findet MyPods es trotzdem nicht, fragt es das iPhone direkt, auch wenn der Bluetooth-Cache von Windows den Medien-Dienst vergessen hat.

**3D-Audio hat keine Wirkung.**
Prüf, ob `/usr/share/libmysofa/default.sofa` existiert (Paket `libmysofa`) und ob die Anwendungen auf den Sink `mypods_fx` spielen.

**Der Signalweg sagt „umgerechnet“, obwohl Hi-Res-Wiedergabe an ist.**
Der Graph übernimmt die Rate des ersten Streams, der startet, während nichts spielt; eine zweite Anwendung mit anderer Rate wird umgerechnet. Bluetooth läuft immer mit der Rate des Codecs. Prüf, ob `~/.config/pipewire/pipewire.conf.d/60-mypods-hires.conf` existiert, und starte PipeWire neu, falls MyPods es nicht konnte (`systemctl --user restart pipewire pipewire-pulse wireplumber`); ohne systemd warten die Dateien auf den nächsten Start.

<br>

## Sicherheit und Datenschutz.

- Die WebSocket-API des Daemons hat **keine Authentifizierung**. Sie lauscht nur auf `127.0.0.1:2020`, andere Rechner erreichen sie also nicht, aber jeder lokale Benutzer und Prozess. Sie lehnt Verbindungen mit einem `Origin`-Header ab; das hält Webseiten in deinem Browser davon ab, darüber Einstellungen zu ändern. Die Schlüssel der AirPods (`irk`, `enc`) verlassen den Daemon nie über die API: Antworten auf Einstellungen und Änderungsmeldungen lassen sie weg.
- Die Schlüssel IRK und ENC der AirPods stehen im Klartext in `~/.config/mypods/config.toml`. Der Daemon schreibt die Datei mit Modus `0600` und ersetzt sie atomar, ein Absturz hinterlässt sie also nie halb geschrieben. Lässt sie sich nicht lesen, wird sie nach `config.toml.broken` verschoben, und MyPods startet mit den Standardwerten. Die Protokolle enthalten die Schlüssel nur auf der Trace-Stufe (`logLevel = 0`); teile sie dann nicht.
- Für das Cover der iPhone-Musik schickt die Oberfläche Interpret und Titel jedes Titels an Apples iTunes Search API (`itunes.apple.com`), nur für Titel auf dem iPhone. Unter Windows fragt MyPods außerdem beim Start und dann täglich die GitHub-API (`api.github.com`) nach dem neuesten Release, sofern *Nach Updates suchen* an ist. Andere Anfragen ins Internet macht MyPods nicht.
- Unter Linux wirbt die Oberfläche über BLE, solange sie läuft, damit sich das gekoppelte iPhone verbinden kann. Andere Geräte in Reichweite können sich ebenfalls verbinden und sehen den eigenen GATT-Server von BlueZ, wie bei jedem LE-Peripheriegerät; MyPods fügt dem nichts hinzu.

<br>

## Herkunft und Lizenz.

MyPods baut auf bestehender Open-Source-Arbeit auf. Der Großteil des Codes stammt aus diesen GPL-3.0-Projekten, lokal kopiert und angepasst. Danke an alle, die das möglich machen:

- [MagicPodsCore](https://github.com/steam3d/MagicPodsCore) von Aleksandr Maslov und Andrei Litvintsev: der Daemon, der AAP-Stack, die BLE-Dekodierung, das Geräte- und Fähigkeitenmodell, die WebSocket-API
- [MagicPodsLinux](https://github.com/steam3d/MagicPodsLinux) von Aleksandr Maslov: die Qt/QML-Oberfläche, die Popup-Animation, die Sprites und der Tray
- [LibrePods](https://github.com/librepods-org/librepods): Protokolldokumentation, Smart Routing und Kopfverfolgung
- [AirPodsDesktop](https://github.com/SpriteOvO/AirPodsDesktop) von SpriteOvO: Continuity-Nachrichtentypen und Modell-IDs
- [zik2ctl](https://github.com/kradhub/zik2ctl) und [pyParrotZik](https://github.com/m0sia/pyParrotZik): das Protokoll des Parrot Zik 2.0
- [AutoEQ](https://github.com/jaakkopasanen/AutoEq) von Jaakko Pasanen (MIT), mit Messungen von oratory1990, crinacle und Rtings: die Filter der Kopfhörerkorrektur

Die vollständige Liste samt mitgelieferten Bibliotheken und wissenschaftlichen Arbeiten steht in [CREDITS.md](CREDITS.md).

MyPods steht unter der [GNU General Public License v3.0](LICENSE), weil der Code, auf dem es aufbaut, unter GPL-3.0 steht. Mitgelieferte Bibliotheken behalten ihre eigenen Lizenzen (MIT, Apache-2.0, LGPL-2.1 mit Ausnahme, SIL OFL 1.1), siehe [CREDITS.md](CREDITS.md).

<sub>MyPods ist ein unabhängiges Projekt und steht in keiner Verbindung zu Apple Inc., Parrot, Samsung Electronics Co., Ltd. oder Bang & Olufsen und wird von keinem davon unterstützt. Apple, AirPods, Beats, Apple Music, Apple Watch, iPhone, macOS und Liquid Glass sind Marken von Apple Inc.; Parrot und Zik sind Marken von Parrot; Samsung und Galaxy Buds sind Marken von Samsung Electronics Co., Ltd.; Beoplay ist eine Marke von Bang & Olufsen. Die Namen dienen hier nur zur Beschreibung von Geräten, Stil und Arbeitsweise.</sub>
