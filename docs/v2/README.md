# NAGLOED v2 — verkenning: Geborgen Geluid

> **Status:** artistieke en technische verkenning, **geen v2-specificatie en geen
> releaseplan**. NAGLOED 1.0.4 is en blijft de stabiele referentie. Er is niets
> gewijzigd aan de DSP-contracten, parameter-IDs, presets, build of
> releasebranch. De verkenning is op 5 november 2026 expliciet geopend door Lonn
> als research-opdracht; issue #14 blijft open en wordt niet geïmplementeerd. De
> guardrails uit dat issue blijven gelden. Alles hier is uitnodiging, hypothese
> of open vraag — geen implementatieverplichting.
>
> **Leespad:** deze README, [ARTISTIC-VISION.md](ARTISTIC-VISION.md),
> [EXPERIMENT-01.md](EXPERIMENT-01.md) en [SAFETY-PRIVACY.md](SAFETY-PRIVACY.md)
> vormen de kern. [ARCHITECTURE-OPTIONS.md](ARCHITECTURE-OPTIONS.md),
> [TECHNICAL-UNCERTAINTIES.md](TECHNICAL-UNCERTAINTIES.md),
> [SPECTRASTROBE.md](SPECTRASTROBE.md), [DISTRIBUTED.md](DISTRIBUTED.md) en
> [IDEAS.md](IDEAS.md) zijn verdiepende bijlagen, geen poort en geen backlog.

**Kern in één zin:** NAGLOED wordt geen audiovisueel instrument dat geluid
vertaalt naar licht, maar wil een omgeving worden met een eigen geheugen en
aandacht, waarin geluid, licht, machines en mensen elkaar kunnen verrassen —
ook door niets te doen.

---

## 1. Samenvatting

- **De vraag is niet welke features erbij kunnen.** De vraag is hoe NAGLOED een
  omgeving wordt waarin geluid, licht, machines en mensen elkaar werkelijk
  kunnen verrassen. Artistieke intentie staat voorop; techniek is ondergeschikt
  aan wat muzikaal de moeite waard blijkt.
- **Verwantschap, geen synesthesie.** Licht en geluid zijn verwanten met een
  gedeelde oorzaak of herinnering, niet twee kanalen van hetzelfde signaal. Ze
  mogen immediate, vertraagd, probabilistisch of helemaal niet op elkaar
  reageren.
- **Drie onderzoekslijnen.** (1) granular audiovisual composition met
  SpectraStrobe/Kasina/DeepVision Ganzfeld; (2) musical agency en generatieve
  compositie via een gebeurtenis- en geheugenmodel; (3) distributed sonic
  environments als langetermijnrichting.
- **Eerste experiment: *Eerste Gloed*.** Een offline studie van 72 seconden met
  één geschreven paar (lichtextinctie + 180 ms late audio-respons), één late
  herinnering en één onbeantwoord lichtgrain. SpectraStrobe-WAV, vooraf
  geverifieerd (52 van 53 PASS, 1 eerlijke INFO, 0 FAIL), met een conservatief
  hardwareprotocol en een eerlijke scheiding tussen "geschreven verwantschap" en
  "systeem-autonomie".
- **Architectuur: v1 onaantastbaar.** Eerste stap is een losse offline renderer
  (A1) die de signaalhelft dekt; een echte event-/geheugenengine en een in-plugin
  seam (B) zijn latere fasen met harde poorten. A2, C en D zijn bijlagen, geen
  planning.
- **Technische top-risico's.** Waargenomen verwantschap bij delta-tijden
  (U02/U03), SpectraStrobe-integriteit over host/keten (U12/U13), runaway tussen
  audio en licht (U19), de 12 s-geheugenlimiet (U10) en de decoder-helderschaal
  bij een per oor gemoduleerde referentie (U31).
- **Onafhankelijk gereviewd.** Twee reviewers (techniek; artistiek/veiligheid/
  scope) kwamen op het reviewpunt tot REQUEST CHANGES. Hun blockers zijn
  verwerkt; de restrisico's staan expliciet open in [REVIEWS.md](REVIEWS.md).
- **Geen v2-releaseplanning.** Pas als het eerste experiment muzikaal iets doet,
  volgt een tweede klein experiment. Niet eerder.

---

## 2. De artistieke beweging

NAGLOED 1.0.4 is een instrument van residu: capture, Sintel-grains, Ruimte, EAR.
De audiovisuele richting is een latere artistieke ontdekking, geen
productvereiste. *Geborgen geluid* — het kind onder de tafel op een familiefeest
— is het beeld: vertrouwd, vreemd, poreus, tijdelijk. De wereld gaat door, maar
je bent er even anders in.

De verkenning scherpt de vraag van issue #14 aan. Werkelijk nieuw zijn de
dramaturgie van het niet-antwoorden, geheugen en vergeten als compositorisch
materiaal, het aandachtsbudget in plaats van een duty cycle, en de keuze om
eerst offline te experimenteren voordat de plug-in wordt aangeraakt. De rest is
ordening en verdieping. De volledige visie staat in
[ARTISTIC-VISION.md](ARTISTIC-VISION.md); de ideeën die verder gaan dan de
opdracht in [IDEAS.md](IDEAS.md).

**Artistieke succescriteria** zijn een kompas, geen meetlat: het moment waarop
je vergeet of licht en geluid bij elkaar horen; een stilte die klopt; een
herkenbare terugkeer die je niet kunt plaatsen; contrast met gewicht; gedeelde
agency; en na afloop niet "mooi effect", maar "wat was dat". Per experiment
worden ze geoperationaliseerd in waarneembare vragen en beslissingsregels
(zie [EXPERIMENT-01.md](EXPERIMENT-01.md) §5).

---

## 3. Drie samenhangende onderzoekslijnen

### 3.1 Granular audiovisual composition

**Intentie.** Onafhankelijke audio- en lichtgrains die ontstaan, evolueren en
elkaar beïnvloeden. SpectraStrobe, Kasina en DeepVision Ganzfeld zijn de eerste
concrete omgevingen: een output-medium, niet het conceptuele model. De Ganzfeld
maakt van licht een diffuus kleurveld in plaats van een beeld van LED's.

**Stand van zaken.** Het SpectraStrobe-signaal is goed gedocumenteerd: vier
hoogfrequente dragers (18,2 kHz referentie met 20 Hz stereo-panning; 18,7/19,2/
19,7 kHz voor R/G/B), helderheid als amplitude ten opzichte van de referentie,
zes lichtkanalen (L/R per kleur). Wat ontbreekt is de decoderkarakteristiek van
de Kasina en de werkelijke signaalintegriteit over hosts, resampling en codecs.
Daarom is meten vóór integreren een harde poort. Zie
[SPECTRASTROBE.md](SPECTRASTROBE.md).

**Eerste concrete vragen.** Overleeft het signaal een 44,1 ↔ 48 kHz-pad en een
DAW-bounce? Wat is de minimale waarneembare helderheid? Hoe verhoudt
DeepVision-helderheid zich tot de standaard Ganzframes? Welke delta tussen licht
en geluid voelt nog als verwantschap? En: hoe leidt de decoder de helderheid af
uit een per oor gemoduleerde referentie (U31)?

### 3.2 Musical agency en generatieve compositie

**Intentie.** Een gebeurtenis- en geheugenmodel dat muzikale contrasten,
autonomie, toeval, herhaling, herinnering en stilte mogelijk maakt — zonder de
realtime DSP afhankelijk te maken van AI-inferentie.

**Hypothese: een klein, begrensd vocabulaire.** Acht event-kinds
(`audio_grain`, `light_grain`, `score_gesture`, `rest`, `remember`, `forget`,
`echo`, `ignore`), vaste eigenschappen, sampletijd, één seed, refractaire
periodes, aandachtsbudgetten en causale diepte ≤ 3. `rest` en `ignore` zijn
gelijkwaardige gebeurtenissen, geen afwezigheid van data. Dit vocabulaire staat
volledig los van `EngineParams`, `VisualState` en de bestaande DSP; het is een
toetsbare hypothese, geen API. De getallen zijn illustratief; alternatieven
(continu geheugen vs. event-geheugen, budget vs. waarschijnlijkheid) blijven
open tot een luistertest beslist. Uitwerking:
[ARCHITECTURE-OPTIONS.md](ARCHITECTURE-OPTIONS.md) §5.

**AI buiten de realtime loop.** Een interessant patroon is de *offstage
componist*: een script of model compileert offline een partituur; op de vloer
speelt alleen de gevalideerde, read-only partituur. Geen inferentie, geen
netwerk, geen model in de kamer. Dat maakt AI een auteur en niet een
onvoorspelbare laag in de audio-thread. Zie [IDEAS.md](IDEAS.md) #1.

**Open vragen.** Wanneer voelt een niet-reactie als intentie in plaats van bug?
Hoe lang mag een herinnering duren voordat ze sentimenteel wordt? Hoe voorkomen
we dat een gedeelde oorzaak een feedback-lus wordt? Hoe verhouden
aandachtsbudgetten zich tot muzikale spanning?

### 3.3 Distributed sonic environments

**Intentie.** Meerdere NAGLOED-instanties, computers, ruimtes en performers die
samenwerken. Wat reist is geen audio maar een telegram: een korte beschrijving
van een gebeurtenis met tijd, soort, kleur en een mate van zekerheid. Een
herinnering kan tussen ruimtes reizen, verkeerd begrepen worden of nooit
aankomen. Lokale autonomie is de default; verbinding is de uitzondering.

**Technische richting (hypothese).** Event-only over OSC/UDP, logische star met
een hub die alleen logt, begrensde queues, rate limits, ttl/confidence-verval,
geen gedeelde klok in de eerste test. Netwerkaudio (AES67/Dante/AVB) en
DMX/Art-Net/sACN zijn latere, aparte vragen. Distributie is een verre
onderzoeksrichting, geen derde stap in een planning. Detail:
[DISTRIBUTED.md](DISTRIBUTED.md).

---

## 4. Architectuur: wat hergebruiken, wat zelfstandig

### 4.1 Wat de huidige code echt is

- `VisualState` / `SnapshotChannel` is een **displaykanaal** (1× per blok,
  verlieslatend, geen tijdstempels of identiteit), **geen eventbus**. De
  bestaande `getVisual()` is alleen voor de GUI.
- De enige echte realtime-seam is de audioblokgrens plus blokgekwantiseerde
  APVTS-parameters. De interne control-rate van 16 samples is een
  rekensnelheid, geen publieke event-resolutie.
- Er is in v1 geen licht-engine, geen SpectraStrobe-encoder, geen event- of
  geheugenmodel, geen post-DSP-routing en geen netwerklaag.
- Bewezen eigenschappen om te bewaren: 0 allocaties in `process()`, 51 samples
  latency bij 48 kHz, blokgrootte-onafhankelijk binnen 2 % RMS, 12 s capture-ring,
  256 grains.

### 4.2 Vier decomposities

| Optie | Wat | Hergebruik | Zelfstandig | Verdict |
|---|---|---|---|---|
| **A** | v1 onaangetast + externe companion/offline renderer | v1 volledig; patroon van `tools/render_main.cpp` | score, renderer, encoder, verificatie; later event-/lichtengine | **Eerste stap** |
| **B** | in-plugin control-rate event-seam + lichte engine | audiopad, capture-ring | licht-engine, scheduler, geheugen, encoder | Latere fase, na A + hardware |
| **C** | losgetrokken core + nieuwe v2-host | GrainEngine, Ruimte, EAR, EngineDSP | core-library + nieuwe host | Bijlage / middellange termijn |
| **D** | distributed peer-model | eventvocabulaire en encoder per node | netwerklaag, lokale klok | Verre onderzoeksrichting |

Deze tabel is een **architectuurkaart, geen fasering of backlog**. Alleen A1 is
nu relevant; de rest is hypothese.

### 4.3 Aanbevolen richting

**A1 — losse offline renderer, zonder v1-code.** Een declaratieve score, een
eigen deterministische renderer, een SpectraStrobe-encoder en een
verificatiestap; uitvoer als 48 kHz/24-bit WAV, 44,1 kHz/16-bit variant,
dim-/mid-varianten voor de hardwareladder, programma-only A/B-WAV, `events.csv`
en tijdlijn. **A1 dekt de signaalhelft, niet de event-engine of het
geheugenmodel**: een echte scheduler met budgets, refractaire periodes en
`remember`/`forget` is nog niet gebouwd. De uitvoerbare vorm staat in
`experiments/spectrastrobe-poc/` en [EXPERIMENT-01.md](EXPERIMENT-01.md).

**A2 — dezelfde score door de echte v1-processor.** Alleen als A1 muzikaal
slaagt: een headless render-tool in de geest van `tools/render_main.cpp`, zonder
wijziging aan `src/`. Een nieuw target botst met de bevroren `CMakeLists.txt`;
dat bouwpad is een open punt (U30). A2 hergebruikt het A1-programma niet
letterlijk: v1-materiaal is breedbandiger, wat de M7-poort juist uitdagender
maakt.

**Harde poorten.** Geen integratie zonder (1) SpectraStrobe-verificatie op echte
hardware, conservatief en met directe stop; (2) byte-identieke deterministische
rerender; (3) aantoonbaar begrensde anti-runaway-scenario's — die laatste is pas
toetsbaar zodra een event-engine bestaat. Als de dragers een host-bounce niet
overleven of de hardwaredetectie onbetrouwbaar is: stoppen, en niet naar B gaan.
B pas met expliciete toestemming van Lonn; C als nieuwe branch; D als
onderzoekslijn.

### 4.4 Niet aanraken

`src/core/ParamIDs.h`, parameterlayout en -ranges, `presets/*.json`,
`Engine`/`EngineDSP`-contract, `GrainEngine`-contract (256 stemmen, 12 s),
`VisualState`/`SnapshotChannel`, signaalvolgorde en latency, `CMakeLists.txt`,
`CMakePresets.json`, `scripts/`, `.github/`, testdrempels, `Demo/` en de
release-artefacten, `main`, `release/1.0.4` en tag `v1.0.4`, en de GPL/JUCE-
licentievoorwaarden. De volledige lijst met bewijs staat in
[ARCHITECTURE-OPTIONS.md](ARCHITECTURE-OPTIONS.md) §4.

---

## 5. Technische onzekerheden

Het volledige register telt 31 aannames (U01–U31) met feit/hypothese, toets,
impact en status: [TECHNICAL-UNCERTAINTIES.md](TECHNICAL-UNCERTAINTIES.md). De
belangrijkste:

| ID | Onzekerheid | Status | Eerste toets |
|---|---|---|---|
| U02/U03 | Ontstaat verwantschap zonder synchronie, en bij welke delta? | Open | Twee renders: strak synchroon vs. gedeelde oorzaak met 80–200 ms offset; hardware-/luistertest |
| U10 | De 12 s capture-ring is te kort voor herinneringen op compositieschaal | Open | Scenariotest: fragment op t=0, terugkeer op t=90 s |
| U12/U13 | SpectraStrobe overleeft de destructieve keten en host-bounce niet; routing is onbewezen | Fase 1/2 | Corruptiematrix en hostmatrix met de analyzer uit de PoC |
| U15 | Dragers zijn "onhoorbaar" en acceptabel voor omstanders | Open | Sinustest, spectrumanalyse, comfortmeting |
| U17/U18 | Een bounded event-engine kan realtime-veilig en zonder allocaties | Later te toetsen | Prototype met allocatieteller en CPU-meting |
| U19 | Refractaire periodes en budgets voorkomen runaway tussen audio en licht | Later te toetsen | Adversariële scores, invariantencheck |
| U22 | Eén seed geeft reproduceerbare audio- én licht-renders | Deels | Byte-identieke rerender; tweede platform met tolerantie |
| U23 | AI blijft buiten de realtime loop | Open | Score-compiler-PoC; review van de datastroom |
| U30 | A2 kan zonder CMake-wijziging en met dezelfde score | Open | Buildpad uitwerken; v1-bronnen vertalen |
| U31 | De decoder-helderschaal bij een 20 Hz-panned referentie is conservatief | Open | Fotodiode-/hardwaremeting; zo nodig koppeling verlagen |

**SpectraStrobe in het kort.** De dragers leven op 18,2–19,7 kHz, dicht bij
Nyquist op 44,1 kHz. Elke resampling, DAW-lowpass, saturatie, limiter, lossy
codec, monosom of OS-audio-EQ kan ze beschadigen; de encoder moet daarom ná alle
processing staan en elk bestand moet een verificatierapport krijgen. De
decoderkarakteristiek van de Kasina is niet publiek; ook de verhouding bij 100 %
helderheid is tegenstrijdig in de bronnen. De gekozen 25 %-koppeling **per
drager** is onder beide interpretaties conservatief; de som per oor kan hoger
zijn en wordt apart bewaakt. De 17–20 kHz-programma-eis is voor het huidige,
smalbandige materiaal gehaald; voor breedbandig v1-materiaal is ze **niet**
aangetoond (eerlijk als INFO gerapporteerd). Zie
[SPECTRASTROBE.md](SPECTRASTROBE.md).

**Open technische vragen (selectie).** Wat is de decoder-passband? Resamplet de
Kasina intern tussen 44,1 en 48 kHz? Welke DAW/exportstap levert een schone,
onbewerkte stereobus? Is sample-accuraatheid artistiek nodig of is 50 ms al
"gelijktijdig"? Hoe lang mag een event- of herinneringswachtrij zijn voordat
drop-gedrag hoorbaar wordt? Hoe voorkomen we dat een echo van een echo een lus
wordt?

---

## 6. Eerste experiment — *Eerste Gloed*

**De vraag.** Kan één gedeeld gebaar — een lichtgrain dat dooft en een auditieve
respons die daar bewust niet mee samenvalt — een voelbare verwantschap maken
zonder frame-synchronie? En is een bewust niet-antwoord op een later lichtgrain
waarneembaar als compositie in plaats van als gemis of fout?

**De compositie (72 s).** Een amberkleurig lichtgrain drijft van links naar
rechts en dooft in 20 ms (het R4-minimum; nog steeds als abrupt waarneembaar).
Een lage audio-impact antwoordt 180 ms later met een andere, veel langere
envelope. Het licht leeft daarna nog even door als diffuus veld. Tussen 28 en
46 s gebeurt bijna niets; één zwak lichtgrain om 38 s blijft opzettelijk
onbeantwoord. Op 46 s keert een getransformeerde herinnering terug — omgekeerd,
verlaagd, dimmer — met een lichtecho die niet samenvalt. Het stuk eindigt in
stilte, zonder slotakkoord.

**Wat het wel en niet is.** De relaties zijn **geschreven**, niet emergent: de
componist plaatst ze, de renderer voert ze uit. De herinnering op 46 s past niet
in de 12 s-ring van v1; dat is een bewuste compositorische keuze en meteen een
open ontwerpvraag. *Eerste Gloed* test een ervaring, geen systeem-autonomie.

**Waarom dit klein en veilig is.** Offline gerenderd; geen plug-inwijziging,
geen host-integratie, geen microfoon, geen netwerk, geen bezoekers. Conservatieve
helderheid (score 20 %, harde clamp op 25 % per drager, dim-/mid-varianten),
korte duur, en een menselijke stop in het protocol. Alles staat in
`experiments/spectrastrobe-poc/`, buiten de build en buiten de release.

**Verificatie vóór hardware.** De PoC meet carrierfrequenties, relatieve
amplitudes, echte inter-drager-isolatie, de 20 Hz-panmodulatie,
programma-energie boven 17 kHz (inclusief een eerlijke breedbandprobe),
piek/true peak, clipping, veiligheidsclamp, envelopes en determinisme. Laatste
run: **52 van 53 PASS, 1 INFO, 0 FAIL**; corruptiematrix 15 rijen
(9 PASS, 5 EXPECTED-DEGRADED, 1 INFO). Daarna volgt pas het hardwareprotocol:
Kasina + DeepVision Ganzfeld, helderheidsladder **dim 6,25 % → mid 12,5 % →
volledig 20 %**, eerst 44,1 kHz/16-bit, dan 48 kHz, DeepVision pas na comfort.
Zie [SPECTRASTROBE.md](SPECTRASTROBE.md) §4 en
`experiments/spectrastrobe-poc/HARDWARE-PROTOCOL.md`.

**Wat we meten.** Technisch: de metingen hierboven met expliciete toleranties.
Perceptueel in twee fasen: fase 0 is een expliciete auteurstest zonder bewijs;
fase 1 is een vooraf geregistreerde toets met minimaal 5 niet-auteurs,
gerandomiseerde en blinde condities, drie controleversies (0 ms vertraging;
38 s-grain wél beantwoord; programma-only), open vragen vóór gerichte vragen en
vooraf vastgelegde beslissingsregels. De niet-antwoordconditie is falsifieerbaar:
als niemand het ontbrekende antwoord opmerkt, is die conditie niet geslaagd.

**Wat het niet bewijst.** Geen realtime plug-inhaalbaarheid, geen host-routing,
geen distributed werking, geen AI, geen systeem-autonomie of emergent geheugen,
geen definitieve architectuur. Hardwarevalidatie moet door Lonn gebeuren; zonder
Kasina/Ganzfeld blijft dat een open poort.

**Vervolg alleen als het muzikaal iets doet.** Eerst een tweede klein experiment
over geheugen en verval (idee #2) of over negeren (idee #3); daarna pas een
gesprek over architectuur. Geen v2-releaseplanning.

---

## 7. Veiligheid, ethiek en toestemming

Licht vlak bij en rondom het gezichtsveld is een ander risico dan een scherm.
De Ganzfeld vult het gezichtsveld en zit dicht op de ogen; de DeepVision is
extra fel. Daarom gelden harde regels, geen UI-adviezen: standaard geen
herhaalde flikkering, nooit meer dan drie flitsen per seconde, geen aanhoudende
3–30 Hz-modulatie in de kleurkanalen, kleurkanalen maximaal 25 % van de
referentie **per drager** (standaard 6,25 %), gladde envelopes (≥ 20 ms hard,
≥ 50 ms artistiek), nooit van zwart naar vol, rood extra voorzichtig, elke
sessie beginnen donker en laag, en een stop die altijd menselijk bereikbaar is.
De voorgeschreven 20 Hz-referentiepanning is geen zichtbare kleurmodulatie, maar
blijft als open aanname geregistreerd. Een echte software-panic van ≤ 50 ms is
een eis voor een latere realtime fase, niet voor de offline PoC. Geen
"veilig voor iedereen"-claim. Zie [SAFETY-PRIVACY.md](SAFETY-PRIVACY.md) §3–4.

Voor bezoekersinteractie gelden toestemming, privacy en tijdelijkheid:
opgevangen geluid is in principe vluchtig, bewaren is een bewuste keuze, geen
cloud en geen profilering, en omstanders zijn niet automatisch deelnemers. Een
fysiek teken (zoals een kaart op tafel) kan van luisteren een afgesproken
toestand maken. Distributed telegrammen bevatten geen audio en geen PII; de
netwerkmodus staat standaard uit en werkt alleen op een vertrouwd lokaal
netwerk. Geen medische, therapeutische of wellness-claims, ook niet in
presetnamen of UI. De dragers zijn niet gegarandeerd onhoorbaar (U15); speel op
een vast, genoteerd niveau en doe een korte comfortbeluistering vóór de sessie.

---

## 8. Onafhankelijke review

De verkenning is op reviewpunt commit `0f6422e` onafhankelijk gereviewd door
twee reviewers die niet aan de inhoud hebben meegewerkt: één op technische
haalbaarheid en signaalintegriteit, één op artistieke intentie, veiligheid en
scope. Beide kwamen tot **REQUEST CHANGES**. Hun blockers — de
veiligheids-/verificatieclaims (clamp, envelopes, stop, ladder, meetdefinities)
en het perceptuele ontwerp (N=1, cueing, niet-falsifieerbaar) — zijn verwerkt.
Na een gerichte hercontrole op commit `6c9e831` gaven **beide reviewers
APPROVE WITH COMMENTS**; hun addenda staan in de reviewbestanden. De
restrisico's staan expliciet open: hardware M11–M13, decoder-helderschaal,
host-/DAW-keten, breedbandige M7 voor A2, de nog niet uitgevoerde perceptuele
fase 1 en cross-platform determinisme. Zie [REVIEWS.md](REVIEWS.md) voor de
volledige verwerking en de originele reviewbestanden in
[reviews/](reviews/).

---

## 9. Wat we niet doen, en wat nu voorligt

**Expliciet niet.** Geen wijziging aan NAGLOED 1.0.4 of zijn DSP-contracten;
geen nieuwe parameter-IDs, presets of UI; geen integratie in de releasebranch;
geen v2-releaseplanning of backlog; geen AI-inferentie in de realtime DSP; geen
always-on ambient passthrough; geen kleurenorgel of één-op-één mapping; geen
therapie-/meditatieclaims; geen netwerk-, DMX- of netwerkaudiocode in de
plug-in; geen cloud.

**Wat nu voorligt.** (1) Lonn beoordeelt deze verkenning, de reviews en de PoC.
(2) Als de artistieke richting klopt: de hardwareverificatie met Kasina +
DeepVision Ganzfeld, conservatief en met directe stop. (3) Alleen bij een groen
signaal en een muzikaal overtuigend eerste experiment: een tweede klein
experiment, en later misschien A2. Geen grootschalige bouw voordat er iets is
ontdekt dat muzikaal de moeite waard is.

---

## 10. Begrippen

- **gebeurtenis** — een benoemd moment in de compositie (audio of licht), met
  tijd, duur en oorzaak.
- **gebaar / paar** — één gedeelde oorzaak die zich in twee domeinen uit; in
  *Eerste Gloed* de extinctie plus de 180 ms late impact.
- **lichtgrain** — een zelfstandige lichtgebeurtenis met kleur, helderheid,
  duur, beweging en positie.
- **niet-antwoord / `ignore` / `rest`** — een bewuste non-respons; een geldige
  gebeurtenis, geen ontbrekende data.
- **herinnering / `remember` / `forget`** — materiaal dat later terugkeert,
  vervormd of niet; vergeten is een actief proces.
- **aandachtsbudget** — een begrensde hoeveelheid interventies per tijdseenheid;
  een alternatief voor een vaste kans per gebeurtenis.
- **verwantschap** — een voelbare relatie tussen licht en geluid zonder
  frame-synchronie of één-op-één mapping.

---

## 11. Documenten en bronnen

| Document | Inhoud |
|---|---|
| [ARTISTIC-VISION.md](ARTISTIC-VISION.md) | Aangescherpte artistieke visie, succescriteria, open artistieke vragen |
| [EXPERIMENT-01.md](EXPERIMENT-01.md) | *Eerste Gloed*: vraag, score, metingen, perceptuele fasen, succescriteria |
| [SAFETY-PRIVACY.md](SAFETY-PRIVACY.md) | Fotosensibiliteit, harde regels R1–R12, stop, toestemming, privacy, retentie |
| [SPECTRASTROBE.md](SPECTRASTROBE.md) | Signaalmodel, integriteitsmatrix, meetplan M1–M14, hardwareprotocol, encoder-eisen |
| [ARCHITECTURE-OPTIONS.md](ARCHITECTURE-OPTIONS.md) | Huidige architectuur met bewijs, decomposities A–D, eventvocabulaire, niet-aanrakenlijst |
| [TECHNICAL-UNCERTAINTIES.md](TECHNICAL-UNCERTAINTIES.md) | 31 aannames U01–U31 met toets en status |
| [DISTRIBUTED.md](DISTRIBUTED.md) | Telegrammen tussen ruimtes, topologie, tijd, faalmodi, security |
| [IDEAS.md](IDEAS.md) | Tien mogelijkheden buiten de opdracht, elk met kleinste test |
| [REVIEWS.md](REVIEWS.md) | Reviewpunt, bevindingen, verwerking en restrisico's |
| [reviews/](reviews/) | Originele onafhankelijke reviews A en B |
| `experiments/spectrastrobe-poc/` | Uitvoerbare offline PoC, verificatie en hardwareprotocol |
| Issue [#14](https://github.com/leonvanbokhorst/nagloed/issues/14) | Oorspronkelijke artistieke richting en guardrails |

Belangrijkste externe bronnen: [Lumasonic SpectraStrobe-specificatie](https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/03-format-specification.md),
[productie/mixnotities](https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/05-production-and-mixing.md),
[SpiraStrober](https://github.com/Syluminose/SpiraStrober),
[OSC 1.0](https://ccrma.stanford.edu/groups/osc/spec-1_0.html),
[Art-Net 4](https://art-net.org.uk/downloads/art-net.pdf),
[AES67-2018 PICS](https://aes.org/wp-content/uploads/2024/05/AES67-2018-PICS-Summary_Merging-Technologies_HORUS_3.9_Audio-interface-to-and-from-AoIP-for-Recording.pdf),
[W3C WCAG 2.3.1](https://www.w3.org/WAI/WCAG22/Understanding/three-flashes-or-below-threshold.html),
[Epilepsy Society](https://epilepsysociety.org.uk/sites/default/files/2020-08/PhotosensitiveepilepsyNovember2016.pdf).
