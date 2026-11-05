# Experiment 01 — Eerste Gloed

> **Status:** voorstel voor één klein, afzonderlijk experiment. Geen v2-feature,
> geen integratie in de plug-in, geen wijziging aan NAGLOED 1.0.4. Het experiment
> staat volledig los van de release en is veilig te verwijderen. Het is in de
> eerste plaats een **materiaalverkenning**: het test of een *geschreven*
> verwantschap tussen één lichtgebaar en één audiogebaar voelbaar is. Het test
> niet of een systeem zelf kan weigeren, herinneren of vergeten.

## 1. De vraag

Kan **één gedeeld gebaar** — een lichtgrain dat dooft en een auditieve respons
die daar bewust niet mee samenvalt — een voelbare verwantschap maken zonder
frame-synchronie? En is een bewust **niet-antwoord** op een later lichtgrain
waarneembaar als compositie in plaats van als gemis of fout?

Twee deelvragen, die apart moeten worden beantwoord:

1. **Verwantschap:** ervaart een luisteraar/kijker een relatie tussen het
   lichtgebaar en de 180 ms latere audio-impact, zonder dat die synchroon lopen?
2. **Niet-antwoord:** merkt een luisteraar/kijker dat het lichtgrain van 38 s
   geen antwoord krijgt, en leest hij dat als intentie, als gemis of als niets?

## 2. Wat dit experiment wel en niet is

**Wel:** een offline, reproduceerbare compositie met een declaratieve score, een
eigen renderer, een SpectraStrobe-encoder en een verificatierapport. De relaties
zijn **geschreven**, niet emergent: de componist plaatst de gebeurtenissen, de
renderer voert ze uit. Dat is bewust — de eerste vraag is of de *ervaring* van
verwantschap werkt, niet of een agent zelf beslissingen neemt.

**Niet:** een bewijs van systeem-autonomie, aandacht of geheugen. De
"herinnering" op 46 s is een gesynthetiseerde transformatie van de impact op
14,18 s; de 12 s capture-ring van v1 kan dat materiaal niet bevatten. Als dit
experiment muzikaal slaagt, is de volgende vraag juist of dat soort geheugen
buiten de ring nodig is — een ontwerpvraag, geen uitgemaakte zaak. "Eén gedeeld
gebaar" betekent hier: **één paar** (lichtextinctie + audio-impact), geen
continue koppeling.

## 3. De compositie (72 seconden)

Werknaam: **Eerste Gloed**. Vier momenten, veel ruimte ertussen.

| Tijd | Gebeurtenis | Licht | Geluid | Relatie |
|---|---|---|---|---|
| 0–8 s | Kamer, bijna niets | Geen licht (alleen referentiedraaggolf) | Zeer schaars kamerbeeld, één ver fragment | Geen |
| 8–14 s | Lichtgrain | Amberkleurig grain start links, drijft langzaam naar rechts, wordt iets helderder | Stil | Alleen licht |
| 14,0 s | Extinctie | Licht dooft in **20 ms** (R4-minimum) | Stil | — |
| 14,18 s | Antwoord | Geen nieuw licht | Lage impact, 180 ms later, andere decay (2,5 s), iets rechts | Verwantschap, geen synchronie |
| 14–28 s | Naleven | Diffuus amberrood veld blijft langzaam ademen en dooft | De impact is al weg | Licht overleeft geluid |
| 28–46 s | Terughoudendheid | Eén zwak lichtgrain om 38 s, dooft vanzelf | Stil | **Opzettelijk onbeantwoord** |
| 46–52 s | Herinnering | Dimmere, tragere echo van het eerste licht, andere timing | Omgekeerd en verlaagd fragment, 46,4 s | Herkenbaar, verplaatst |
| 52–72 s | Uitdoving | Langzame fade, geen slotakkoord | Kamer wordt stil | Stilte als einde |

De vertraging van 180 ms is een **artistieke keuze**, geen latency-bug. De
audio-envelope lijkt niet op de licht-envelope: het licht dooft in 20 ms, het
geluid heeft een lange decay. Ze zijn familie, niet kopie. De extinctie is
verlengd van 8 ms naar 20 ms zodat ze de harde veiligheidsregel R4 respecteert;
20 ms is nog steeds als abrupt waarneembaar. De start van het tweede lichtveld
krijgt een fade-in van 50 ms (R5: geen sprong van zwart naar vol).

## 4. Technische uitvoering

1. **Score als data.** `score.json` beschrijft gebeurtenissen, kleuren, panning,
   envelopes, vertragingen en stiltes. De renderer voert de score uit met een
   vaste seed; een render is reproduceerbaar.
2. **Programma-audio.** Schaars, gesynthetiseerd "gevonden" materiaal,
   bandbegrensd onder 15,5 kHz. Geen echte stemmen, geen field recording van
   personen: in dit eerste experiment is privacy geen variabele. (Voor latere
   experimenten met echte omgevingen geldt [SAFETY-PRIVACY.md](SAFETY-PRIVACY.md).)
3. **SpectraStrobe-encodering.** De referentiedraaggolf (18,2 kHz) loopt continu
   met een 20 Hz stereo-pan-LFO. De kleurkanalen (18,7 / 19,2 / 19,7 kHz) volgen
   de lichtgrains. De carriers worden **na** de audioprocessing toegevoegd; het
   programma wordt vooraf bandbegrensd.
4. **Conservatieve helderheid en veiligheidsbegrenzing.** Referentie −30 dBFS,
   kleurkanalen per drager maximaal 25 % daarvan (score: 20 %). De encoder
   clampt een score die meer vraagt, valideert de minimale attack/release
   (≥ 20 ms), weigert stappen en begrenst het aantal helderheidsovergangen per
   seconde. Voor de hardwareladder zijn er extra varianten: **dim** (max 6,25 %)
   en **mid** (12,5 %). De piek van de hele mix blijft onder −6 dBFS.
5. **Verificatie vooraf.** De PoC controleert carrierfrequenties, relatieve
   amplitudes, isolatie tussen de kleuren, de 20 Hz-panmodulatie, de
   bandbegrenzing, de headroom, de veiligheidsclamp, de envelopes en de
   deterministische re-render. De bindende toleranties staan in
   [SPECTRASTROBE.md](SPECTRASTROBE.md) §3; dit document noemt geen losse,
   ruimere criteria. Laatste run: **52/52 kernchecks PASS, 1 INFO, 0 FAIL**;
   de INFO is de eerlijke breedbandprobe voor M7 (de −60 dBFS-eis is voor
   breedbandig v1-materiaal niet aangetoond). Pas daarna gaat er iets naar de
   hardware. Zie `experiments/spectrastrobe-poc/out/verify-report.md`.

## 5. Wat we meten

### Technisch (objectief, offline)

De bindende metingen M1–M10 en de adviserende corruptiematrix M14 staan in
[SPECTRASTROBE.md](SPECTRASTROBE.md) §3. De uitvoer en het laatste rapport staan
in `experiments/spectrastrobe-poc/out/verify-report.md`. De PoC bewijst
signaalintegriteit op deze machine; hardwaredetectie (M11–M13) is niet gemeten.

### Perceptueel — gefaseerd, met een expliciet onderscheid

De perceptuele toets kent twee fasen. Fase 0 is een auteurstest en levert
**geen bewijs**. Fase 1 is een vooraf geregistreerde toets met onafhankelijke
deelnemers.

#### Fase 0 — auteurstest (Lonn, verkennend)

Lonn kijkt/luistert één keer, zonder vooraf de timing te hoeven weten, en noteert
achteraf in eigen woorden wat opviel. Daarna pas de gerichte vragen. Dit is
materiaalverkenning: één persoon, geen blindering, geen bewijs. De uitkomst mag
de compositie bijsturen, niet de conclusie dragen.

#### Fase 1 — vooraf geregistreerde toets (onafhankelijke deelnemers)

Deze fase wordt **pas uitgevoerd als de hardware detection groen is** en als de
controleversies bestaan. Ze wordt vooraf vastgelegd:

- **Deelnemers:** minimaal 5, niet de auteur, geen familie of directe
  medewerker. Vrijwillig, geïnformeerd, geen (vermoede) fotosensitiviteit;
  gehoorniveau en leeftijd genoteerd. kinderen alleen met begeleiding en alleen
  bij een lagere helderheid.
- **Condities, in gerandomiseerde volgorde en blind voor de deelnemer:**
  A = de score zoals ze is; B = dezelfde score met 0 ms vertraging (A1 op
  14,00 s); C = dezelfde score waarin het lichtgrain van 38 s wél een antwoord
  krijgt; D = programma-only, zonder dragers/licht. B en C zijn
  **controleversies** die nog gerenderd moeten worden; zonder die controles is
  fase 1 niet geldig en wordt ze niet uitgevoerd.
- **Procedure:** eerst een open vraag ("Wat merkte je op?"), dan de gerichte
  items. De deelnemer krijgt vooraf geen uitleg over de 38 s-gebeurtenis of over
  de verwachte relatie. De observator noteert letterlijk.
- **Analyse:** per conditie en per vraag worden antwoorden onafhankelijk
  gecodeerd door twee codeerders. Minstens één codeur is **blind voor de
  conditie** en codeert op geanonimiseerde fragmenten; Lonn is auteur en codeert
  niet als enige. De codering en de beslissingsregels worden vóór de sessie
  vastgelegd. Geen statistische claim bij N=5; wel een duidelijke richting en
  een lijst met tegenvoorbeelden.

#### De vragen (open eerst, dan gericht)

1. "Wat merkte je op?" (open, zonder ontwerpverklap)
2. "Heb je een verband ervaren tussen wat je zag en wat je hoorde? Beschrijf het
   in je eigen woorden." (open; geen dichotomie)
3. "Was er een moment waarop je iets verwachtte dat niet kwam? Wat deed dat?"
   (open)
4. "Hoe ervoer je de lange stilte?" (open)
5. "Wat bleef er hangen?" (open)
6. Pas daarna: "Het lichtgrain rond 38 seconden kreeg geen antwoord. Heb je dat
   opgemerkt? Zo ja: hoe voelde dat?" (gericht; design wordt nu pas onthuld)
7. "Zou je dit aan iemand anders willen laten zien? Waarom wel/niet?" (open)

#### Beslissingsregels (voorstel, vooraf vast te leggen)

- **Verwantschap:** A wordt vaker als "verwant" beschreven dan B (0 ms) en D
  (programma-only). Als A niet boven B uitkomt, voegt de vertraging artistiek
  niets toe; als A niet boven D uitkomt, voegt het licht niets toe.
- **Niet-antwoord:** minstens 2 van de 5 deelnemers noemen het ontbrekende
  antwoord bij vraag 3 of 6. Als niemand het opmerkt, is de niet-antwoordconditie
  voor deze uitvoering **niet geslaagd**; dat is een uitkomst, geen succes.
  Wordt het door niemand als intentie gelezen en door meer dan de helft als
  fout/gemis, dan is de dramaturgie niet geslaagd.
- **Stilte:** de stilte mag niet door meer dan de helft als "storing" of
  "leegte" worden beschreven. Anders is de terughoudendheid te lang of te
  onduidelijk.
- **Geen demo-gevoel:** de vraag "wat bleef hangen" levert geen antwoord dat
  uitsluitend uit effect-woorden bestaat ("mooi licht", "vette reverb"). Dit is
  een kwalitatieve check, geen statistiek.

Deze regels zijn strenger dan "voelt goed": ze kunnen ook **falen**. Dat is de
bedoeling.

## 6. Hardwareprotocol en veiligheid

Het conservatieve protocol staat in
`experiments/spectrastrobe-poc/HARDWARE-PROTOCOL.md` en de bindende eisen in
[SPECTRASTROBE.md](SPECTRASTROBE.md) §4 en [SAFETY-PRIVACY.md](SAFETY-PRIVACY.md).
Kort: eerst donker en laag, Kasina + standaard Ganzframes, 44,1 kHz/16-bit,
helderheidsladder **dim (6,25 %) → mid (12,5 %) → volledig (20 %)**, daarna pas
48 kHz en DeepVision. Primaire stop is menselijk: weergave stoppen,
Kasina-helderheid naar minimum, bril af, of één oog bedekken. De offline PoC
heeft geen bewezen software-panic van 50 ms en claimt die niet. Eén persoon
bedient, één persoon kijkt; geen andere flikkerbronnen; geen mensen met
(vermoede) fotosensitiviteit; gehoorniveau en toestemming vooraf; geen
medische of wellness-claims.

## 7. Succes- en faalsignalen

**Succes (na fase 1, niet na fase 0):**

- de vertraging voelt niet als fout, en A wordt vaker als verwant beschreven
  dan B en D;
- de stilte wordt niet door de meerderheid als storing ervaren;
- de late terugkeer is herkenbaar zonder meteen plaatsbaar te zijn;
- het niet-antwoord wordt door minstens 2 van de 5 opgemerkt en vaker als
  intentie dan als gemis gelezen;
- de luisteraar beschrijft iets anders dan "een effect".

**Falen (nuttige informatie):**

- licht en geluid voelen als twee losse demonstraties;
- de vertraging voelt als fout of als willekeur;
- het licht is te fel, te snel of de carriers zijn hoorbaar/irritant;
- de audio is te vol en de stilte kan niet dragen;
- de helderheid is zo conservatief dat de Ganzfeld niets laat zien (dan is het
  een hardware-/helderheidsprobleem, geen artistiek probleem);
- niemand merkt het niet-antwoord op, of de meerderheid leest het als bug;
- de hardware herkent het signaal niet (signaalintegriteit- of detectieprobleem,
  geen artistiek probleem).

## 8. Wat dit experiment niet bewijst

- Geen realtime plug-inhaalbaarheid; dit is een offline render.
- Geen host-routing of DAW-gedrag; dat is een aparte technische vraag.
- Geen distributed werking, geen netwerk, geen meerdere kamers.
- Geen AI-component.
- Geen systeem-autonomie of emergent geheugen; de relaties zijn geschreven.
- Geen uitspraak over de definitieve architectuur van v2.
- Geen algemene uitspraak over "audio-visuele verwantschap"; alleen over deze
  ene, kleine situatie.

## 9. Uitvoering en bewijs

De uitvoerbare vorm staat in `experiments/spectrastrobe-poc/`:

```bash
cd experiments/spectrastrobe-poc
./run.sh
```

Dat schrijft naar `out/`: de 48 kHz/24-bit mix, de 44,1 kHz/16-bit variant, de
dim- en mid-varianten, een programma-only A/B-versie, `events.csv`,
`timeline.svg` en een verificatierapport. Het hardwareprotocol staat in
`HARDWARE-PROTOCOL.md`.

## 10. Vervolg — alleen als het muzikaal iets doet

Geen v2-plan en geen backlog. Als *Eerste Gloed* iets raakt, zijn er twee
kleine, losstaande vervolgvragen denkbaar:

1. **Geheugen en verval** (idee #2 uit [IDEAS.md](IDEAS.md)): één fragment dat
   drie keer terugkeert en elke keer meer vervormt, met een licht dat niet
   meebeweegt maar meeverandert. Vraag: wanneer wordt een herinnering
   sentimenteel?
2. **De kunst van het negeren** (idee #3): tien lichtgebeurtenissen, acht
   genegeerd versus vier genegeerd, met een vooraf vastgelegde paired-toets.
   Vraag: voelt terughoudendheid als intentie of als gemis?

Pas daarna, en alleen met expliciete toestemming van Lonn, is een gedeelde
architectuur aan de orde. Niet eerder.
