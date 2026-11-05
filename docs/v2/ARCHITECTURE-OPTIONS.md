# NAGLOED v2 — architectuur-opties

Verkenning, geen werkorder. Dit document hoort bij issue #14
(`leonvanbokhorst/nagloed`) en is **expliciet buiten scope van v1.0.4**. Het
beschrijft wat de huidige code doet, welke decomposities denkbaar zijn en welke
richting het eerste experiment het minste risico geeft. Niets hierin is een
implementatievoorstel voor `src/`, `CMakeLists.txt`, presets of release.

Leeswijzer: elke bewering is gemarkeerd als **[F] feit in deze checkout**,
**[T] technische oplossing/hypothese**, **[I] artistieke intentie** of
**[O] open onderzoeksvraag**. Aannames met een ID (U01, U02, …) staan uitgewerkt
in `docs/v2/TECHNICAL-UNCERTAINTIES.md`. Dit document is een **bijlage** bij de
verkenning, geen poort en geen backlog: alleen de artistieke visie, het
experiment en het veiligheidsblad zijn nu leidend.

---

## 1. Huidige architectuur zoals aangetroffen

### 1.1 Signaalpad en lagen

| Laag | Plek | Wat het doet | Bewijs |
|---|---|---|---|
| Contracts | `src/core/` | Bevroren publieke typen: parameter-IDs, ranges, `EngineParams`, `VisualState`, `PresetAccess` | `docs/ARCHITECTURE.md:17-24` |
| DSP | `src/dsp/` | `Engine` (pimpl) rond `EngineDSP`; Sintel → Ruimte → EAR | `src/dsp/EngineDSP.h:28-47` |
| GUI | `src/ui/` | Praat alleen via `VisualSource` + APVTS; include nooit `src/dsp` | `README.md:184-186`, `docs/ARCHITECTURE.md:32-34` |
| Plugin-glue | `src/plugin/` | `NagloedProcessor`: APVTS, buslayout, state, presets | `src/plugin/PluginProcessor.h:15-67` |
| Tools | `tools/` | `nagl_tests`, `nagl_render`, `nagl_preview` compileren dezelfde bronnen | `README.md:127-138`, `CMakeLists.txt:112-143` |

Het audiopad per blok: extra kanalen wissen, `readParams(apvts)` →
`engine.setParams(...)` → `engine.process(...)` → `engine.getVisual(v)` →
`visualChannel.publish(v)` (`src/plugin/PluginProcessor.cpp:41-55`). Binnen de
engine: capture + grains (Sintel), FDN (Ruimte), analoge keten + eindlimiter
(EAR), met een harde ±2.0-clamp als laatste veiligheidsstap
(`src/dsp/EngineDSP.cpp:600-664`). De latency is 51 samples bij 48 kHz
(`docs/VALIDATION.md:52-53`).

### 1.2 De echte seams

De vraag uit issue #14 is of `VisualState` als eventbus kan dienen. Dat kan
niet: het is een **displaykanaal**. De seams die er werkelijk zijn:

| Seam | Plek | Richting | Resolutie | Geschikt als eventbus? |
|---|---|---|---|---|
| `VisualState` / `SnapshotChannel` | `src/core/Contracts.h:27-57`, `src/core/Snapshot.h:9-32`; publicatie `PluginProcessor.cpp:52-54` | audio → GUI | 1× per blok, 256 `GrainView`-records | **Nee.** Display-kopie; engine mag een subset sturen (`docs/ARCHITECTURE.md:34`); geen tijdstempels, geen identiteit, geen geschiedenis, verlieslatend |
| `EngineParams` / APVTS | `src/core/EngineParams.h:12-76`, `src/core/Parameters.h:193-260` | host/bericht → audio | 1× per blok; parameters staan aan blokstart | **Nee.** Alleen actuele controlwaarden; geen events, geen `t`, geen oorzaak |
| Audio-blokgrens | `PluginProcessor.cpp:41`, `EngineDSP.cpp:502-504` | host ↔ plugin | 128–4096 samples | **Nee.** Alleen audio I/O; geen metadata-kanaal |
| MIDI-buffer | `PluginProcessor.h:32`, `EngineDSP.cpp:502-504` | host → plugin | — | **Nee.** `acceptsMidi()` is `false`; de engine negeert de buffer expliciet |
| `getVisual()` | `EngineDSP.cpp:671-701` | audio-thread, direct na `process()` | 1× per blok | **Nee.** Zelfde displaydoel, uitsluitend voor de GUI |
| Capture-ring / grainpool | `src/dsp/GrainEngine.h:150-166` | intern, per sample | 12 s ring, 256 stemmen | **Nee.** Privaat achter de pimpl (`Engine.h:39-43`) |
| Offline score/automation | `tools/DemoScore.h:13-26`, `tools/render_main.cpp:81-106` | score → processor | per blok, tijdgestuurd | **Bijna.** Bestaand reproduceerbaar renderpad, maar zonder licht- of eventmodel |
| APVTS-state/presets | `PluginProcessor.cpp:88-136`, `presets/*.json` | opslag | per save/load | **Nee.** Session-state, geen runtime-kanaal |

Conclusie **[F]**: er is in v1 geen sample-accurate eventbus en geen
licht-uitgang. De enige echte realtime-seam is de audioblokgrens plus
parameterupdates aan blokstart. Een v2-eventseam moet gebouwd worden, niet
hergebruikt. De interne control-rate is weliswaar 16 samples
(`src/dsp/EngineDSP.h:73`, absoluut sampleteller in
`src/dsp/EngineDSP.cpp:580-598`), maar dat is een *interne* rekensnelheid; de
plugin levert geen events op die resolutie.

### 1.3 Realtime- en threadingfeiten

- **[F]** `prepare()` alloceert; `process()`/`setParams()`/`getVisual()` niet.
  Bewezen met een global-`operator new`-teller: 0 allocaties
  (`tests/DspTests.cpp:1060-1119`, `docs/VALIDATION.md:48-49`).
- **[F]** Het audioresultaat is blokgrootte-onafhankelijk binnen 2% RMS voor
  128/512/4096 (`tests/DspTests.cpp:699-735`).
- **[F]** CPU ≈ 1.9–2.2% van één core bij 48 kHz/128 samples met een 64-grain
  cloud; ≈ 5.6% bij 200 grains (`docs/VALIDATION.md:50-51`).
- **[F]** Parameters worden aan blokstart gelezen; hostautomation is dus
  blokgekwantiseerd, ook al rekent de engine intern op 16-sampleframes
  (`src/core/Parameters.h:190-260`).
- **[F]** De GUI kan de engine niet bereiken buiten `VisualSource` om; de
  pimpl houdt `EngineDSP` privé (`src/dsp/Engine.h:14-44`).
- **[F]** Determinisme is alleen aantoonbaar per build/platform (offscreen
  editorbeeld byte-identiek, `README.md:148-150`); de demo-render is een
  offline WAV van 48 kHz/24-bit (`tools/render_main.cpp:49-51,74-76`).

### 1.4 Wat voor v2 ontbreekt

**[F]** Geen event/gesture-representatie, geen scheduler met tijdstempels, geen
geheugen/vergeten, geen licht-engine of SpectraStrobe-encoder, geen
post-DSP-routing, geen netwerktransport, geen consent-/privacylaag. Dat zijn
precies de zeven onderzoeksvragen uit issue #14 (Representation, Timing and
history, Audio architecture, SpectraStrobe encoding, Where in the signal chain,
Host behaviour, Human/hardware experience).

---

## 2. Vier kandidaat-decomposities

### 2.0 Overzicht

| | Hergebruik | Wordt zelfstandig | Grens | Realtime | Grootste risico | Verdict |
|---|---|---|---|---|---|---|
| **A** Externe companion/offline renderer | v1 volledig, render-toolpatroon | score, event-engine, licht, encoder, renderer | bestand (score → WAV/CSV) | geen (offline) | geen live-ervaring; hardware nog niet bewezen | **Aanbevolen eerste experiment** |
| **B** In-plugin control-rate event-seam + lichte engine | audiopad + capture-ring als bron | licht-engine, scheduler, geheugen, encoder | nieuwe interne API + bounded queue; build-flag | audio-thread of lock-free worker, 0,33 ms raster | v1 aanraken, host-routing, runaway | Fase 2, alleen na A + hardware |
| **C** Herbruikbare core + nieuwe v2-host | GrainEngine, Ruimte, EAR, EngineDSP | core-library + aparte v2-host | library-API + event-API; apart CMake-target | zelfde discipline, host kiest raster | grote refactor, regressie, dubbele QA | Middellange termijn, nieuwe branch |
| **D** Distributed peer-model | eventvocabulaire + encoder per node | peers, netwerklaag, lokale klok | bounded telegramprotocol | netwerkthread nooit in audiopad | runaway tussen kamers, drift, security | Horizon; detail in `docs/v2/DISTRIBUTED.md` |

### 2.A v1 onaangetast + externe companion/offline renderer

**[I]** Het eerste artistieke bewijs is niet "de plugin doet licht", maar "één
gedeelde gebeurtenis voelt verwant zonder synchroon te zijn" (issue #14). Dat
vraagt geen pluginwijziging.

**[T] Hergebruik.** De v1-plugin blijft draaien zoals hij is (AU/VST3/standalone,
`README.md:64-68`). Het bestaande offline patroon wordt hergebruikt:
`tools/render_main.cpp:45-115` toont een host-loze lus met een declaratieve
score (`tools/DemoScore.h:13-26`), blokgrootte 512, 48 kHz en 24-bit WAV. Voor
audio dat echt door v1 moet: dezelfde bronnen headless linken zoals
`NAGLOED_HEADLESS` dat al doet (`PluginProcessor.cpp:57-64`).

**[T] Wat zelfstandig wordt.** Een scoreformaat (declaratief, versioneerd), een
event/gesture-vocabulaire (hypothese), een SpectraStrobe-encoder, een renderer
en een verificatiestap — alles buiten de plugin, in een aparte map of tool. Geen
nieuwe parameter-IDs, geen APVTS, geen presetwijziging. **Wat er nog niet is:**
een echte event/geheugen-engine en een zelfstandige licht-engine met eigen
toestand. De eerste PoC dekt de signaalhelft (encoder + integriteit); de
event-engine is een volgende stap, niet iets wat al gebouwd is.

**[T] Grens/contract.** Bestandsgebaseerd en eenrichtingsverkeer: score in →
WAV + `events.csv` + timeline uit. Eén seed per render. Geen gedeeld geheugen,
geen realtime koppeling, geen plugin-ABI-wijziging. Het scoreformaat is de
enige nieuwe "API" en die blijft binnen de verkenning.

**[T] Realtime/threading.** Geen realtime-eis in v1. De renderer is
single-threaded en deterministisch; latentie is de duur van een render, niet
audiolatentie. Optioneel komt er later een apart realtime companion-proces,
maar dat is een tweede stap.

**Risico's.** Geen live-ervaring; de artistieke vraag "voelt dit in het echt"
blijft open tot de hardwaretest. Risico dat er een parallelle architectuur
groeit die later niet integreert — ondervangen door hetzelfde eventvocabulaire
(§5) als B te gebruiken. SpectraStrobe-routing in een host blijft ongetest;
daarom is de hardwareverificatie een harde poort, niet een detail. Zie
`docs/v2/SPECTRASTROBE.md` voor signaalintegriteit en toleranties.

**Te toetsen aannames.** U05, U06, U12, U14, U22 (determinisme), plus de
artistieke U02/U03.

**Verdict.** Ontwerpkeuze met het minste risico voor v1.0.4. Dit is ook
letterlijk de "suggested first experiment" uit issue #14.

### 2.B In-plugin control-rate event-seam + zelfstandige licht-engine

**[I]** Als A artistiek overtuigt, is de volgende vraag of de koppeling *in* de
plugin kan leven zonder de bestaande stabiliteit te breken.

**[T] Hergebruik.** Het volledige audiopad en de capture-ring als bron. De
bestaande control-rate van 16 samples (`EngineDSP.h:73`) kan als raster voor
een event-tap dienen: één event-frame per 16 samples (≈0,33 ms bij 48 kHz),
mits de engine dat als *extra* kanaal naast `VisualState` aanbiedt.

**[T] Wat zelfstandig wordt.** Een licht-engine met eigen toestand, scheduler,
RNG en geheugen; een encoder; een optioneel eventlog. De audio-DSP blijft
inhoudelijk ongemoeid.

**[T] Grens/contract.** Een nieuwe interne API, bijvoorbeeld
`EventSink::push(const Event&) noexcept` (audio-thread) plus
`LightEngine::render(...)`, met een vaste capaciteit en expliciete
drop-policy. **Niet** `VisualState` hergebruiken of uitbreiden. Alles achter
een build-flag (`NAGLOED_V2_EXPERIMENTAL`) en nooit op de v1.0.x-releasebranch.
SpectraStrobe-carriers kunnen niet door de bestaande keten
(`EngineDSP.cpp:600-664` bevat low-pass, saturatie, limiter); ze moeten na de
DSP worden toegevoegd of via een aparte bus/offline export. Dat is een
routingvraag, geen DSP-vraag — zie `docs/v2/SPECTRASTROBE.md`.

**[T] Realtime/threading.** Event-engine op de audio-thread of op een
lock-free worker met bounded queue; nooit blokkeren, nooit alloceren. CPU moet
binnen het bestaande budget blijven (U17). Refractaire periodes en budgets
(U18/U19) zijn hier geen optimalisatie maar een veiligheidsvoorwaarde.

**Risico's.** Aanraken van `src/plugin` en `CMakeLists.txt` is onvermijdelijk;
dat is precies wat de guardrails van issue #14 verbieden zolang v1 niet klaar
is. Host-routing van carriers is onbewezen. Een audio↔licht-lus kan ontaarden.
De v1-realtimegaranties (0 allocaties, blokinvariantie) moeten opnieuw bewezen
worden met de extra module.

**Te toetsen aannames.** U01–U04, U12, U13, U17–U21.

**Verdict.** Fase 2, alleen na een geslaagde A + hardwareverificatie. Niet in
deze verkenning bouwen.

### 2.C Losgetrokken herbruikbare core + nieuwe v2-host

**[I]** Op lange termijn is NAGLOED mogelijk niet één plugin maar een
granulaire kern met meerdere gastheren: de v1-plugin, een installatie-app, een
renderer.

**[T] Hergebruik.** `GrainEngine`, `RuimteReverb`, `AnalogChain`, `Modulation`,
`DspUtil` en de `EngineDSP`-orkestratie; de tests grotendeels. Extractie naar
een library-target (`nagl_core`) met een stabiele, host-agnostische API.

**[T] Wat zelfstandig wordt.** De core-library; daarnaast een nieuwe v2-host
(standalone of nieuwe plugin) die event-engine, licht, routing en UI bezit. v1
blijft op de releasebranch bevroren.

**[T] Grens/contract.** Library-API (`prepare/reset/setParams/process/
getVisual`) plus event-API; eigenaarschap van buffers; geen GUI-afhankelijkheid
in de core. Let op: de huidige `Engine`-signatuur gebruikt `juce::AudioBuffer`
en `juce::MidiBuffer` (`src/dsp/Engine.h:34`), dus "JUCE-vrij" is een separaat
besluit, geen automatisme.

**[T] Realtime/threading.** Zelfde discipline als v1; de nieuwe host kiest
blokgrootte en control-rate. Distributie en licensing veranderen niet
(GPL-3.0-or-later, eigen JUCE-licentie; `README.md:205-213`).

**Risico's.** Grote refactor, CMake-churn, regressierisico, dubbele QA en
onderhoud, en een tweede product met eigen release/validatie. Voor het eerste
experiment is dit overkill.

**Te toetsen aannames.** U05, U17, U20, U21.

**Verdict.** Middellange termijn, als aparte branch en nieuw target — nooit als
in-place herstructurering van v1.

### 2.D Distributed peer-model

**[I]** Kamers als gedeeltelijk autonome luisteraars: telegrammen, herinneringen
die tussen ruimtes reizen, bewuste drift (issue #14; detail in
`docs/v2/DISTRIBUTED.md`).

**[T] Hergebruik.** Het eventvocabulaire en de encoder per node; elke node
draait zijn eigen audio- en lichtwereld.

**[T] Wat zelfstandig wordt.** De peer-/netwerklaag: bounded telegrammen over
OSC, lokale klokautonomie, drop-policy, optionele gedeelde klok. Geen
netwerkaudio in fase 1.

**[T] Grens/contract.** Minimaal telegram (id, kind, tijd, parameters, seed,
cause), rate limits, vertrouwd lokaal netwerk, geen PII. De netwerkthread raakt
het audiopad nooit direct.

**Risico's.** Runaway feedback tussen kamers, klokdrift, duplicaten,
beveiliging, en een veel grotere complexiteit dan het eerste experiment
rechtvaardigt. Dit versterkt U25–U28.

**Verdict.** Horizon. Pas zinvol als het eventvocabulaire uit A/B stabiel is.
Cross-ref: `docs/v2/DISTRIBUTED.md`.

---

## 3. Aanbevolen richting voor het eerste experiment

**[T]** Kies **A**, in twee strikt gescheiden stappen, en houd v1.0.4
onaangetast:

1. **A1 — losse offline renderer, zonder v1-code.** Een declaratieve score van
   60–90 s, een eigen deterministische renderer, een SpectraStrobe-encoder en
   een verificatiestap; uitvoer 48 kHz/24-bit stereo plus een 44,1 kHz/16-bit
   variant, dim-/mid-varianten voor de hardwareladder, een program-only
   A/B-WAV, `events.csv` en een tijdlijn. Inhoudelijk minimaal: één lichtkorrel
   met beweging en abrupte extinctie, één auditieve respons met *niet-gelijke*
   envelope en bewuste vertraging, één getransformeerde terugkeer veel later, en
   minimaal één bewust onbeantwoord licht-event. **A1 dekt in deze verkenning de
   signaalhelft, niet de event-engine of het geheugenmodel:** een echte
   scheduler met budgets, refractaire periodes en `remember`/`forget` is nog
   niet gebouwd en is een volgende stap.
2. **A2 — dezelfde score door de echte v1-processor.** Alleen als A1 slaagt:
   een headless render-tool in de geest van `tools/render_main.cpp` die v1
   audio produceert, zodat hetzelfde scoremateriaal met echt NAGLOED-materiaal
   klinkt. Geen wijziging aan `src/`; wel een apart buildproject of een
   expliciet goedgekeurde CMake-uitzondering, want een nieuw target in
   `CMakeLists.txt` botst met de bevriezing. A2 hergebruikt het A1-programma
   niet letterlijk: v1 heeft andere bronnen (breedbandige noise/drone), wat de
   M7-poort juist uitdagender maakt.

De concrete invulling van A1 staat uitgewerkt in `docs/v2/EXPERIMENT-01.md`
("Eerste Gloed"); de uitvoerbare vorm hoort in `experiments/spectrastrobe-poc/`.

Harde poorten **[T]**:

- SpectraStrobe-verificatie op echte hardware (Kasina + DeepVision Ganzfeld),
  conservatief, met directe stop; zonder die meting geen integratie. Protocol
  en toleranties: `docs/v2/SPECTRASTROBE.md`.
- Deterministische rerender: byte-identieke WAV en `events.csv` bij gelijke
  seed, platform en samplerate (U22).
- Anti-runaway-scenario's moeten aantoonbaar begrensd blijven (U19). Deze poort
  is pas toetsbaar zodra er een event-engine bestaat; A1 dekt haar niet.
- Als de carriers een host-bounce niet overleven of de hardwaredetectie
  onbetrouwbaar is: **stop**, en ga niet naar B.

Pas daarna, en alleen met expliciete toestemming van Lonn: **B** achter een
build-flag, vervolgens **C** als nieuwe branch, **D** als onderzoekslijn.

---

## 4. Niet aanraken

Deze lijst is bindend voor de verkenning. Wijzigingen hieraan gaan via een
aparte, gereviewde PR buiten deze documenten om.

| Niet aanraken | Waarom | Bewijs |
|---|---|---|
| `src/core/ParamIDs.h` | Parameter-IDs zijn permanente API; automation, presets en sessies adresseren ze | `ParamIDs.h:3-4` |
| Parameterlayout, ranges, defaults, choice-tabellen | GUI, presetvalidator en DSP-adapter lezen hieruit; drift is een releasebug | `src/core/Parameters.h:24-56,84-186` |
| `presets/*.json` en het stateformaat | Sessies en pluginval hangen eraan | `PluginProcessor.cpp:88-136` |
| `Engine`/`EngineDSP`-contract en pimpl | Realtimecontract en informatiedichtheid van de DSP-laag | `Engine.h:14-44`, `EngineDSP.h:34-47` |
| `GrainEngine`-contract, 256 stemmen, 12 s ring | Bevroren muzikale en realtime-eigenschappen | `GrainEngine.h:43-47,105,150-152` |
| `VisualState`, `GrainView`, `SnapshotChannel` | Displaycontract; mag niet als eventbus worden hergebruikt | `Contracts.h:27-57`, `Snapshot.h:9-32` |
| Signaalvolgorde Sintel → Ruimte → EAR, latency 51 samples | Klank en hostcompensatie | `ARCHITECTURE.md:3-13`, `EngineDSP.cpp:600-664`, `VALIDATION.md:52-53` |
| `CMakeLists.txt`, `CMakePresets.json`, `scripts/`, `.github/` | Build- en releasepoorten; tools compileren dezelfde bronnen | `CMakeLists.txt:70-143`, `README.md:140-163` |
| `tests/`-drempels en harnesses | Bewijzen de realtime- en determinismeclaims | `tests/DspTests.cpp:699-735,1060-1119`, `tools/preview_main.cpp:235-388` |
| `Demo/`, `docs/ui-preview.png`, `docs/demo-spectrogram.png` | Release-artefacten met deterministische hero-claim | `README.md:144-168` |
| `README.md`, `docs/ARCHITECTURE.md`, `.gitignore` | Alleen additief in `docs/v2/`; geen herschrijving | taakafspraak |
| `main`, `release/1.0.4`, tag `v1.0.4` | v1 is de basis; de verkenning is een aparte branch | `docs/CONTRIBUTING.md:6-15` |
| GPL-3.0-or-later en de JUCE-licentievoorwaarden | Juridische basis van het project | `README.md:205-213` |

---

## 5. Hypothese: minimaal, begrensd event/gesture-vocabulaire

> **Status: hypothese.** Dit is geen API, geen code en geen voorstel voor
> `src/core/`. Het is een toetsbaar vocabulaire voor een offline experiment,
> bedoeld om A en later B hetzelfde te laten praten. Alle getallen zijn
> **illustratief**, geen startwaarden en geen eisen. Minstens twee alternatieven
> blijven expliciet open tot een luistertest beslist: (a) continu geheugen versus
> event-geheugen, en (b) aandachtsbudget versus waarschijnlijkheid per
> gebeurtenis.

### 5.1 Uitgangspunten

- **[I]** Relaties worden uitgedrukt als *gebeurtenissen met een oorzaak en een
  delta*, niet als continue mapping. Een licht-event "hoort bij" een
  audio-event omdat ze dezelfde oorzaak delen of omdat het ene het andere
  later echo't — niet omdat ze frame-synchroon zijn (issue #14).
- **[I]** Niet-optreden is een geldige gebeurtenis (`rest`, `ignore`), even
  belangrijk als wél reageren.
- **[T]** Het vocabulaire is bounded: vaste structuur, vaste capaciteiten,
  expliciete budgets, één seed, sampletijd. Alles wat niet past wordt geweigerd
  of gedropt, nooit stilletjes oneindig.
- **[T]** Het staat volledig los van de bestaande DSP: het vervangt
  `EngineParams` niet, breidt `VisualState` niet uit, voegt geen parameter-IDs
  toe en compileert niet mee in v1.
- **[T]** De term *gesture* botst met het bestaande merkvocaulaire (BRAND.md
  definieert "gesture" als een bewuste performance move: macro, freeze,
  scan-sweep; `docs/BRAND.md:267`). In dit vocabulaire heet het daarom
  `score_gesture`: een gesamplede, benoemde beweging, geen continue modulatie.

### 5.2 Event kinds

| Kind | Domein | Betekenis | Verplicht | Typische duur |
|---|---|---|---|---|
| `audio_grain` | audio | Eén korrel of korte korrelgroep uit een bron | bron, positie, duur, envelope | 5–500 ms |
| `light_grain` | licht | Eén lichtkorrel met kleur, helderheid, beweging, verval | RGB, helderheid, duur, positie | 20 ms–8 s |
| `score_gesture` | beide | Benoemde, begrensde beweging (freeze, scan-sweep, macro) | naam, duur, doel | 0,5–20 s |
| `rest` | — | Intentional niet-gebeuren; blokkeert events in zijn venster | start, duur | 0,5–30 s |
| `remember` | beide | Materiaal vastleggen voor later gebruik | `memoryId`, bron, levensduur | direct |
| `forget` | beide | Herinnering vrijgeven/vervallen | `memoryId` | direct |
| `echo` | beide | Later terugkeren van een event, getransformeerd | `parentId`, delay, transformatie | delay 10 ms–60 s |
| `ignore` | beide | Bewuste non-respons; beëindigt een causale keten | `causeId` | direct |

### 5.3 Eigenschappen (vaste, kleine struct)

Gemeenschappelijk: `schemaVersion`, `id` (monotone u32), `kind`, `domain`,
`t` (int64 samples in de score-samplerate), `durationSamples`, `source`
(input/capture/noise/drone/memory), `causeId` (0 = geen), `delaySamples`, `seed`
(afgeleid), `budgetClass` (`sparse`/`normal`/`dense`), `priority`.

Per domein, optioneel: `pitchSemis`, `position` (0..1 in de capture),
`pitchSpread`, `pan` (-1..1), `gain` (0..1), en voor licht `rgb` (3×0..1),
`brightness` (0..1), `eye` (-1..1), `movement` (doelpositie aan het einde),
`envelope` (vorm-id), `persistence` (0..1). Leeg = standaardwaarde. Geen
vrije-tekstvelden, geen PII, geen bestandspaden in events.

### 5.4 Scheduling, refractaire periode, budgets

- **[T]** Events worden gesorteerd op `(t, id)` en in sampletijd afgespeeld;
  de scheduler kent geen wandklok en geen hostblokken. Late events worden
  volgens een expliciete policy gedropt of naar voren geklemd, nooit
  onbeperkt ingehaald.
- **[T]** **Refractair:** per `(kind, source)` en per causale keten geldt een
  minimumafstand (startwaarde: 20 ms voor `light_grain`, 40 ms voor
  `audio_grain`, 250 ms voor `echo`). Binnen de periode wordt een event
  geweigerd, niet vertraagd.
- **[T]** **Budgets:** maximaal 8 events/s totaal in `sparse`, 32/s in
  `normal`, 128/s in `dense`; maximaal 16 gelijktijdige `light_grain`s;
  maximaal 8 herinneringen; maximaal 256 gelijktijdige audio-grains (bestaande
  pool, `GrainEngine.h:105`); een attention-budget per seconde waarin elk kind
  een vaste kost heeft (artistieke onderbouwing: "aandacht: een budget, geen
  duty cycle", `docs/v2/ARTISTIC-VISION.md` §4.2).
- **[T]** **Anti-runaway:** causale diepte ≤ 3 (audio→licht→audio telt als 2);
  `echo` van een `echo` alleen als het budget dat toelaat; `ignore` beëindigt
  de keten; geen event mag zijn eigen `causeId`-keten terugvinden; de renderer
  controleert achteraf de invarianten en faalt hard bij overschrijding.
- **[T]** **Seeded determinisme:** één `seed` in de score; elke event-RNG is
  afgeleid van `(seed, id)` met een deterministische hash, zodat de uitkomst
  niet van uitvoeringsvolgorde afhangt. Geen `std::random_device`, geen
  wandklok, vaste float-volgorde, samplerate in de score vastgelegd.

### 5.5 Hoe dit los staat van de bestaande DSP

- **[T]** `EngineParams` (`src/core/EngineParams.h:12-76`) blijft de enige
  parameteringang van de audio-engine; het vocabulaire raakt die niet.
- **[T]** `VisualState` blijft display-only; er komt geen extra veld bij
  (`src/core/Contracts.h:27-50`).
- **[T]** In A mapt de renderer events op zijn eigen synthese (of later op
  v1-automation via `tools/render_main.cpp`-patroon); in B zou een aparte
  module events consumeren. In beide gevallen verandert er niets aan de
  bestaande DSP-contracten of parameter-IDs.
- **[T]** De eerste acceptatietest is inhoudelijk, niet technisch: één gedeeld
  event met ongelijke envelopes en bewuste vertraging, één herinnerde
  terugkeer, één onbeantwoord licht-event, en een adversarieel scenario dat
  aantoont dat de budgets houden.

### 5.6 Illustratie (indicatief, geen API)

```json
{
  "schemaVersion": 1,
  "sampleRate": 48000,
  "seed": 20261105,
  "durationSamples": 3840000,
  "events": [
    { "id": 1, "kind": "light_grain", "domain": "light", "t": 480000,
      "durationSamples": 14400, "rgb": [0.9, 0.25, 0.1], "brightness": 0.35,
      "eye": -0.6, "movement": 0.8, "envelope": "sharp-extinct" },
    { "id": 2, "kind": "echo", "domain": "audio", "t": 513600,
      "parentId": 1, "delaySamples": 19200, "transform": "low-transient" },
    { "id": 3, "kind": "rest", "domain": "both", "t": 700000,
      "durationSamples": 240000 },
    { "id": 4, "kind": "ignore", "domain": "light", "t": 1180000,
      "causeId": 3 }
  ]
}
```

---

## 6. Open vragen en verwijzingen

- **[O]** Wat is de kleinst waarneembare delta waarbij audio en licht nog als
  verwant gelden, en wanneer wordt het "reactie" of "toeval"? (U03)
- **[O]** Hoe lang mag een herinnering duren voordat zij haar betekenis
  verliest, gegeven de 12 s capture-ring? (U10)
- **[O]** Overleeft een carrier een echte host-bounce, en zo niet, welke
  exportstap is artistiek acceptabel? (`docs/v2/SPECTRASTROBE.md`; U12/U13)
- **[O]** Is een in-plugin eventseam ooit nodig, of volstaat een externe
  companion voor de gewenste ervaring? (A vs. B)
- **[O]** Welke mate van netwerkverlies en drift is artistiek gewenst in plaats
  van hinderlijk? (`docs/v2/DISTRIBUTED.md`; U25–U27)

Verwante documenten: `docs/v2/TECHNICAL-UNCERTAINTIES.md`,
`docs/v2/ARTISTIC-VISION.md`, `docs/v2/EXPERIMENT-01.md`,
`docs/v2/SPECTRASTROBE.md`, `docs/v2/DISTRIBUTED.md`,
`docs/v2/SAFETY-PRIVACY.md`, `docs/v2/IDEAS.md`, issue #14.
