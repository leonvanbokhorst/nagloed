# Veiligheid en privacy — licht, flikkering, bezoekers en data

> **Status:** onderzoeksdocument bij de v2-verkenning (issue #14), **buiten scope
> van v1.0.4**. Geen implementatievoorstel. Dit document bindt de PoC en het
> hardwareprotocol in `docs/v2/SPECTRASTROBE.md`; het is geen medisch advies en
> geen juridisch advies.
>
> **Leeswijzer:** elke bewering is gemarkeerd als **[bewijs]** (direct in een
> bron), **[inferentie]** (onderbouwd, niet direct bevestigd) of **[open]** (niet
> vast te stellen; vereist meting, deskundige of jurist). Bronlinks staan in §8.

---

## 1. Waarom dit document bestaat

NAGLOED v2 verkent licht als tweede uitingsvorm naast geluid: losse
lichtkorrels, kleurvelden en beweging in een Ganzfeld-bril. Twee risico's komen
daarbij samen die in v1.0.4 niet bestaan:

1. **Flikkerend licht vlak bij en rondom het gezichtsveld**, via hardware die
   daar niet voor is ontworpen door ons en waarvan de decoder zich anders
   gedraagt dan een beeldscherm.
2. **Microfoonopname van de omgeving**, inclusief bezoekers en omstanders, in
   een installatiecontext.

Beide worden hier conservatief afgebakend. De artistieke intentie uit issue #14
blijft leidend; dit document bepaalt alleen de grenzen waarbinnen die intentie
mag worden uitgevoerd. Waar artistieke wens en veiligheid botsen, wint
veiligheid, en wordt de artistieke oplossing opnieuw ontworpen.

---

## 2. Fotosensitiviteit en flikkering

### 2.1 Wat de bronnen zeggen

- Ongeveer 3 % van mensen met epilepsie heeft fotosensitieve epilepsie; bij
  kinderen en jongeren loopt dat op tot ongeveer 5 %, en na het twintigste
  levensjaar wordt het minder vaak vastgesteld. **[bewijs]** —
  [Epilepsy Society, factsheet photosensitive epilepsy](https://epilepsysociety.org.uk/sites/default/files/2020-08/PhotosensitiveepilepsyNovember2016.pdf)
- Veel voorkomende uitlokfrequenties liggen tussen **3 en 30 flitsen per
  seconde**; sommige mensen zijn gevoelig tot 60 Hz; onder 3 Hz is het
  ongebruikelijk. **[bewijs]** — zelfde factsheet.
- Van alle kleuren is **verzadigd rood** het meest uitlokkend; daarom kent de
  WCAG een aparte roodflitsdrempel. **[bewijs]** —
  [W3C, Understanding SC 2.3.1](https://www.w3.org/WAI/WCAG22/Understanding/three-flashes-or-below-threshold.html)
- De WCAG-drempel (niveau A) is: **niet meer dan drie flitsen in één seconde**,
  óf blijven onder de algemene flits- en roodflitsdrempels. Een flits is een
  paar tegengestelde luminantieveranderingen van ≥ 10 % van de maximale
  relatieve luminantie, waarbij het donkerste beeld onder 0,80 ligt.
  **[bewijs]** — W3C.
- Risicofactoren die de kans vergroten: het effect vult het hele gezichtsveld,
  een donkere achtergrond, vermoeidheid/stress, en patronen die snel van
  richting veranderen. **[bewijs]** — Epilepsy Society.
- Britse tv-regels beperken flitsen tot 3 Hz of minder; de Health and Safety
  Executive adviseert voor publieke voorstellingen en clubs maximaal 4 Hz.
  **[bewijs]** — Epilepsy Society.
- De leverancier van de encoder waarschuwt zelf dat de visuele uitgang
  stroboscopisch licht kan maken, dat het niet bedoeld is voor mensen met
  epilepsie of een andere aanvalsstoornis, en dat een arts geraadpleegd moet
  worden. **[bewijs]** —
  [Lumasonic health & safety](https://github.com/Lumasonic/Lumasonic/blob/main/docs/02-Prism-Studio/02-health-and-safety.md)
- De DeepVision-bril is bedoeld voor **ogen open**, heeft een **hogere maximale
  helderheid** en diffundeert het licht tot een kleurveld; SpectraStrobe-content
  komt op DeepVision **veel feller** over dan op de standaard Ganzframes.
  **[bewijs]** —
  [Lumasonic devices](https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/02-devices.md)
- De standaard Ganzframes zijn bedoeld voor **ogen dicht**. **[bewijs]** —
  [Kasina Quickstart](https://mindplacesupport.com/download/1244/)
- De referentiepanning in het SpectraStrobe-format is een L/R-ritme van
  10–50 Hz. Dat is niet bedoeld als zichtbaar licht, maar als het in de
  kleurkanalen lekt ontstaat onbedoelde flikkering. **[bewijs]** spec,
  **[inferentie]** dat dit als flikkering zichtbaar kan worden.

### 2.2 Wat dit betekent voor NAGLOED

Een Ganzfeld-bril vult het gezichtsveld en zit dicht op de ogen. Daarmee valt
een belangrijk mitigatiemiddel van een beeldscherm weg: afstand en een klein
flitsgebied. De WCAG-drempels zijn voor schermen bedoeld; voor een volledig
gezichtsveld zijn ze een **ondergrens**, niet een veilige bovengrens.
**[inferentie]** — gebaseerd op de WCAG-uitleg over het 10°-veld en de
risicofactor "effect vult het gezichtsveld" uit de Epilepsy Society-factsheet.

Daarom hanteert NAGLOED strengere regels dan de WCAG-minimumdrempel. De
kwantitatieve vertaling naar de werkelijke LED-uitgang (cd/m², flitsduur,
ruimtelijke omvang) is niet gemaakt; dat is een **open** punt en de reden dat
elke hardwarestap in `SPECTRASTROBE.md` met een ladder en een directe stop
gebeurt.

---

## 3. Harde ontwerpregels voor licht

Deze regels gelden voor de encoder, de licht-engine en de preset-/moduskeuze.
Ze zijn niet optioneel en niet door een gebruiker uit te zetten zonder
expliciete, gewaarschuwde handeling.

| # | Regel |
|---|---|
| R1 | **Standaard geen herhaalde flikkering.** Kleur- en helderheidsveranderingen zijn traag; standaard luminantiemodulatie onder 1 Hz. |
| R2 | **Nooit meer dan drie flitsen in één seconde** (WCAG-niveau), en geen aanhoudende modulatie in 3–30 Hz. Dit is een harde encoderlimiet, niet een UI-advies. De voorgeschreven 10–50 Hz-panning van de **referentiedrager** valt hier niet onder: die is geen zichtbare kleurmodulatie, mits de referentie niet in de kleurkanalen lekt. Dat lek is meetpunt M5/M6 en een stopcriterium. |
| R3 | **Helderheidsplafond:** kleurkanalen maximaal 25 % van de referentie **per kleurdrager**, standaard 6,25 % (zie `SPECTRASTROBE.md` §6). Het plafond gaat vóór artistieke wens. De som van de drie kleuren in één oor kan hoger uitkomen; die wordt apart bewaakt. |
| R4 | **Gladde envelopes:** attack/release ≥ 20 ms hard, ≥ 50 ms voor artistieke events. Een eenmalige, geïsoleerde extinctie van 20 ms is toegestaan; herhaalde snelle modulatie niet. Gladde envelopes maken herhaling in 3–30 Hz niet veilig; R2 blijft gelden. |
| R5 | **Geen sprong van zwart naar vol.** Elke lichtgebeurtenis start en eindigt op een tussenwaarde; fade-in bij aanvang, fade-out bij einde. |
| R6 | **Rood is extra voorzichtig:** geen verzadigde roodflitsen; rood krijgt een lager plafond dan groen/blauw. |
| R7 | **Referentiedrager is geen artistiek kanaal.** De 10–50 Hz-panning van de referentie mag niet als zichtbaar effect worden gebruikt of versterkt. |
| R8 | **Elke sessie begint donker en laag**, ook als de vorige sessie goed ging. Geen "herstel" van de vorige helderheid zonder bevestiging. |
| R9 | **Kinderen en jongeren:** extra terughoudendheid; fotosensitiviteit komt bij hen vaker voor. Begeleiding door een volwassene is voorwaarde. |
| R10 | **Pauzes en volume:** maximaal ongeveer een uur aaneengesloten, daarna pauze; waarschuw voor gehoorschade bij koptelefoon, in lijn met de leverancierswaarschuwing. |
| R11 | **Noodmaatregel bij ongemak:** bril af of één oog bedekken dempt het effect sterk; de bediener moet dit kunnen instrueren. |
| R12 | **Geen "veilig"-claim.** We communiceren "conservatief" en "met directe stop", nooit "veilig voor iedereen". Waar het woord "veilig" toch valt, gaat het over **signaalintegriteit** ("signaalveilig bestand"), nooit over gezondheid. |

Aanvullend: een **begrensde audio-reactieve mapping** is hoogstens één optionele
modus, nooit de default en nooit het conceptuele model. De voorkeursrelatie is
gebeurtenis/gebaar/herinnering, niet een continue afbeelding van audioniveau op
helderheid. Waar een mapping wordt gebruikt, heeft ze een plafond, een
snelheidslimiet en een dode zone (geen licht bij stilte of lage niveaus), zodat
de installatie niet continu flikkert. **[technische oplossing]**

---

## 4. Direct stoppen en bediening

### 4.1 Drie niveaus

1. **Software-panic (fase B).** Een altijd bereikbare stop die alle dragers en
   kleurgains binnen 50 ms naar nul brengt, klikvrij, zonder "laatste flits".
   De knop is zichtbaar in de GUI en heeft een sneltoets; hij is nooit achter
   een menu of instelling verborgen. Deze stop hoort bij de realtime encoder in
   een latere fase; de **offline PoC heeft hem niet** en claimt hem niet. Daar
   is de primaire stop menselijk (hardware + bril af).
2. **Hardware.** Kasina-helderheid naar minimum, daarna uitschakelen; bril
   afkoppelen. De Kasina heeft aparte knoppen voor audio-volume en
   LED-helderheid. **[bewijs]** — Lumasonic Kasina-doc
3. **Bril af.** De eenvoudigste en snelste menselijke actie.

### 4.2 Eisen aan de stop

- De stop werkt ook als de audio-engine nog blokken verwerkt, als de host
  vastloopt of als het netwerk wegvalt. De stop is dus niet afhankelijk van de
  transportstatus van de DAW. **[technische oplossing]**
- Bij signaalverlies of einde van het stuk gaan **eerst de kleuren naar nul**,
  daarna de referentie. Reden: de decoder kan bij plotseling signaalverlies een
  laatste helderheid vasthouden; dat mag geen blijvend fel beeld geven.
  **[inferentie]** — nog te verifiëren op hardware **[open]**.
- De stop wordt **voor elke sessie getest** en het resultaat wordt genoteerd.
- Er is één aangewezen bediener per sessie; die persoon kijkt niet mee in de
  bril en houdt de stop vast. **[technische oplossing]**

### 4.3 Omgeving en voorbereiding

- Ruimte verlicht, geen andere flikkerbronnen (spotlights, beamers, discolicht)
  in hetzelfde gezichtsveld. **[inferentie]** op basis van de donkere-achtergrond-
  risicofactor.
- Geen gebruik in combinatie met andere stroboscopische bronnen of in een
  drukke clubcontext zonder aparte risicoafweging. **[open]**
- Duidelijke mondelinge instructie vooraf: wat je gaat zien, dat het licht fel
  kan zijn, dat je direct stopt bij ongemak, en dat je de bril mag afzetten.
- Geen gebruik door mensen die niet kunnen of willen instemmen (slapend,
  onder invloed, kinderen zonder begeleiding).
- **Gehoor en niveau.** De dragers (18,2–19,7 kHz) zijn niet gegarandeerd
  onhoorbaar; dat hangt af van leeftijd, gehoor, transducers en weergaveniveau
  (U15). Speel op een vast, genoteerd niveau, niet "zo hard als nodig om het
  programma te horen". Doe vóór de sessie een korte drager-only
  comfortbeluistering (10 s) op het bedoelde niveau. Bij tinnitus, hyperacusis,
  jonge oren of twijfel: lagere niveaus of niet deelnemen. Dit is een
  comfort- en gehoorspreventiemaatregel, geen medische test.

---

## 5. Geen medische of wellness-claims

- NAGLOED is een **artistiek instrument**. Het is geen medisch hulpmiddel, geen
  therapie en geen meetinstrument voor hersenen of gezondheid.
- We doen **geen** uitspraken over ontspanning, slaap, concentratie, angst,
  pijn, "brainwave entrainment", neurologische effecten of welzijnswinst.
- De omgeving (met name de hardwareleveranciers) gebruikt soms
  entrainment-taal. Wij nemen die niet over in UI, README, presetnamen of
  communicatie. Verwijzingen naar de hardware zijn beschrijvend, niet
  goedkeurend.
- Veiligheidscommunicatie is risicobeperking, geen bewijs vanwerking. Woorden
  als "veilig", "geneest", "kalmeert" en "verbetert" worden vermeden.
- Bij twijfel over een formulering: beschrijf wat het systeem **doet**
  (licht, kleur, timing), niet wat het **voor de gebruiker betekent**.

**[technische oplossing]**

---

## 6. Bezoekers, toestemming en privacy

### 6.1 Uitgangspunt

In een installatie kan NAGLOED de omgeving versterken of transformeren. Daarvoor
is microfoonopname nodig. Dat is iets anders dan "de omgeving horen": natuurlijk
horen valt buiten de software; vanaf het moment dat de software opneemt, gelden
privacyverplichtingen. **[inferentie]**

Spraak is persoonsgegeven; het opnemen en bewaren van bezoekersgeluid valt
daarmee in de sfeer van de AVG/GDPR (doelbinding, minimale gegevens,
opslagbeperking, transparantie). Of en hoe dat juridisch precies ligt, is
**[open]** en vraagt juridische toets, zeker voor een publieke installatie en
voor minderjarigen.

### 6.2 Toestemming en transparantie

- Zichtbare aankondiging **vóór** de opnamezone: dat er geluid wordt opgenomen,
  waarvoor, hoe lang het bestaat, en hoe je het kunt vermijden.
- Actieve toestemming van de deelnemer die de installatie gebruikt; voor
  kinderen toestemming van een begeleider.
- Een **mic-off-stand** is altijd beschikbaar: de installatie werkt dan met
  interne bronnen (ruis, drones, eerder geladen materiaal). Dat is geen
  verslechtering van het concept; issue #14 noemt interne bronnen expliciet.
- De bezoeker kan op elk moment stoppen en verzoeken om verwijdering.
- Geen opname zonder dat de bezoeker het weet, ook niet "voor de sfeer" of voor
  latere promotie.

### 6.3 Omstanders

- Omstanders kunnen onbedoeld in de microfoon vallen en kunnen praktisch niet
  allen toestemmen. Maatregelen: duidelijke zone, gerichte microfoon,
  aankondiging, korte buffer, en een installatie die niet op gesprekken is
  gericht.
- Verwerking tot korrels maakt spraak niet automatisch onherkenbaar of
  anoniem; fragmenten kunnen verstaanbaar terugkomen. We claimen dus **geen**
  anonimisering. **[inferentie]**
- Als verstaanbare spraak een realistisch risico is: extra demping, kortere
  buffer, of een "alleen interne bronnen"-modus als standaard. **[open]** welke
  mate van abstractie in de praktijk voldoende is.

### 6.4 Tijdelijk bewaren van opgevangen geluid

- Standaard: **alleen vluchtig geheugen**. De capture-ring (12 s in v1) wordt
  continu overschreven; er wordt niets naar schijf geschreven en niets
  verstuurd. Freeze houdt de buffer alleen zolang de gebruiker die vasthoudt.
- Geen automatische opname, geen sessielogging met audio, geen crashdump met
  audio.
- Exporteren kan alleen door een **bewuste, zichtbare handeling** van de
  bediener. Dan geldt: lokaal opslaan, documenteren, en na de sessie verwijderen
  (voorstel: binnen 24 uur, tenzij de bezoeker expliciet anders afspreekt).
- Er is een vaste, gedocumenteerde bewaartermijn en een vaste verwijderroutine;
  "tijdelijk" zonder einddatum is geen beleid. **[technische oplossing]**
- Toegang tot eventuele opnames is beperkt tot de bediener; geen gedeelde map,
  geen cloud, geen automatische back-up.

### 6.5 Local-only verwerking

- Verwerking en weergave zijn **lokaal**. Geen netwerk, geen telemetrie, geen
  analytics, geen accounts, geen uploads by default.
- Logs bevatten geen audio, geen spraakfragmenten en geen persoonsgegevens;
  foutlogs worden ontdaan van paden en bestandsnamen die naar bezoekersmateriaal
  kunnen wijzen.
- Een netwerkverbinding is alleen aanwezig als een toekomstige modus dat echt
  vereist, en dan uitsluitend voor het minimale telegram hieronder.

### 6.6 Distributed modus: dataminimalisatie

`docs/v2/ARCHITECTURE-OPTIONS.md` (§2.D) beschrijft een toekomstig peer-model
waarin ruimtes alleen gebeurtenissen uitwisselen, geen audio. Als dat ooit wordt
gebouwd, geldt:

- **Geen audio over het netwerk**, ook niet tijdelijk of als "feature".
- Alleen het minimale telegram: id, soort, tijd, parameters, seed, oorzaak —
  geen namen, geen stem, geen vrije tekst, geen locatie van personen.
- Tijdstempels grof en relatief; geen continue stroom die als profiel bruikbaar
  is.
- Geen persistente geschiedenis buiten de sessie; geen logs die bezoekers
  traceerbaar maken.
- Alleen op een vertrouwd lokaal netwerk; geen internetblootstelling zonder
  aparte beveiligings- en privacytoets. **[open]**
- Deelnemers kunnen de distributed modus weigeren zonder de kernervaring te
  verliezen.

**[technische oplossing]**, grotendeels overgenomen uit het eerder genoemde
architectuurdocument.

### 6.7 Ontwerpprincipes privacy (samengevat)

1. **Data-minimalisatie:** alleen wat nodig is voor het geluid; geen
   identificerende kenmerken.
2. **Lokaal-eerst:** verwerking, opslag en weergave op de machine zelf.
3. **Vluchtig by default:** ringbuffer in geheugen, geen persistente opname.
4. **Transparantie vóór opname:** aankondiging en toestemming, geen verrassing.
5. **Bezoeker heeft altijd de regie:** mic uit, stoppen, verwijderen.
6. **Geen hergebruik:** opgenomen geluid wordt niet gebruikt voor promotie,
   analyse, training of andere doeleinden dan de sessie.
7. **Bewaartermijn met einddatum** en een aantoonbare verwijderroutine.
8. **Bystanders meenemen** in het ontwerp, niet wegredeneren.
9. **Geen schijnveiligheid:** als abstractie geen anonimisering garandeert,
   communiceren we dat.
10. **Alles wat niet nodig is, bestaat niet:** geen analytics, geen accounts,
    geen cloud.

---

## 7. Open vragen

1. **[open]** Wat is de werkelijke luminantie (cd/m²) en flitsduur van de
   LED-uitgang op de gekozen Kasina-stand en met DeepVision, en hoe verhoudt
   die zich tot de WCAG-drempels? Zonder die meting blijft de koppeling een
   relatieve veiligheidsmarge.
2. **[open]** Hoe gedraagt de decoder zich als de dragers plotseling stoppen
   (bijv. bij een panic of stroomverlies)? Blijft de laatste helderheid staan?
   Bepaalt of de stopvolgorde (eerst kleur, dan referentie) voldoende is.
3. **[open]** Hoe controleren we objectief dat er in de LED-uitgang geen
   ongewenste 3–30 Hz-modulatie zit, ook bij complexe envelopes? Fotodiode plus
   analyse is voorgesteld, maar nog niet uitgevoerd.
4. **[open]** Wat is de juridische grondslag en is een DPIA nodig voor een
   publieke installatie met microfoonopname? Wie is verwerkingsverantwoordelijke,
   hoe lang mag materiaal blijven, en hoe werkt het verwijderverzoek?
5. **[open]** Kunnen bezoekersgeluiden na korrelverwerking als anoniem gelden?
   Werkinstelling: nee, tenzij aangetoond. Wat is een verdedigbare mate van
   abstractie?
6. **[open]** Hoe organiseren we eerste hulp en incidentafhandeling? Scholing
   van bedieners volgens officiële richtlijnen van een erkende
   epilepsie-organisatie; geen zelfbedachte medische instructies.
7. **[open]** Is een verplichte leeftijdsgrens of begeleiding nodig, en hoe
   communiceren we dat zonder medicinale framing?
8. **[open]** Welke signalering en tekst werken in een drukke publieke ruimte
   (taal, leesbaarheid, kinderen)?
9. **[open]** Nemen we een "noodstopgetuige" op in de installatie (een tweede
   persoon die meekijkt), en zo ja, hoe zonder de ervaring te verstoren?
10. **[open]** Kunnen we ooit "veilig" claimen, of blijft het bij
    "conservatief ontworpen"? Werkstandpunt: het laatste.

---

## 8. Bronnen

Veiligheid en flikkering:

- Epilepsy Society, *Photosensitive epilepsy* (factsheet, 2016):
  https://epilepsysociety.org.uk/sites/default/files/2020-08/PhotosensitiveepilepsyNovember2016.pdf
- W3C WAI, *Understanding Success Criterion 2.3.1: Three Flashes or Below
  Threshold* (WCAG 2.2):
  https://www.w3.org/WAI/WCAG22/Understanding/three-flashes-or-below-threshold.html
- ITU-R BT.1702 (referentiekader voor flitsdrempels in omroep):
  https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.1702-3-202311-I!!PDF-E.pdf
- Lumasonic, *Health and safety*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/02-Prism-Studio/02-health-and-safety.md
- Lumasonic, *Devices* (Ganzframes, DeepVision, Kasina/Limina):
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/02-devices.md
- MindPlace, *Kasina Quickstart Guide*:
  https://mindplacesupport.com/download/1244/

SpectraStrobe en hardware:

- Lumasonic, *SpectraStrobe format specification*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/03-format-specification.md
- Lumasonic, *Production and mixing*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/08-SpectraStrobe/05-production-and-mixing.md
- Lumasonic, *Kasina*:
  https://github.com/Lumasonic/Lumasonic/blob/main/docs/03-Prism-VST/08-kasina.md
- `docs/v2/SPECTRASTROBE.md` (meetplan, toleranties, hardwareprotocol)

Privacy en kader:

- Verordening (EU) 2016/679 (AVG/GDPR), met name de beginselen in art. 5
  (doelbinding, minimale gegevens, opslagbeperking) en de
  informatieverplichtingen in art. 12–14:
  https://eur-lex.europa.eu/eli/reg/2016/679/oj
- NAGLOED issue #14: `gh issue view 14 --repo leonvanbokhorst/nagloed`
- `docs/v2/ARCHITECTURE-OPTIONS.md` (§2.D distributed peer-model)
- `docs/v2/EXPERIMENT-01.md`
