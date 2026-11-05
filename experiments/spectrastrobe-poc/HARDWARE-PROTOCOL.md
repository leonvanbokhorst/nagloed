# Hardwareprotocol — Eerste Gloed (SpectraStrobe PoC)

> **EXPERIMENT — geen deel van NAGLOED v1.0.4.**
> Dit protocol hoort bij `experiments/spectrastrobe-poc/`. Het is een
> conservatieve, menselijke verificatie van het signaal op een echte MindPlace
> Kasina + DeepVision Ganzfeld. **Geen medische, therapeutische of
> wellness-claims.** Het doel is vaststellen of het signaal wordt herkend en of
> het gebaar artistiek iets doet — niets meer.
>
> De bindende eisen en het volledige meetplan staan in
> `docs/v2/SPECTRASTROBE.md` (§4 hardwareverificatie, M11–M13). Dit document is
> de korte, uitvoerbare variant voor *Eerste Gloed*.

## 1. Vooraf — veiligheid (niet overslaan)

- **Geen persoon met (vermoede) fotosensitiviteit of epilepsie in de ruimte.**
  Bij twijfel: niet doen.
- Lees `docs/v2/SAFETY-PRIVACY.md` vóór de sessie. Zolang dat document niet
  bestaat: houd de ruimte verlicht, gebruik geen andere flikkerbronnen en begin
  met de standaard Ganzframes, ogen dicht.
- Eén persoon bedient (software + Kasina), één persoon kijkt. Niet parallel.
- **Stopmiddelen binnen handbereik:** weergave stopzetten, Kasina-helderheid
  naar minimum, stekker/voeding, bril af. Stop is altijd toegestaan zonder
  uitleg; "dit is genoeg" is een geldige reden.
- Bij ongemak, hoofdpijn, duizeligheid, onverklaarbare flits of detectieverlies:
  **direct stoppen** en noteren. Dat is informatie, geen mislukking.
- Nooit de helderheid opvoeren om een detectieprobleem te maskeren. Eerst
  signaalintegriteit (de offline check), dan pas hardware.

## 2. Materiaal

- Mac met deze map; `./run.sh` moet groen zijn (exitcode 0) vóór de sessie.
- Kopieer naar een microSD-kaart, **zonder metadata**, korte bestandsnamen:
  - `spectrastrobe-44k1-16bit.wav` (**veilige, aanbevolen weergaveversie**;
    16-bit PCM, geen metadata, geen dither)
  - `spectrastrobe-48k-24bit.wav` (**niet op de Kasina geverifieerd**; dit is
    de technische referentie. Alleen proberen als de firmware 24-bit aankan —
    onbekend, zie open vraag 8 in `docs/v2/SPECTRASTROBE.md`)
  - `program-only-48k-24bit.wav` (A/B, alleen luisteren)
- Kasina met actuele firmware. LED-helderheid op een **vaste, genoteerde
  stand** — niet "max". Audio- en LED-helderheid zijn aparte knoppen.
- Standaard Ganzframes (ogen dicht) én DeepVision Ganzframes (ogen open,
  diffuser, feller). DeepVision pas na een comfortabele T1–T3.

## 3. Conservatieve ladder (niet overslaan)

Elke trede: **10–15 s**, daarna 5 s donker. Bij twijfel: terug naar 0 en stoppen.
De mute/stop-handeling moet binnen 50 ms werken; test dat eerst met de hand.

| Trede | Wat | Duur | Doel |
|---|---|---|---|
| T0 | Referentie-only: speel de master, **lichtintensiteit Kasina laag**. Kasina-display moet `Mode SpectraStr/AS` tonen met SpectraStr actief. Geen zichtbaar licht nodig. | 60 s | Formaat- en panningdetectie. Zo niet: stoppen; dit is een signaal-/detectieprobleem. |
| T1 | Eerste Gloed, **44,1 kHz**, alleen de eerste 15 s (lichtgrain + 8 ms-extinctie + 180 ms-impact). Kasina-helderheid laag. | 15 s | Herkent de Kasina het signaal? Zie je amber, beweging links→rechts, abrupte extinctie, en een lichte impact later? |
| T2 | Idem op **48 kHz**, dezelfde Kasina-stand. | 15 s | Is er verschil in detectie of helderheid tussen 44,1 en 48? Dat is een bevinding. |
| T3 | Volledige score (72 s), 44,1 kHz, lage Kasina-helderheid. | 72 s | Eerste volledige artistieke indruk. Niet tegelijk analyseren; eerst gewoon kijken/luisteren. |
| T4 | Volledige score, 48 kHz. | 72 s | Vergelijking met T3. |
| T5 | DeepVision, ogen open, **lagere** Kasina-helderheid dan T3/T4. Alleen als T1–T4 comfortabel waren. | 72 s | Diffuus kleurveld; is de bedoelde helderheid zichtbaar zonder fel te zijn? |
| T6 | A/B: `program-only` zonder Ganzfeld, alleen luisteren. | 72 s | Staat het geluid op zichzelf? Draagt de stilte? |

Daarna optioneel: **tweede keer kijken** (wetend wat er komt) om te horen of
herkenning de verwantschap verandert.

## 4. Wat je per trede noteert

Vul dit in tijdens of direct na elke trede. Kort mag.

| Vraag | T0 | T1 | T2 | T3 | T4 | T5 | T6 |
|---|---|---|---|---|---|---|---|
| Detectie (SpectraStr ja/nee) | | | | | | | n.v.t. |
| Zichtbare kleur(en) | | | | | | | n.v.t. |
| Ongewenste kanalen / flikkering | | | | | | | n.v.t. |
| Beweging links→rechts herkenbaar | | | | | | | n.v.t. |
| Abrupte extinctie gezien | | | | | | | n.v.t. |
| Impact als antwoord ervaren | | | | | | | n.v.t. |
| Stilte 28–46 s gedragen | | | | | | | |
| Onbeantwoord grain om 38 s opgemerkt | | | | | | | n.v.t. |
| Late echo herkenbaar | | | | | | | n.v.t. |
| Comfort (1–5) | | | | | | | |
| Opmerkingen | | | | | | | |

Vijf perceptuele vragen (uit `docs/v2/EXPERIMENT-01.md` §5), na afloop:

1. Waren licht en geluid voor jou verwant, of leken het twee losse dingen?
2. Merkte je dat ze niet synchroon liepen — stoorde dat, of maakte het de
   relatie interessanter?
3. Wat deed de stilte tussen 28 en 46 seconden?
4. Merkte je dat het lichtgrain om 38 seconden geen antwoord kreeg? Voelde dat
   als intentie of als gemis?
5. Wat bleef er hangen: het licht, het geluid, de stilte, of niets?

## 5. Stop- en acceptatiecriteria

- **Doorgaan** alleen als T0 detectie geeft en T1–T4 zonder ongewenste kanalen,
  flikkering of ongemak verlopen.
- **Stoppen en rapporteren** bij: geen detectie, zichtbare flikkering buiten de
  bedoelde kleurovergang, ongemak/hoofdpijn, of een onverklaarbare flits.
- **Niet doorgaan naar DeepVision** als de standaard Ganzframes al onaangenaam
  zijn.
- **Geen conclusies over therapie, ontspanning of gezondheid.** Alleen:
  werkte de detectie, hoe zag het licht eruit, hoe voelde het gebaar.

## 6. Technische controle na de sessie

- M11: 120 s onafgebroken SpectraStr-detectie op het Kasina-display.
- M12 (optioneel, fotodiode + audio-interface): geen periodieke
  luminantiemodulatie 3–30 Hz met ≥ 10% diepte; helderheid monotoon met de
  ladder. Zonder fotodiode: alleen visueel, expliciet als "niet objectief"
  noteren.
- M13: stop-latentie — dragers volledig uit binnen 50 ms na de stopactie.
- Noteer firmwareversie, Kasina-helderheidsstand, sample rate, Ganzframe-type en
  de volgorde van de treden. Zonder die context is de observatie niet
  reproduceerbaar.

## 7. Als de hardware niets herkent

Dat is een **signaalintegriteit- of detectieprobleem**, geen artistiek falen.
Controleer in deze volgorde:

1. Speelt de juiste WAV (geen 44,1-bestand op 48 kHz of omgekeerd)?
2. Is de referentie echt continu aanwezig (master heeft lead-in van ≥ 1 s)?
3. Is de weergaveketen schoon: geen EQ, normalisatie, spatial audio,
   loudness-processing, Bluetooth of lossy compressie?
4. Is de WAV niet door een DAW/limiter gehaald na de render?
5. Herhaal de offline check (`./run.sh`) en de corruptiematrix.

Pas daarna opnieuw proberen, met dezelfde conservatieve helderheid.
