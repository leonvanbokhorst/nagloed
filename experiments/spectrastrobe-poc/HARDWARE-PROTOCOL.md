# Hardwareprotocol — Eerste Gloed (SpectraStrobe PoC)

> **EXPERIMENT — geen deel van NAGLOED v1.0.4.**
> Dit protocol hoort bij `experiments/spectrastrobe-poc/`. Het is een
> conservatieve, menselijke verificatie van het signaal op een echte MindPlace
> Kasina + DeepVision Ganzfeld. **Geen medische, therapeutische of
> wellness-claims.** Het doel is vaststellen of het signaal wordt herkend en of
> het gebaar artistiek iets doet — niets meer.
>
> Bindend: `docs/v2/SAFETY-PRIVACY.md` (R1–R12) en
> `docs/v2/SPECTRASTROBE.md` §4 (M11–M13). Dit document is de korte,
> uitvoerbare variant voor *Eerste Gloed*.

## 1. Vooraf — veiligheid en toestemming

- **Geen persoon met (vermoede) fotosensitiviteit of epilepsie in de ruimte.**
  Bij twijfel: niet doen. Kinderen/jongeren alleen met begeleiding én
  toestemming van een volwassene (R9).
- **Toestemming en mondelinge instructie vooraf:** wat je gaat zien, dat het
  licht fel kan zijn, dat je altijd mag stoppen, hoe je de bril afzet.
  Geen gebruik door mensen die niet kunnen of willen instemmen (slapend, onder
  invloed, kinderen zonder begeleiding).
- **Eén persoon bedient, één persoon kijkt.** De bediener kijkt niet mee in de
  bril en houdt de stop binnen handbereik.
- Ruimte verlicht, geen andere flikkerbronnen (spotlights, beamers, discolicht)
  in hetzelfde gezichtsveld.
- **Eén oog bedekken (R11)** is een geldige noodmaatregel en dempt het effect
  sterk; de bediener moet dat kunnen instrueren.
- **Stopmiddelen binnen handbereik:** weergave stopzetten, Kasina-helderheid
  naar minimum, stekker/voeding, bril af. Stop is altijd toegestaan zonder
  uitleg; "dit is genoeg" is een geldige reden.
- Bij ongemak, hoofdpijn, duizeligheid, onverklaarbare flits of detectieverlies:
  **direct stoppen** en noteren. Dat is informatie, geen mislukking.
- **Geen software-panic-claim.** Deze offline WAV heeft **geen encoder-panic en
  geen geteste 50 ms-softwareramp**. De primaire stop is menselijk: weergave
  stoppen + Kasina-helderheid laag + bril af. (Een realtime panic hoort bij
  fase B; zie `docs/v2/SPECTRASTROBE.md` §6.)

## 2. Gehoor en niveau — vóór de sessie

- De dragers (18,2–19,7 kHz) zijn **niet gegarandeerd onhoorbaar**; dat hangt af
  van leeftijd, gehoor, transducers en weergaveniveau (open aanname **U15**).
- Kies **één vast, genoteerd playbackniveau** (niet "zo hard als nodig om het
  programma te horen"). Noteer het in het formulier.
- **Comfortbeluistering (10 s, dragers-only)** op het bedoelde niveau:
  genereer daarvoor een dragers-only fragment in een aparte map:
  `python3 render.py --carriers-only --colour-scale 0.3125 --out-dir out/carriers-only`.
  Speel 10 s op het bedoelde niveau; dit bestand bevat alleen de vier dragers,
  geen programma. Vraag naar hoorbaarheid, scherpte of ongemak. Bij tinnitus,
  hyperacusis, jonge oren of twijfel: lager niveau of niet deelnemen. Dit is een
  comfort- en gehoorspreventiemaatregel, geen medische test.

## 3. Materiaal

- Mac met deze map; `./run.sh` moet groen zijn (exitcode 0) vóór de sessie.
- Kopieer naar een microSD-kaart, **zonder metadata**, korte bestandsnamen:
  - `spectrastrobe-dim-44k1-16bit.wav` — **6,25% max**: eerste exposure
  - `spectrastrobe-mid-44k1-16bit.wav` — **12,5% max**
  - `spectrastrobe-44k1-16bit.wav` — **20% max** (volledige score)
  - `program-only-44k1-16bit.wav` — A/B zonder dragers
  - `spectrastrobe-48k-24bit.wav` — **niet op de Kasina geverifieerd**; alleen
    als de firmware 24-bit/48 kHz aankan (onbekend, open vraag 8 in
    `docs/v2/SPECTRASTROBE.md`)
- Kasina met actuele firmware. LED-helderheid op een **vaste, genoteerde
  stand** — niet "max". Audio- en LED-helderheid zijn aparte knoppen.
- Standaard Ganzframes (ogen dicht, eerste stappen) en DeepVision Ganzframes
  (ogen open, diffuser, feller). DeepVision pas na een comfortabele ladder.

## 4. Conservatieve helderheidsladder (niet overslaan)

Elke trede: **10–15 s**, daarna 5 s donker. Bij twijfel: terug naar 0 en stoppen.

| Trede | Bestand | Max kleur | Doel |
|---|---|---|---|
| T0 | `spectrastrobe-dim-44k1-16bit.wav` | 6,25% | **Detectie**: Kasina-display moet `Mode SpectraStr/AS` tonen met SpectraStr actief. Speel eventueel alleen de eerste seconden (referentie-only). Geen SpectraStr? Stop: formaat-/panningprobleem. |
| T1 | `spectrastrobe-dim-44k1-16bit.wav` | 6,25% | **Eerste exposure**: eerste 15 s (lichtgrain + 20 ms-extinctie + 180 ms-impact) op lage Kasina-helderheid. |
| T2 | `spectrastrobe-mid-44k1-16bit.wav` | 12,5% | **Midden**: volledige 72 s op dezelfde Kasina-stand. |
| T3 | `spectrastrobe-44k1-16bit.wav` | 20% | **Volledig**: volledige 72 s op dezelfde Kasina-stand. |
| T4 | `spectrastrobe-48k-24bit.wav` | 20% | **48 kHz/24-bit**: alleen als T0–T3 comfortabel waren én de firmware het formaat aankan. Verschil in detectie/helderheid is een bevinding. |
| T5 | `spectrastrobe-44k1-16bit.wav` | 20% | **DeepVision, ogen open**, op **lagere** Kasina-helderheid dan T3 (DeepVision komt veel feller over). Alleen na T0–T4. |
| T6 | `program-only-44k1-16bit.wav` | n.v.t. | **A/B zonder Ganzfeld**: alleen luisteren. Staat het geluid op zichzelf? Draagt de stilte? |

Daarna optioneel, als aparte fase-0-observatie (geen bewijs): een **tweede keer
kijken** (wetend wat er komt) om te horen of herkenning de verwantschap
verandert. Doe dit niet als extra technische trede en niet binnen de fase-1-toets.

## 5. Observatieformulier

Vul dit in tijdens of direct na elke trede. Kort mag.

| Vraag | T0 | T1 | T2 | T3 | T4 | T5 | T6 |
|---|---|---|---|---|---|---|---|
| Detectie (SpectraStr ja/nee) | | | | | | | n.v.t. |
| Zichtbare kleur(en) | | | | | | | n.v.t. |
| Ongewenste kanalen / flikkering | | | | | | | n.v.t. |
| Beweging links→rechts herkenbaar | | | | | | | n.v.t. |
| Abrupte extinctie (20 ms) gezien | | | | | | | n.v.t. |
| Impact als antwoord ervaren | | | | | | | n.v.t. |
| Stilte 28–46 s gedragen | | | | | | | |
| Late echo herkenbaar | | | | | | | n.v.t. |
| Eén oog bedekt getest (R11) | | | | | | | n.v.t. |
| Comfort (1–5) | | | | | | | |
| Opmerkingen | | | | | | | |

Deze tabel is voor de **technische ladder** en de fase-0-auteurstest. Er staat
bewust **geen** vraag in over het onbeantwoorde grain: dat zou de waarnemer
vooraf cueën. Deel de 38 s-gebeurtenis ook niet vooraf.

Vast te leggen context: firmwareversie, Kasina-helderheidsstand, playbackniveau,
sample rate, Ganzframe-type, volgorde van de treden, datum. Zonder die context
is de observatie niet reproduceerbaar.

**Artistieke observatie, fase 0 (auteurstest, verkennend, geen bewijs).** Stel
achteraf eerst één open vraag: "Wat merkte je op?" Noteer het antwoord letterlijk
en in eigen woorden. Pas daarna mogen de gerichte vragen uit
`docs/v2/EXPERIMENT-01.md` §5 worden gesteld. Deze fase is één persoon (Lonn),
niet geblindeerd en geen bewijs; ze mag de compositie bijsturen.

**Fase 1 (vooraf geregistreerd, onafhankelijke deelnemers)** gebruikt een apart
formulier met open-eerst-vragen, gerandomiseerde/blinde condities en
beslissingsregels uit `docs/v2/EXPERIMENT-01.md` §5. Die fase wordt pas
uitgevoerd als de controleversies zijn gerenderd en het formulier vooraf is
vastgelegd. Ze hoort **niet** in deze eerste hardware-sessie.

## 6. Stop- en acceptatiecriteria

- **Doorgaan** alleen als T0 detectie geeft en T1–T3 zonder ongewenste kanalen,
  flikkering of ongemak verlopen. T4 (48k) en T5 (DeepVision) pas daarna.
- **Stoppen en rapporteren** bij: geen detectie, zichtbare flikkering buiten de
  bedoelde kleurovergang, ongemak/hoofdpijn, of een onverklaarbare flits.
- **Nooit** de helderheid opvoeren om een detectieprobleem te maskeren.
  Eerst signaalintegriteit (de offline check), dan pas hardware.
- **Geen conclusies over therapie, ontspanning of gezondheid.** Alleen:
  werkte de detectie, hoe zag het licht eruit, hoe voelde het gebaar.

## 7. Technische controle na de sessie

- M11: 120 s onafgebroken SpectraStr-detectie op het Kasina-display (herhaal
  met dim/mid/full en noteer eventuele uitval).
- M12 (optioneel, fotodiode + audio-interface): geen periodieke
  luminantiemodulatie 3–30 Hz met ≥ 10% diepte; helderheid monotoon met de
  ladder. Zonder fotodiode: alleen visueel, expliciet als "niet objectief"
  noteren.
- M13: stop-latentie — meet hoe snel de dragers/LED's uit zijn na de
  **menselijke** stopactie (weergave stoppen + Kasina-helderheid laag). Er is
  geen software-panic om te meten; noteer dat expliciet.
- Vergelijk de observaties met `out/verify-report.md` (offline cijfers) en met
  de corruptiematrix (wat overleeft weergave/transport).

## 8. Als de hardware niets herkent

Dat is een **signaalintegriteit- of detectieprobleem**, geen artistiek falen.
Controleer in deze volgorde:

1. Speelt de juiste WAV (geen 44,1-bestand op 48 kHz of omgekeerd)?
2. Is de referentie echt continu aanwezig (master heeft lead-in van ≥ 1 s)?
3. Is de weergaveketen schoon: geen EQ, normalisatie, spatial audio,
   loudness-processing, Bluetooth of lossy compressie?
4. Is de WAV niet door een DAW/limiter gehaald na de render?
5. Herhaal de offline check (`./run.sh`) en de corruptiematrix.

Pas daarna opnieuw proberen, met dezelfde conservatieve helderheid.
