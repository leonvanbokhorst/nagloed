# Review B — artistieke intentie, veiligheid en scope

**Reviewer:** reviewer-b, onafhankelijk (niet betrokken bij de totstandkoming van deze verkenning)
**Datum:** 5 november 2026
**Branche / checkout:** `feature/v2-exploration` (gedeelde checkout)
**Gelezen en beoordeeld:** issue #14 (`gh issue view 14 --repo leonvanbokhorst/nagloed`), `docs/v2/README.md`, `ARTISTIC-VISION.md`, `IDEAS.md`, `EXPERIMENT-01.md`, `SAFETY-PRIVACY.md`, `SPECTRASTROBE.md`, `DISTRIBUTED.md`, `ARCHITECTURE-OPTIONS.md` en `TECHNICAL-UNCERTAINTIES.md` (voor zover nodig voor scope en consistentie), en `experiments/spectrastrobe-poc/` met `README.md`, `score.json`, `HARDWARE-PROTOCOL.md`, `out/events.csv`, `out/timeline.svg` en `out/verify-report.md`.
**Methode:** documentanalyse plus een eigen render en verificatie in `/tmp/review-b-out` (niet `./run.sh` in de gedeelde map). Daaruit: render exit 0, verify exit 0, 31 PASS / 0 FAIL / 0 SKIP, deterministisch. De score-check `score-L1-extinctie` bevestigt daarin 8 ms als PASS; dat is voor deze review relevant bewijs, geen detail.
**Beperkingen:** de Kasina en DeepVision Ganzfeld zijn niet beschikbaar, dus M11–M13 zijn niet gemeten; er is geen gesprek met Lonn geweest; `render.py` en `verify.py` zijn niet regel voor regel geaudit, alleen op veiligheids- en claimrelevantie. Deze review gaat over intentie, veiligheid, ethiek en scope; technische signaalintegriteit is het terrein van reviewer-a.

---

## Verdict

**REQUEST CHANGES.**

De verkenning is geen lege herverpakking: de dramaturgie van het niet-antwoorden, het geheugen/vergeten als compositiemateriaal, het aandachtsbudget en de keuze voor een offline renderer vóór enige plug-inwijziging zijn echte aanscherpingen van issue #14. R1–R12 zijn concreet, de bronnen zijn serieus en de toon is grotendeels nuchter.

Toch kan dit pakket niet naar de volgende poort (hardware) zoals het er nu ligt. Er zijn twee blockers:

1. **De PoC schendt de eigen harde veiligheidsregels, en de verificatie keurt die schending goed.**
2. **De perceptuele toets is zo opgezet dat ze de centrale vraag niet kan beantwoorden; de succescriteria zijn deels onfalsifieerbaar.**

Daarnaast zes majors, vooral op het gebied van de uitvoerbare veiligheid van de eerste hardware-sessie, de bewijsclaim van het experiment, de guardrail tegen audio-reactive mapping, en de scope (de verkenning is een de facto roadmap en specificatie geworden voor werk dat issue #14 expliciet nog niet wil).

---

## Blockers

### B1 — De PoC breekt R4 en R5; de verificatie codificeert dat als PASS

`SAFETY-PRIVACY.md:103` (R4) zegt letterlijk: "**Gladde envelopes:** attack/release ≥ 20 ms hard, ≥ 50 ms voor artistieke events." En `SAFETY-PRIVACY.md:104` (R5): "**Geen sprong van zwart naar vol.** Elke lichtgebeurtenis start en eindigt op een tussenwaarde; fade-in bij aanvang, fade-out bij einde."

De centrale gebeurtenis van *Eerste Gloed* is echter een extinctie van 8 ms:

- `EXPERIMENT-01.md:40`: "| 14,0 s | Extinctie | Licht dooft abrupt (8 ms) | Stil | — |"
- `experiments/spectrastrobe-poc/score.json:90`: `"env": [[0.0, 0.0], [1.2, 0.55], [3.5, 0.75], [5.4, 0.95], [5.992, 1.0], [6.0, 0.0]]`
- `score.json:93`: `"extinction": "abrupt, 8 ms"`
- `HARDWARE-PROTOCOL.md:53`: T1 speelt "lichtgrain + 8 ms-extinctie + 180 ms-impact" af als eerste kennismaking.

Erger dan een losse inconsistentie: `verify.py:498-504` maakt van "≤ 10 ms" een expliciet PASS-criterium ("L1-extinctie is abrupt", `0.0 < ext <= 0.010`), en `out/verify-report.md` meldt "score-L1-extinctie ... PASS ... 8 ms". De verificatie beloont dus precies wat de bindende veiligheidsregel verbiedt. Er staat nergens een uitzondering, motivering of meting die de 8 ms rechtvaardigt; ik heb alle docs daarop doorzocht en alleen de ontwerpbeschrijving gevonden, geen afweging.

Daar komt een tweede, niet-onderkende discontinuïteit bij. `score.json:101` laat L2 beginnen met `"env": [[0.0, 0.35], ...]`. `render.py:76` initialiseert de envelope op de eerste waarde (`arr = array.array("d", [pts[0][1]]) * count`), dus op t = 14,0 s springt de kleurdrager in één sample van 0 naar 0,35. Dat is een stap, en `SPECTRASTROBE.md:321-322` eist "gladde attacks/releases, **≥ 20 ms**, geen stappen of discontinuïteiten." Geen van de offline checks (M1–M10) meet dit; M12 is gericht op periodieke 3–30 Hz-modulatie en zou een eenmalige stap ook niet als zodanig vangen, en M12 is niet uitgevoerd.

Ten derde: `SPECTRASTROBE.md:337-339` belooft dat "De encoder begrenst zichzelf, ook als de artistieke score meer vraagt: max 25 % koppeling, geen herhaalde luminantiemodulatie in 3–30 Hz ...". In `render.py` bestaat zo'n zelfbegrenzing niet; de amplitude volgt direct `score["light"]["events"]` (zie `render.py:321-352`), en de enige ratio-controle zit achteraf in `verify.py`. Een score die 25 % of een 3–30 Hz-flikkerpatroon vraagt, wordt dus niet door de encoder tegengehouden. Dat maakt de veiligheidsclaim in het encoderdocument onbewezen.

Wat moet er gebeuren vóór de hardwarepoort:

- Kies expliciet: 8 ms wordt een gedocumenteerde, gemotiveerde uitzondering op R4 (met voorafgaande LED-meting op hardware, M12, en een ondertekende afweging), of de extinctie wordt ≥ 20 ms. Twintig milliseconden is nog steeds als "abrupt" waarneembaar; de intentie hoeft er niet aan te sneuvelen. De huidige combinatie ("hard, niet optioneel" in het ene document, "8 ms" in het andere) is niet houdbaar.
- Herstel de L2-start naar een fade-in vanaf 0 en voeg een geautomatiseerde envelopecheck toe (minimale attack/release, geen stappen) aan de verificatie, in plaats van een check die een te korte release beloont.
- Implementeer de beloofde encoderbegrenzing of schrap de claim uit `SPECTRASTROBE.md` §6.
- Verwijder of corrigeer de claim in `EXPERIMENT-01.md:67-68` dat er "een helderheidsladder in het hardwareprotocol" staat. `HARDWARE-PROTOCOL.md:45-58` bevat T0–T6, maar geen helderheids- of koppelingsladder; de bindende ladder staat in `SPECTRASTROBE.md` §4.2 (regels 217-237: 0, 1/64, 1/32, 1/16, 1/8, 1/4 van de referentie) en wordt in het uitvoerbare protocol voor *Eerste Gloed* overgeslagen. De eerste volledige kennismaking is daardoor meteen de volledige 20 %-koppeling, terwijl R3 "standaard 6,25 %" noemt (`SAFETY-PRIVACY.md:102`) en de PoC 20 % gebruikt (`experiments/spectrastrobe-poc/README.md:37`, `score.json:138`).

### B2 — De perceptuele toets kan de vraag niet beantwoorden; criteria zijn niet falsifieerbaar

`EXPERIMENT-01.md:92` noemt de perceptuele fase "Perceptueel (Lonn, met hardware)" en regel 94: "Vijf vragen, gevraagd na één keer kijken/luisteren in dim licht." Dat is één luisteraar, N = 1, en die luisteraar is de auteur/opdrachtgever die de partituur kent. Er is geen blindering, geen onafhankelijke deelnemer, geen tweede meting en geen vooraf vastgelegde analyse. Daarmee is dit een auteurstest, niet een toets van "ervaart een luisteraar verwantschap of illustratie?" (`EXPERIMENT-01.md:14-15`).

De vijf vragen zijn bovendien sturend:

- `EXPERIMENT-01.md:97-98`: "Merkte je dat het licht en het geluid niet synchroon liepen — en stoorde dat, of maakte het de relatie interessanter?" Presupponeert dat de asynchronie is opgemerkt en biedt alleen een waarderingsoordeel aan.
- `EXPERIMENT-01.md:99`: "Wat deed de stilte tussen 28 en 46 seconden?" Presupponeert dat die stilte als een afgebakend ding is ervaren.
- `EXPERIMENT-01.md:100-101`: "Merkte je dat het lichtgrain om 38 seconden geen antwoord kreeg? Zo ja: voelde dat als intentie of als gemis?" Dit verklapt het ontwerp en het doelgedrag, en vraagt vervolgens naar de interpretatie.
- `EXPERIMENT-01.md:96`: "Waren licht en geluid voor jou verwant, of leken het twee losse dingen?" Een gedwongen dichotomie zonder middencategorie.
- `EXPERIMENT-01.md:102`: "Wat bleef er hangen: het licht, het geluid, de stilte, of niets?" Een gesloten keuzelijst.

De cueing begint al eerder: de observatietabel in `HARDWARE-PROTOCOL.md:76` vraagt per trede "Onbeantwoord grain om 38 s opgemerkt", en pas daarna (`HARDWARE-PROTOCOL.md:81`) komen "de vijf perceptuele vragen ... na afloop". De waarnemer weet dus al dat er een onbeantwoord grain is voordat hem wordt gevraagd of hij het als intentie of gemis ervoer.

De criteria zijn niet falsifieerbaar:

- `EXPERIMENT-01.md:115`: "het niet-antwoord van 38 s opvalt (of juist productief onopgemerkt blijft)". Beide uitkomsten gelden als succes; hiermee is de niet-antwoord-conditie per definitie niet weerlegbaar. In de faallijst (`EXPERIMENT-01.md:118-125`) staat geen enkele faalconditie voor het niet-antwoord.
- De overige succescriteria zijn niet geoperationaliseerd: "de vertraging voelt niet als fout", "de stilte voelt als compositie", "de terugkeer is herkenbaar zonder plaatsbaar te zijn" — geen schaal, geen drempel, geen minimumaantal waarnemers, geen vergelijkingsconditie.
- Twee van de zes artistieke succescriteria uit `ARTISTIC-VISION.md:180-191` zijn met dit experiment principieel niet te testen: "Contrast dat muzikaal werkt" (regels 186-187) en "Gedeelde agency" (regels 188-189). *Eerste Gloed* is uniform schaars (de render logged "programma 7,4 s van 72,0 s (90% stilte)") en bevat geen performer of systeem dat zelf iets beslist.

Wat ontbreekt is een echte controlegroep. Dezelfde score met 0 ms vertraging, dezelfde score waarin het grain van 38 s wél antwoord krijgt, en een versie waarin de paren willekeurig zijn geplaatst: zonder zo'n A/B is "verwantschap" niet te onderscheiden van voorkennis, verwachting of productie-effect. De enige vergelijking die nu bestaat is T6 (`HARDWARE-PROTOCOL.md:58`): alleen luisteren zonder Ganzfeld. Dat test of de audio op zichzelf staat, niet de relatie.

Ook de herhaling is geen controle: `EXPERIMENT-01.md:104` zegt "twee keer kijken (de tweede keer wetend wat er komt)". Dat is een expectation-effect, geen herhaalde meting; en omdat het hardwareprotocol daarvoor al T0–T6 afspeelt, is de "tweede keer" in de praktijk de zevende blootstelling.

Wat moet er gebeuren vóór de hardwarepoort (en liefst vóór er conclusies worden getrokken):

- Vooraf een evaluatieplan vastleggen: aantal luisteraars (minimaal 5, niet de auteur), randomisatie en blindering, één of twee controlevarianten, een gestructureerde schaal plus vrije observatie, en een beslissingsregel per criterium.
- De vragen herformuleren zonder ontwerpverklap: eerst open ("Wat merkte je op?"), dan gerichte items, en de 38 s-vraag pas stellen nadat is gevraagd wat überhaupt opviel.
- De observatietabel in het hardwareprotocol scheiden van de perceptuele vragen (eerst waarnemen, dan pas de doelvragen).
- De niet-antwoord-conditie falsifieerbaar maken met de paired-conditie (38 s-grain beantwoord vs. onbeantwoord) en een vooraf vastgelegd onderscheid "intentie" versus "gemis".
- De succes-/faallijst opschonen: geen "of juist onopgemerkt" als succes, en per criterium een waarneembare drempel.
- Als dit bewust een demo is en geen toets: noem het zo, haal de succescriteria weg en claim niet dat de vraag is getest.

---

## Majors

### M1 — De eerste hardware-sessie is niet veilig genoeg dichtgetimmerd

R1–R12 zijn goed doordacht, maar het uitvoerbare protocol laat gaten die juist bij deze eerste sessie tellen.

**Stop.** `HARDWARE-PROTOCOL.md:48` eist "De mute/stop-handeling moet binnen 50 ms werken; test dat eerst met de hand." Bij een offline WAV is de enige "stop" het stopzetten van de speler; dat kapt de dragers abrupt af. Er bestaat geen gerenderde stopvariant met een klikvrije 50 ms-ramp, en M13 (`SPECTRASTROBE.md:179`) vraagt een fotodiode-meting die in het korte protocol ontbreekt. Tegelijk staat in `SAFETY-PRIVACY.md:140-142` als open vraag dat de decoder bij plotseling signaalverlies "een laatste helderheid" kan vasthouden. De "directe stop" uit `README.md:251-254` is dus een menselijke noodgreep (bril af, Kasina uit), geen geteste software-stop. Dat mag, maar dan moet het protocol dat expliciet als primaire stop benoemen en de 50 ms-claim niet doen alsof die al werkt.

**Toestemming en leeftijd.** `HARDWARE-PROTOCOL.md:16-28` noemt uitsluiting van fotosensitiviteit en een altijd toegestane stop, maar geen informatie/instemming van de persoon onder de bril, geen leeftijdscheck en geen begeleiderseis. `SAFETY-PRIVACY.md:108` (R9) eist bij kinderen begeleiding door een volwassene en `SAFETY-PRIVACY.md:155-156` sluit mensen uit die niet kunnen of willen instemmen. Het korte protocol herhaalt dat niet. Voor een solo-sessie van Lonn is dat theoretisch, maar het protocol wordt als generiek document gepresenteerd.

**Gehoor en de dragers.** `experiments/spectrastrobe-poc/README.md:46-47` stelt: "De dragers zijn onhoorbaar hoog (18,2–19,7 kHz)". Dat is precies de aanname die `README.md:214` (U15) en `TECHNICAL-UNCERTAINTIES.md:55` als **open** markeren, met de expliciete waarschuwing dat 18–20 kHz bij luid afspelen "hinderlijk of gehoorschadig" kan zijn. De PoC is met −17,4 dBTP zeer stil terwijl de referentie continu op −30 dBFS staat; in de 90 % stilte is het dragercluster het enige signaal. Een luisteraar die het programma goed wil horen, draait het volume omhoog en versterkt daarmee ook de dragers. Er is geen playbackniveau-regel, geen gehoortest met de dragers alleen, en geen uitsluiting van tinnitus/hyperacusis of jonge oren. R10 (`SAFETY-PRIVACY.md:109`) noemt alleen gehoorschade via koptelefoon. Voeg een aparte 10 s drager-only auditieve comforttest op het bedoelde niveau toe, plus een niveau- en uitsluitingsregel, voordat het kunstwerk wordt afgespeeld.

**Noodinstructie.** R11 (`SAFETY-PRIVACY.md:110`) noemt "bril af of één oog bedekken"; die tweede, snelle handeling staat niet in `HARDWARE-PROTOCOL.md:22-28`. De omgevingsregel "ruimte verlicht, geen andere flikkerbronnen" staat in het korte protocol alleen in de inmiddels achterhaalde voorwaarde "Zolang dat document niet bestaat" (`HARDWARE-PROTOCOL.md:18-20`); sinds `SAFETY-PRIVACY.md` bestaat, is die instructie daar niet meer vanzelfsprekend.

### M2 — De visie is een echte aanscherping, maar de claim "de vraag verandert" is te sterk; de criteria zijn richtinggevend, niet toetsbaar

`ARTISTIC-VISION.md:22-24` zegt: "Deze verkenning bouwt daarop voort, maar verandert de vraag." Dat is te sterk. De vraag "hoe componeer je verwantschap zonder illustratie, zonder frame-synchronie, met terughoudendheid als default" is al de vraag van issue #14. Wat werkelijk nieuw is, is beperkt maar waardevol:

- de expliciete dramaturgie van het niet-antwoorden (`ARTISTIC-VISION.md:158-162`),
- geheugen/vergeten als compositiemateriaal (`ARTISTIC-VISION.md:82-88`),
- het aandachtsbudget in plaats van een duty cycle (`ARTISTIC-VISION.md:73-80`),
- de offline renderer als eerste, veilige stap (`README.md:171-178`),
- een gecodificeerd veiligheidskader.

De rest is uitwerking, herhaling of een andere ordening van issue #14. Dat is niet erg, maar de documenten presenteren de uitwerking soms als een nieuwe artistieke waarheid. De zes artistieke succescriteria (`ARTISTIC-VISION.md:180-191`) zijn een kompas, geen criteria: "Het moment van vergeetachtigheid", "Een stilte die klopt", "Contrast dat muzikaal werkt", "Geen demo-gevoel" hebben geen waarneembare definitie, geen schaal en geen falsificatievoorwaarde. Zeg dat, of operationaliseer ze per experiment; nu suggereren ze een meetbaarheid die er niet is.

De toon is grotendeels trouw aan NAGLOED/BRAND: Nederlands, concreet, geen marketing, geen "unleash"-taal, en de technische documenten zijn precies en nuchter. Maar de framingsdocumenten schuiven op naar manifest en zelfprijzing: "een omgeving met een eigen geheugen en een eigen aandacht" (`ARTISTIC-VISION.md:28-30`), "de wereld gaat door, maar je bent er even anders in" (`README.md:54-56`), "Dit is de artistieke kern van issue #14 en het minste risico voor v1.0.4" (`README.md:176-178`), en de claim dat de verkenning al onafhankelijk is gereviewd (`README.md:308-310`). `docs/BRAND.md:232-245` vraagt "precise, tactile, unsentimental ... no hype"; de technische documenten halen dat, de visiedocumenten niet altijd.

### M3 — Het experiment test een geschreven verwantschap, niet het geheugen, de aandacht of het niet-antwoorden van een systeem

Dit is de belangrijkste inhoudelijke zwakte na B2. De relaties in de PoC zijn annotaties, geen gedrag:

- `score.json:131-135` beschrijft R1–R4 als relaties, maar A1 staat hard gecodeerd op `"start_s": 14.18` (`score.json:56`); de renderer leidt die timing niet van L1 af. De "reactie" is dus een handmatige plaatsing.
- `score.json:116` zegt van L3: "opzettelijk onbeantwoord: geen audio, geen latere terugkeer". Dat is een afwezigheid die de componist heeft weggelaten, niet een beslissing van een systeem met een aandachtsbudget.
- De "herinnering" is 38 s na L1 (8,0 → 46,0) en 32,2 s na de start van A1 (14,18 → 46,4). De 12 s capture-ring (`TECHNICAL-UNCERTAINTIES.md:40`, U10; `README.md:212`) kan dat materiaal niet bevatten; de PoC synthetiseert de transformatie. Als *Eerste Gloed* muzikaal slaagt, vraagt dat dus precies het soort geheugen buiten de ring dat issue #14 als het meest beschermde deel van v1 aanwijst. Die afhankelijkheid hoort in de succescriteria van het experiment te staan, niet pas later te worden ontdekt.

Daarmee bewijst *Eerste Gloed* iets waardevols — een componist kan een voelbare verwantschap schrijven met ongelijke envelopes en een bewuste vertraging — maar niet dat NAGLOED een omgeving met eigen aandacht en geheugen kan zijn. De documenten moeten dat onderscheid expliciet maken, anders wordt een geslaagde demo later gelezen als bewijs voor de visie.

Ten slotte: "één gedeeld gebaar" (`EXPERIMENT-01.md:9-11`) is in de partituur twee gebeurtenissen (een lichtextinctie en een 180 ms latere audio-impact). Conceptueel is dat verdedigbaar, maar terminologisch wordt "één gebaar" gebruikt voor een paar. Maak dat precies, want het is de kern van de vraag.

### M4 — De guardrail tegen één-op-één mapping en kleurenorgel staat, maar er zijn twee erosies

Wat overeind blijft: er is geen wijziging aan v1 (zie `git status`: alleen untracked `docs/v2/` en `experiments/`), geen nieuwe parameter, preset of UI, en de PoC-compositie is licht→audio voor L1→A1, wat issue #14 expliciet toestaat ("een lichtgebeurtenis kan een geluid veroorzaken"). Geen therapie-, meditatie- of wellnessclaims gevonden; de veiligheidsdocumenten vermijden ze consequent.

Twee erosies:

1. **Audio-reactive mapping wordt genormaliseerd.** `SAFETY-PRIVACY.md:113-116`: "de licht-engine mag geen licht produceren op basis van een onbegrensde audio-reactive mapping. Elke mapping heeft een plafond, een snelheidslimiet en een dode zone (geen licht bij stilte of lage niveaus)". Dit veronderstelt dat de licht-engine op audioniveaus reageert en vraagt alleen om begrenzing. Dat botst met `ARTISTIC-VISION.md:28-30` en 114-130, waar licht een zelfstandige wereld is die los van audio kan leven, en met issue #14's "rather than impose one-to-one audio-reactive behaviour". De dode zone is als veiligheidsrem prima, maar als artistiek model is dit de kleurenorgel met een begrenzer. Maak expliciet dat een begrensde mapping één optionele modus is (en nooit de default), of haal de passage uit het veiligheidsdocument.
2. **Het eventvocabulaire stuurt de kunst.** `README.md:107-112` presenteert acht event-kinds, "vaste eigenschappen, sampletijd, één seed, refractaire periodes, aandachtsbudgetten en causale diepte ≤ 3"; `ARCHITECTURE-OPTIONS.md:301-391` werkt dat uit met exacte duurreeksen, refractaire waarden (20/40/250 ms), budgetten (8/32/128 events/s, 16 lichtgrains, 8 herinneringen), veldnamen en een voorbeeldscore. De documenten noemen dit een hypothese, maar het vocabulaire keert terug in README, ARCHITECTURE, DISTRIBUTED, IDEAS en EXPERIMENT als de gedeelde taal. Dat is de facto vocabulairesturing: geheugen *is* een event-kind, aandacht *is* een budget per seconde, causaliteit *is* diepte ≤ 3. Houd minstens twee alternatieven open (continu geheugen vs. event-geheugen; budget vs. waarschijnlijkheid) tot een luistertest beslist, en markeer de getallen als illustratie, niet als startwaarden van een implementatie.

### M5 — Scope: de verkenning is niet compact; ze bevat een de facto roadmap en specificaties voor werk dat issue #14 nog niet wil

`docs/v2` is 176 KB en 2.548 regels (`wc -l`), plus PoC-README, hardwareprotocol en ongeveer 90 KB Python, voor één offline stuk van 72 s met vier lichtgebeurtenissen. Dat is geen "klein experiment" meer.

Belangrijker: ondanks "Geen v2-releaseplanning" (`README.md:45-46`) is er een gefaseerde planning. `README.md:164-169` geeft A–D met verdicts "Fase 2, pas na A + hardware", "Middellange termijn" en "Horizon"; `README.md:276-279` zegt "Eerst een tweede experiment ... dan een derde ... Pas daarna nadenken over een gedeelde architectuur"; `DISTRIBUTED.md:120-126` geeft een genummerde zes-staps prototypevolgorde; `TECHNICAL-UNCERTAINTIES.md` bevat 29 aannames met "Fase-1-toets"/"Fase-2-toets"-statussen. Issue #14 zegt: "Do not ... insert it into current workstreams or release gates" en "This issue ... is not a work order". Dit is een backlog met poorten, ook als elke stap "alleen met toestemming van Lonn" heet.

Daarnaast is er specificatiedrift. `ARCHITECTURE-OPTIONS.md:303-306` zegt dat het vocabulaire "geen API, geen code" is, maar §5.2–5.4 (regels 327-376) geeft per kind verplichte velden en duurreeksen, refractaire waarden, budgetten, RNG-afleiding, en regels 395-433 een voorbeeldscore in JSON. Een renderer die dat volgt, is de referentie-implementatie; dan is de "hypothese" in de praktijk een spec.

De activatie zelf is niet traceerbaar. `README.md:6-8` en `ARTISTIC-VISION.md:5-7` stellen dat "De verkenning expliciet geopend [is] door Lonn", maar issue #14 staat nog open met de instructie om er niet aan te werken, en er is geen comment of ander spoor van die activering. Leg vast wie, wanneer en met welke scope activeerde; anders rust de hele branche op een niet-controleerbare aanname.

Ook de verhouding artistiek/technisch is scheef: de technische en veiligheidsdocumenten (SPECTRASTROBE 409, ARCHITECTURE 433, DISTRIBUTED 171, SAFETY 353, TECHNICAL 139 = 1.505 regels) zijn meer dan twee keer zo groot als de artistieke (ARTISTIC 230, IDEAS 295, EXPERIMENT 163 = 688 regels), terwijl `README.md:19-22` zegt dat artistieke intentie vooropstaat en techniek ondergeschikt is. Dat is op zichzelf geen artistiek falen, maar het is een signaal dat de verkenning door de techniek wordt getrokken. Advies: houd één artistieke notitie (enkele pagina's), één experimentprotocol en één veiligheidsblad als leespad; parkeer architectuur, distributed en het onzekerhedenregister als bijlagen die expliciet buiten de poort vallen tot Lonn ze activeert.

### M6 — Documentintegriteit: claims die nog niet waar zijn en verwijzingen die niet bestaan

- `README.md:308-311`: "De verkenning is onafhankelijk gereviewd door twee reviewers ... Hun verdicten en de verwerking daarvan staan in [REVIEWS.md](REVIEWS.md). [REVIEW-PLACEHOLDER]" — `docs/v2/REVIEWS.md` bestaat niet (de reviews worden nu geschreven) en de zin staat in de voltooide tijd. Dat is een procesclaim die op het moment van schrijven onwaar is. Zet de zin in de toekomende tijd of laat hem pas verschijnen als `REVIEWS.md` bestaat, en verwijder de letterlijke placeholder.
- `experiments/spectrastrobe-poc/README.md:43-45` en `HARDWARE-PROTOCOL.md:18-20`: "Lees `docs/v2/SAFETY-PRIVACY.md` ... (parallel opgesteld); zolang dat document er niet is ..." en "Zolang dat document niet bestaat ...". Het document bestaat. Dat is een teken dat de documenten parallel zijn geschreven en niet verzoend; juist op veiligheidsinstructies is dat riskant.
- `SPECTRASTROBE.md:337-339` belooft een zelfbegrenzende encoder die niet in de PoC zit (zie B1).
- De M2-definitie wijkt af van de meting: `SPECTRASTROBE.md:168` definieert M2 als "RMS van 18,2 kHz-band, -30,0 dBFS ± 0,5 dB", terwijl `out/verify-report.md` een piekamplitude van −29,96 dBFS meldt en de RMS −32,97 dBFS is. Met de letterlijke RMS-definitie zou de check buiten ±0,5 dB vallen, maar hij staat als PASS. Dat is een technisch detail voor reviewer-a, maar het raakt de vraag of "geverifieerd" hetzelfde betekent in het document en in het rapport.
- De PoC noemt 20 % (`experiments/spectrastrobe-poc/README.md:37`, `score.json:138`) terwijl `EXPERIMENT-01.md:65-66` "maximaal ongeveer 25%" zegt; de renderer logt "max 25% van referentie (score gebruikt 20%)". Verhelder welke waarde de ontwerpwaarde is en welke de bovengrens.

---

## Minors

- **m1 — Geen vast reviewpunt.** `git status --short` toont `?? docs/v2/` en `?? experiments/`: de hele verkenning is untracked. Een review hoort bij een commit-SHA of een expliciet vastgelegde worktree; wordt er tegen een mogelijk bewegend doel gereviewd en is de herkomst van eventuele wijzigingen niet in git te herleiden. Leg een commit of een SHA vast voordat de reviews worden verwerkt.
- **m2 — R11 ontbreekt in het uitvoerbare protocol.** `SAFETY-PRIVACY.md:110` noemt "één oog bedekken"; `HARDWARE-PROTOCOL.md:22-28` noemt alleen bril af, Kasina uit en stekker. Neem de one-eye-handeling op in de stopmiddelen.
- **m3 — Herhaalschema is niet wat het document zegt.** `EXPERIMENT-01.md:104` spreekt van "twee keer kijken (de tweede keer wetend wat er komt)"; `HARDWARE-PROTOCOL.md:50-58` heeft daarvoor al T0–T6, dus de "tweede" blik is in de praktijk de zevende. Bovendien is er geen volgordebalans (eerst zonder, dan met voorkennis) tussen condities.
- **m4 — "Veilig"-taal versus R12.** `HARDWARE-PROTOCOL.md:34` noemt de 44,1/16-bit file "**veilige, aanbevolen weergaveversie**" en `ARTISTIC-VISION.md:134` zegt "Geborgen betekent veilig en warm", terwijl R12 (`SAFETY-PRIVACY.md:111`) "veilig"-claims verbiedt. In context gaat het om signaalintegriteit respectievelijk de betekenis van een woord, niet om gezondheid, maar maak dat onderscheid expliciet ("signaalveilig" versus "gezondheidsclaim") om misverstanden te voorkomen.
- **m5 — De 72 s en 90 % stilte zijn erg weinig materiaal.** De render logt "programma 7,4 s van 72,0 s (90% stilte)". Daarmee zijn de faalcriteria "de audio te vol is en de stilte niet kan dragen" en "contrast" niet echt testbaar; er is vrijwel geen dichtheid om contrast mee te maken. Overweeg een tweede, dichtere variant als contrastconditie in plaats van alleen deze sparzame versie.
- **m6 — De niet-antwoord-conditie heeft geen gevestigde vraag.** L3 is een zwak, koel grain (`score.json:108-117`) dat niet als "vraag" is opgezet; de enige basis voor verwachting is de eerdere L1→A1-pairing. Eén eerder paar is zwak bewijs dat de luisteraar een antwoord verwacht. Maak van L3 een herkenbare vraag of herhaal het patroon vaker voordat "niet-antwoord" iets betekent.
- **m7 — Geen analyseplan voor de kwalitatieve antwoorden.** `EXPERIMENT-01.md:94` vraagt vijf vragen, maar nergens staat wie ze afneemt, hoe ze worden vastgelegd, hoe ze over condities worden vergeleken of wanneer een antwoord als "verwant" telt. Zonder dat is de uitkomst anekdotisch.

---

## Nits

- `EXPERIMENT-01.md:106`: "of hetgeluid op zichzelf staat" — spatie ontbreekt.
- `SPECTRASTROBE.md:41`: "enigsins" — "enigszins".
- `SPECTRASTROBE.md:275`: "routingdtest" — "routingtest".
- `README.md:311`: letterlijke `[REVIEW-PLACEHOLDER]` in een document dat naar buiten kan.
- `README.md:176-178`: "het minste risico" is een zelfbeoordeling, geen feit; formuleer als ontwerpkeuze.
- Terminologie is wisselend Engels/Nederlands ("restraint", "agency", "grains", "events") terwijl `docs/BRAND.md:232-270` juist een strak vocabulaire vraagt. Maak een korte woordenlijst voor de verkenning.

---

## Ideeën: de drie zwakste en de twee sterkste

`IDEAS.md` heeft per idee een kleinste test en een benoemd risico; dat maakt de bundel bruikbaar. Maar de bundel is ongelijk, en het bestand geeft zelf toe dat #2, #3 en #10 "varianten van hetzelfde materiaal" zijn (`IDEAS.md:283-285`) — dat is een zwakte, geen dwarsverband.

**Zwakste drie:**

1. **#5 "De lamp verveelt zich"** (`IDEAS.md:124-146`). Antropomorfisme is hier een gimmick: "verveling" is volgens het idee zelf "moeilijk te onderscheiden van random" (`IDEAS.md:144-145`) en de artiest hoopt dat een toestandsmachine met vijf toestanden als intentie leest. Het is ook conceptueel in tegenspraak met de Ganzfeld-gedachte: er is geen lamp, er is een diffuus kleurveld. De "verveelde lamp" is een personage dat de installatie niet heeft.
2. **#10 "De bijna-gebeurtenis"** (`IDEAS.md:252-276`). Een envelope-truc (70 % van de helderheid halen en dan doven) die het document zelf al als risicovol bestempelt: "De grens tussen 'bijna' en 'kapot' is klein" (`IDEAS.md:274-276`). Het is een effect, geen dramaturgie, en variant van #3 (weigeren) en #2 (onvolledig geheugen); de kunst zit niet in de bijna-gebeurtenis zelf maar in de verwachting eromheen, en die wordt niet ontworpen.
3. **#8 "Aanwezigheid zonder beelden"** (`IDEAS.md:198-224`). Dit idee voegt iets toe dat `ARTISTIC-VISION.md:97-100` expliciet niet nodig vindt: "het systeem hoeft aanwezigheid niet te detecteren om erop te reageren ... zonder camera's of sensoren." Audio-only occupancy-schatting is onbetrouwbaar (het document zegt het zelf, `IDEAS.md:222-224`), privacygevoelig in de buurt van microfoons, en de sterkste vondst — de kaart op tafel als ritueel — heeft geen enkele sensing nodig. Schrap de sensing, houd het ritueel.

**Sterkste twee:**

1. **#2 "Het vergeetboek"** (`IDEAS.md:47-72`). Dit is het enige idee dat een structureel muzikaal proces toevoegt in plaats van een effect: elke terugkeer kost detail, sommige fragmenten verdwijnen definitief. Het is toetsbaar (de controle-render "zesde recall als eerste recall", `IDEAS.md:67-69`), het past bij de premisse (nagloed is wat overblijft), en het risico wordt correct benoemd: degradatie mag geen LoFi-effect worden, het verschil moet in de structuur zitten (`IDEAS.md:70-72`). Dit verdient een eigen experiment na *Eerste Gloed*.
2. **#3 "De kunst van het negeren"** (`IDEAS.md:74-97`). Weigeren als zet met geheugen, inclusief een "lade" met vervaltermijn, is precies de dramaturgie die de visie belooft en die *Eerste Gloed* niet test. De kleinste test (acht van tien genegeerd versus vier van tien) is concreet en falsifieerbaar. Let op de valkuil die het idee zelf noemt: het publiek moet het verschil merken zonder dat het als bug leest; daarom is de paired-conditie uit B2 hier onmisbaar.

#1 "De offstage componist" is het meest onderscheidende idee, maar het is vooral een distributie- en auteursmodel (offline partituur, geen runtime-inferentie); artistiek is het zwakker dan #2 en #3. #4 "Contrapunt" en #7 "Aandachtsbudget" zijn degelijk maar liggen dicht bij wat de visie al zegt; #6 "Weer" en #9 "Poste restante" zijn mooi maar respectievelijk modulatie-gevoelig en scope-explosief.

---

## Per beoordelingsvraag, kort

| Vraag | Oordeel |
|---|---|
| 1. Visie: aanscherping of herverpakking? | Gedeeltelijk echte aanscherping (niet-antwoorden, geheugen/vergeten, aandacht, A1-first, veiligheidskader); de claim "de vraag verandert" is te sterk en de succescriteria zijn richtinggevend, niet toetsbaar. Toon grotendeels nuchter, lokaal manifest en zelfprijzend. **Major (M2).** |
| 2. Guardrails | "Geen v1-wijziging" houdt stand (alleen untracked docs/experiments); geen therapie-/wellnessclaims; geen kleurenorgel in de compositie. Maar audio-reactive mapping wordt genormaliseerd (`SAFETY-PRIVACY.md:113-116`) en het eventvocabulaire stuurt de kunst; "geen v2-releaseplanning" wordt niet volgehouden. **Major (M4, M5).** |
| 3. Experimentdesign | Test één geschreven paar, geen systeemgedrag; vragen zijn sturend; criteria deels onfalsifieerbaar; N = 1, geen blindering, geen A/B, geen context, geen analyseplan; 72 s met 90 % stilte is te weinig; de niet-antwoord-conditie is niet falsifieerbaar. **Blocker (B2), major (M3).** |
| 4. Veiligheid en ethiek | R1–R12 zijn conservatief en goed onderbouwd; geen medische claims. Maar 8 ms/R4, L2-stap, ontbrekende helderheidsladder, geen geteste stop, geen toestemming/leeftijd/gehoortest in het uitvoerbare protocol, en een onbewezen "onhoorbaar"-claim. **Blocker (B1), major (M1).** |
| 5. Ideeën | Ongelijk; zwakste #5, #10, #8; sterkste #2 en #3; #1 meest onderscheidend maar vooral infrastructuur. **Minor, met één echte aanbeveling: #2 als tweede experiment.** |
| 6. Scope en creativiteit | Te veel document (176 KB / 2.548 regels) voor één klein experiment; de facto roadmap (A–D, fase 2, zesstaps distributed) en specificatie (vocabulaire, budgetten, JSON); technische sturing reduceert de creatieve ruimte; de activering door Lonn is niet traceerbaar. Geen verborgen v1-wijziging. **Major (M5).** |

---

## Vijf vragen die Lonn zou moeten stellen

1. **Is de 8 ms-extinctie een harde artistieke eis of een detail?** Zo hard: wie neemt de uitzondering op R4/R5 schriftelijk voor zijn rekening, en met welke meting (fotodiode/M12) voordat er iemand onder de bril zit? Zo nee: waarom staat er dan "≥ 20 ms hard" in het veiligheidsdocument en "8 ms" in de partituur?
2. **Is *Eerste Gloed* een demo of een toets?** Zo toets: welke N, welke blindering, welke controleconditie (0 ms vertraging; 38 s-grain wél beantwoord), welke vooraf vastgelegde beslissingsregel? Zo demo: schrap de succescriteria en noem het een verkenning van materiaal, niet van verwantschap.
3. **Wat is de claim die je wilt toetsen: dat een componist verwantschap kan schrijven (wat *Eerste Gloed* test) of dat een systeem zelf kan weigeren, herinneren en vergeten (wat het niet test)?** En zo het laatste: accepteer je nu al dat de 12 s-ring daarvoor moet wijken, of blijft die onaanraakbaar?
4. **Welke documenten zijn bindend en welke wegwerpbaar?** Wordt het acht-kinds-vocabulaire een specificatie of blijft het één van meerdere hypothesen? Durf je ARCHITECTURE-OPTIONS, DISTRIBUTED en TECHNICAL-UNCERTAINTIES voorlopig uit het leespad en uit de poort te halen tot de artistieke vraag is beantwoord?
5. **Wie is verantwoordelijk voor de eerste sessie met microfoon en bezoekers, en voor de eerste sessie met de bril?** Welke toestemming, welke retentie, welke niveau- en gehoortest, welke noodprocedure (epilepsie, één oog, 18–20 kHz), en waar is de activatie van deze verkenning door jou gedocumenteerd — issue #14 staat nog steeds op "do not implement; do not start research"?

---

## Wat ik heb gecontroleerd, en wat niet

**Wel gecontroleerd:**

- `gh issue view 14 --repo leonvanbokhorst/nagloed` volledig, inclusief de guardrails en de opdracht aan agents om dit niet te implementeren.
- Alle gevraagde documenten, plus `ARCHITECTURE-OPTIONS.md` en `TECHNICAL-UNCERTAINTIES.md` voor de scope- en consistentiebeoordeling.
- `git status --short` (alleen `?? docs/v2/`, `?? experiments/`; geen wijzigingen in v1-bestanden), `git diff --stat` (leeg).
- Eigen render en verificatie in `/tmp/review-b-out`: render en verify exit 0, 31 PASS / 0 FAIL / 0 SKIP; de score-check bevestigt 8 ms als PASS en de determinismecheck slaagt. `./run.sh` is niet in de gedeelde map gedraaid.
- `out/events.csv`, `out/timeline.svg`, `out/verify-report.md` en de score/relaties tegen de documenten gelegd.

**Niet gecontroleerd:**

- De werkelijke Kasina + DeepVision (niet beschikbaar): M11–M13, de decoderkarakteristiek, de LED-uitgang en de stop-latentie zijn ongemeten. Alles wat ik over veiligheid op hardware zeg, is daarom een eis vooraf, geen meting.
- De volledige implementatie van `render.py` en `verify.py`; ik heb alleen de veiligheids- en claimrelevante paden gevolgd.
- Of Lonn de verkenning inderdaad expliciet heeft geopend; dat staat in de documenten maar is niet in de repo of het issue traceerbaar.
- De juridische houdbaarheid van de privacy- en consentparagraaf; dat vraagt een jurist, zoals `SAFETY-PRIVACY.md:191-192` zelf zegt.

Er is niets buiten dit bestand gewijzigd; er zijn geen git write-commando's uitgevoerd en er is geen code of documentatie gefixt.
