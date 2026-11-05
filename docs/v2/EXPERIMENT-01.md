# Experiment 01 — Eerste Gloed

> **Status:** voorstel voor één klein, afzonderlijk experiment. Geen v2-feature,
> geen integratie in de plug-in, geen wijziging aan NAGLOED 1.0.4. Het experiment
> staat volledig los van de release en is veilig te verwijderen.

## 1. De vraag

Kan **één gedeeld gebaar** een voelbare verwantschap maken tussen één
lichtgrain en één audiogebeurtenis — zonder dat ze synchroon bewegen — en is een
bewust **niet-antwoord** even overtuigend als een antwoord?

Dit is de kleinste vraag die de artistieke visie test. Ze gaat niet over
technische haalbaarheid van een heel systeem, maar over de muzikale kern:
ervaart een luisteraar verwantschap of illustratie?

## 2. Waarom dit experiment, en waarom zo klein

- Het raakt precies de relatie tussen een audiogebeurtenis en een
  lichtgebeurtenis, zoals gevraagd.
- Het kan **offline** worden gerenderd tot een WAV met SpectraStrobe-signaal.
  Er is geen plug-inwijziging, geen host-integratie en geen nieuwe
  audiopijplijn nodig.
- Het is **veilig**: geen realtime licht, geen netwerk, geen microfoon, geen
  bezoekers. De intensiteit is conservatief en de duur is kort.
- Het is **duidelijk een experiment**: alles staat in
  `experiments/spectrastrobe-poc/`, buiten de build en buiten de release.
- Het levert ook een **meetbare technische basis**: signaalintegriteit,
  helderheid, crosstalk en headroom zijn objectief te controleren voordat er
  ooit licht op iemands netvlies staat.

## 3. De compositie (72 seconden)

Werknaam: **Eerste Gloed**. Vier momenten, veel ruimte ertussen.

| Tijd | Gebeurtenis | Licht | Geluid | Relatie |
|---|---|---|---|---|
| 0–8 s | Kamer, bijna niets | Geen licht (alleen referentiedraaggolf) | Zeer schaars kamerbeeld, één ver fragment | Geen |
| 8–14 s | Lichtgrain | Amberkleurig grain start links, drijft langzaam naar rechts, wordt iets helderder | Stil | Alleen licht |
| 14,0 s | Extinctie | Licht dooft abrupt (8 ms) | Stil | — |
| 14,18 s | Antwoord | Geen nieuw licht | Lage impact, 180 ms later, andere decay (2,5 s), iets rechts | Verwantschap, geen synchronie |
| 14–28 s | Naleven | Diffuus amberrood veld blijft langzaam ademen en dooft | De impact is al weg | Licht overleeft geluid |
| 28–46 s | Terughoudendheid | Eén zwak lichtgrain om 38 s, dooft vanzelf | Stil | **Opzettelijk onbeantwoord** |
| 46–52 s | Herinnering | Dimmere, tragere echo van het eerste licht, andere timing | Omgekeerd en verlaagd fragment, 46,4 s | Herkenbaar, verplaatst |
| 52–72 s | Uitdoving | Langzame fade, geen slotakkoord | Kamer wordt stil | Stilte als einde |

De vertraging van 180 ms is een **artistieke keuze**, geen latency-bug. De
audio-envelope lijkt niet op de licht-envelope: het licht dooft in milliseconden,
het geluid heeft een lange decay. Ze zijn familie, niet kopie.

## 4. Wat het experiment technisch doet

1. **Score als data.** `score.json` beschrijft de gebeurtenissen, kleuren,
   panning, envelopes, vertragingen en stiltes. De renderer voert de score uit
   met een vaste seed, zodat een render reproduceerbaar is.
2. **Programma-audio.** Een schaars, gesynthetiseerd "gevonden" materiaal,
   bandbegrensd onder 16–17 kHz. Geen echte stemmen, geen field recording van
   personen: in dit eerste experiment is privacy geen variabele. (Voor latere
   experimenten met echte omgevingen geldt het protocol in
   [SAFETY-PRIVACY.md](SAFETY-PRIVACY.md).)
3. **SpectraStrobe-encodering.** De referentiedraaggolf (18,2 kHz) loopt continu
   met een langzame stereo-pan-LFO. De kleurkanalen (18,7 / 19,2 / 19,7 kHz)
   volgen de lichtgrains. De carriers worden **na** de audioprocessing
   toegevoegd; het programma wordt vooraf bandbegrensd.
4. **Conservatieve helderheid.** Referentie rond −30 dBFS, kleurkanalen in dit
   eerste experiment maximaal ongeveer 25% daarvan. De piek van de hele mix
   blijft onder −6 dBFS. Er is een helderheidsladder in het hardwareprotocol:
   van nauwelijks zichtbaar naar iets meer, nooit van "veilig" naar "fel".
5. **Verificatie vooraf.** `verify.py` controleert de carrierfrequenties, de
   relatieve amplitudes, crosstalk, de 20 Hz-panmodulatie, de bandbegrenzing,
   de headroom en de deterministische re-render. Pas daarna gaat er iets naar
   de hardware.
6. **Hardwareprotocol.** Een korte, gecontroleerde sessie op de echte Kasina +
   DeepVision Ganzfeld, met directe stop, observatieformulier en geen enkele
   medische of wellness-claim.

## 5. Wat we meten

### Technisch (objectief, offline)

| Meting | Criterium (voorstel) |
|---|---|
| Carrierfrequenties | binnen ±1 Hz van 18.200 / 18.700 / 19.200 / 19.700 Hz |
| Relatieve kanaalamplitude | volgt de score binnen enkele dB; helderheid ≤ 25% van referentie |
| Crosstalk tussen kleurkanalen | ≤ −40 dB |
| Referentie-panmodulatie | 20 Hz detecteerbaar, diepte consistent met de LFO |
| Programma > 17 kHz | voldoende onderdrukt ten opzichte van de carriers |
| Piek / true peak | ≤ −6 dBFS, geen clipping, geen non-finite samples |
| Determinisme | twee renders met dezelfde seed zijn identiek |
| Corruptiematrix | gedocumenteerd: wat overleeft 44,1 ↔ 48 kHz, lowpass, MP3/AAC? |

### Perceptueel (Lonn, met hardware)

Vijf vragen, gevraagd na één keer kijken/luisteren in dim licht:

1. Waren licht en geluid voor jou verwant, of leken het twee losse dingen?
2. Merkte je dat het licht en het geluid niet synchroon liepen — en stoorde dat,
   of maakte het de relatie interessanter?
3. Wat deed de stilte tussen 28 en 46 seconden?
4. Merkte je dat het lichtgrain om 38 seconden geen antwoord kreeg? Zo ja: voelde
   dat als intentie of als gemis?
5. Wat bleef er hangen: het licht, het geluid, de stilte, of niets?

Daarnaast: twee keer kijken (de tweede keer wetend wat er komt) om te horen of
herkenning de verwantschap verandert. En één keer alleen luisteren zonder
Ganzfeld, om te controleren of hetgeluid op zichzelf staat.

## 6. Succes- en faalsignalen

**Het experiment is de moeite waard als:**

- de vertraging niet als fout voelt;
- de stilte niet als "er is iets stuk" voelt;
- de late terugkeer herkenbaar is zonder meteen plaatsbaar;
- het niet-antwoord van 38 s opvalt (of juist productief onopgemerkt blijft);
- de luisteraar na afloop iets anders beschrijft dan "een effect".

**Het experiment faalt (en dat is nuttige informatie) als:**

- licht en geluid als twee losse demonstraties voelen;
- het licht te fel of te snel is, of de carriers hoorbaar/irritant zijn;
- de audio te vol is en de stilte niet kan dragen;
- de helderheid zo conservatief is dat de Ganzfeld niets laat zien;
- de hardware het signaal niet herkent (dan is het een signaalintegriteit- of
  detectieprobleem, geen artistiek probleem).

## 7. Wat dit experiment níet bewijst

- Geen realtime plug-inhaalbaarheid; dit is een offline render.
- Geen host-routing of DAW-gedrag; dat is een aparte technische vraag.
- Geen distributed werking, geen netwerk, geen meerdere kamers.
- Geen AI-component.
- Geen uitspraak over de definitieve architectuur van v2.
- Geen algemene uitspraak over "audio-visuele verwantschap"; alleen over deze
  ene, kleine situatie.

## 8. Uitvoering en bewijs

De uitvoerbare vorm staat in `experiments/spectrastrobe-poc/`:

```bash
cd experiments/spectrastrobe-poc
./run.sh
```

Dat schrijft naar `out/`: de 48 kHz/24-bit mix, de 44,1 kHz/16-bit variant, een
programma-only A/B-versie, `events.csv`, `timeline.svg` en een verificatierapport.
Het hardwareprotocol staat in `HARDWARE-PROTOCOL.md`.

## 9. Vervolg — alleen als het muzikaal iets doet

Geen v2-plan. Wel een kleine, logische volgende stap als *Eerste Gloed* iets
raakt:

1. **Tweede experiment: geheugen en verval.** Eén fragment dat drie keer
   terugkeert en elke keer meer vervormt, met een licht dat niet meebeweegt maar
   meeverandert. Vraag: wanneer wordt een herinnering sentimenteel?
2. **Derde experiment: een tweede kamer.** Twee instanties op één netwerk die
   alleen gebeurtenissen uitwisselen (geen audio), met lokale autonomie. Vraag:
   kan een herinnering tussen kamers reizen zonder dat de kamers synchroon
   lopen?
3. **Pas daarna:** nadenken over een gedeelde architectuur en een eventuele
   v2-vorm. Niet eerder.
