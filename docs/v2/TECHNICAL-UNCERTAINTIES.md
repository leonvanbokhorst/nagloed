# NAGLOED v2 — technische onzekerheden en aannames

Register bij de verkenning van issue #14. **Buiten scope van v1.0.4**; geen
implementatieverplichting. Dit bestand legt vast wat we denken te weten, wat
bewezen is in deze checkout, en wat eerst gemeten moet worden. De
architectuurkeuzes staan in `docs/v2/ARCHITECTURE-OPTIONS.md`.

## Legenda

- **Soort aanname:** `[I]` artistieke intentie · `[T]` technische
  oplossing/hypothese · `[O]` open onderzoeksvraag.
- **Feit/hypothese:** *Feit* = aantoonbaar in deze repo, de tests of een externe
  specificatie. *Hypothese* = nog niet gemeten. *Deels* = deel bewezen, deel
  niet.
- **Impact:** hoog = raakt de kern van de ervaring of de v1-veiligheid;
  midden = raakt haalbaarheid of kwaliteit; laag = lokaal bij te stellen.
- **Status:** *bewezen* · *deels bewezen* · *open* · *fase-1-toets* (offline
  experiment) · *fase-2-toets* (in-plugin/companion).

---

## A. Tijd, control-rate en waargenomen verwantschap

| ID | Aanname | Feit/hypothese | Waarom onzeker | Hoe toetsen | Impact | Status |
|---|---|---|---|---|---|---|
| U01 | `[T]` Een raster van 16 samples (≈0,33 ms bij 48 kHz) is fijn genoeg als event-seam voor gedeelde audio/licht-gebeurtenissen. Het interval bestaat feitelijk (`src/dsp/EngineDSP.h:73`, absoluut sampleteller in `EngineDSP.cpp:580-598`), maar is nooit als *event*raster gebruikt. | Deels feit | De engine rekent op dit raster, maar er is geen meting van de waarneembare kwaliteit van events op deze resolutie; ook is niet duidelijk of een grover raster (1–5 ms) volstaat. | Renders met event-raster 0,33/1/5/20 ms; luister-/kijktest met dezelfde oorzaak en verschillende offsets. | Midden | Open |
| U02 | `[I]` Verwantschap ontstaat door gedeelde oorzaak en gedeelde geschiedenis, niet door frame-synchronie. Dit is de artistieke premisse van issue #14 ("related, but neither is a literal illustration"). | Hypothese (intentie) | De premisse is nog nooit in een werkende render getest; het is denkbaar dat het publiek zonder enige synchronie helemaal geen relatie ervaart. | Twee renders van dezelfde score: (a) strak synchroon, (b) gedeelde oorzaak met 80–200 ms offset; vergelijkend luisteren. | Hoog | Fase-1-toets |
| U03 | `[O]` Delta-tijden 0–50 ms worden als "gelijktijdig" ervaren, 100–500 ms als "reactie", boven ≈1 s als "herinnering". | Hypothese | Geen perceptiemeting; bovendien verschilt dit per medium (licht heeft geen echte onset zoals audio) en per persoon. | Gestructureerde A/B met Kasina + Ganzfeld, vaste delta's (0/16/50/120/250/600/1500 ms), observatieformulier. | Hoog | Open |
| U04 | `[O]` De schets "abrupte extinctie links → auditieve impact" vereist sample-accurate koppeling. | Hypothese | Volgt uit U03; als 50 ms al "gelijktijdig" voelt, is sample-accuraatheid artistiek irrelevant en technisch duur. | Render met 1 sample vs 1 frame (16 samples) vs 1 blok offset; verschil hoorbaar? | Laag/midden | Open |
| U05 | `[T]` Reproduceerbare renders vereisen dat de event-scheduler in sampletijd werkt en niet in hostblokken. Het DSP-resultaat is al blokgrootte-onafhankelijk binnen 2% RMS (`tests/DspTests.cpp:699-735`). | Deels feit | Blokinvariantie is bewezen voor audio; voor events (sortering, refractaire periodes, RNG-afleiding) is niets bewezen. 2% RMS is bovendien geen bit-identieke garantie. | Score renderen op 128/512/4096 blokken; WAV en `events.csv` vergelijken; daarna bit-identieke rerender op hetzelfde platform. | Hoog | Deels bewezen |
| U06 | `[T]` Het bestaande offline pad (`tools/render_main.cpp:45-115`, `tools/DemoScore.h:13-26`) is een bruikbare basis voor een v2-score-renderer. | Deels feit | Het pad bestaat en werkt, maar kent alleen audio-automation; licht, events en het post-DSP-probleem ontbreken. | Prototype in `experiments/`; meet of dezelfde score-opbouw en WAV-uitvoer herbruikbaar zijn. | Laag | Fase-1-toets |

## B. Audio, capture en de onhoorbare bron

| ID | Aanname | Feit/hypothese | Waarom onzeker | Hoe toetsen | Impact | Status |
|---|---|---|---|---|---|---|
| U07 | `[I]` De droge input hoeft niet hoorbaar te zijn; het echte milieu blijft via de eigen oren van de luisteraar aanwezig. Issue #14 noemt dit expliciet ("without requiring NAGLOED to manufacture an artificial always-on ambient passthrough"). | Hypothese (intentie) | Een insert-effect met alleen nat signaal is technisch triviaal, maar of dat artistiek genoeg "porositeit" geeft, is ongetest; ook de host-routing (dry/wet) bepaalt dit. | Mix-test: bron-only vs nat-only vs mix; luistertest in een stille en een rumoerige ruimte. | Hoog | Open |
| U08 | `[T]` De capture-ring kan als exclusieve grain-bron dienen zonder dat de droge input in de uitgang lekt. | Deels feit | De mix- en inputparameters bestaan (`src/core/EngineParams.h:48-50`), maar "volledig inaudible" is een routing- en meetvraag, geen parametervraag; bleed via reverb/EAR is mogelijk. | Meting met `mix=0`: autocorrelatie en spectrale vergelijking tussen input en output; test met en zonder directe monitoring. | Midden | Fase-1-toets |
| U09 | `[T]` De zelf-gegenereerde bronnen (Noise/Drone, `src/dsp/GrainEngine.cpp:408-491`) zijn artistiek toereikend als interne bron zonder input. | Deels feit | De code bestaat en is getest op stabiliteit, maar de esthetische geschiktheid is nooit beoordeeld; het zijn bestaande NAGLOED-klanken, geen v2-ontwerp. | Renders zonder input, 3–5 minuten, beoordelen op "geborgen geluid"; A/B tegen capture-materiaal. | Midden | Open |
| U10 | `[T]` De 12 s capture-ring plus freeze is genoeg voor herinnering op compositieschaal (minuten). | Hypothese | De ring is hard 12 s (`src/dsp/GrainEngine.cpp:11`, `src/core/Version.h:18`); een "herinnering" die later terugkeert vereist óf een tweede, tragere buffer, óf events/`remember` die materiaal buiten de ring bewaren. Dat is een ontwerpbeslissing, geen instelling. | Scenariotest: fragment op t=0, terugkeer op t=90 s; hoorbaar verschil met/without extra memory. | Hoog | Open |
| U11 | `[T]` Grain-feedback kan niet ontaarden in een runaway onder event-gestuurde, dichte scenarios. De bestaande DSP begrenst feedback (0–0,95, `src/dsp/GrainEngine.h:66`; schaalcorrectie in `src/dsp/EngineDSP.cpp:413-414`, toepassing in `src/dsp/GrainEngine.cpp:196-204`) en tests bewijzen decay voor de bestaande configuratie (`tests/DspTests.cpp:1267-1327`). | Deels feit | Bewezen voor vaste parameters; niet voor een event-engine die dichtheid, feedback en licht-echo's combineert. | Fuzz: 10.000 willekeurige scores met maximale dichtheid/feedback; assert op decay en eindige output. | Hoog | Fase-2-toets |
| U29 (aanvullend op U10) | `[I/T]` Vergeten is een componerbare parameter (TTL, expliciet `forget`, decay van een herinnering), niet alleen het technische einde van de capture-ring. | Hypothese (intentie + techniek) | Het vocabulaire uit `ARCHITECTURE-OPTIONS.md` §5 stelt `remember`/`forget` voor, maar of luisteraars vergeten als muzikaal materiaal ervaren is ongetest; ook een TTL zonder hoorbaar verval is artistiek betekenisloos. | Twee renders van dezelfde score: één met harde `forget`, één met geleidelijk verval; vergelijk met "niets onthouden". | Midden | Fase-1-toets |

## C. Licht, SpectraStrobe en signaalintegriteit

> Signaalmodel, toleranties en het hardwareprotocol staan in
> `docs/v2/SPECTRASTROBE.md`; hier alleen de architectonische onzekerheid. Niet
> dupliceren.

| ID | Aanname | Feit/hypothese | Waarom onzeker | Hoe toetsen | Impact | Status |
|---|---|---|---|---|---|---|
| U12 | `[T]` SpectraStrobe-carriers overleven de bestaande insert-keten niet (low-pass, saturatie, limiter, `src/dsp/EngineDSP.cpp:600-664`); licht-encoding moet dus ná de NAGLOED-DSP of offline. | Hypothese met sterke aanwijzing | De keten bevat aantoonbaar destructieve trappen, maar de exacte schade per trap en per samplerate is niet gemeten; de carriers leven op 18,2–19,7 kHz, waar de keten per definitie ingrijpt. | Corruptiematrix: resample 44,1↔48, low-pass 16 kHz, saturatie, MP3/AAC, gain; meet carrier-amplitude en -frequentie. | Hoog | Fase-1-toets |
| U13 | `[T]` Post-processing-injectie of een alternatieve bus is in AU/VST3-hosts betrouwbaar genoeg. Issue #14 vraag 6 stelt dit expliciet ter discussie. | Hypothese | Hosts verschillen in buslayout, bounce-pad, realtime vs offline en sample-rateconversie; een encoder die in één DAW werkt, werkt niet automatisch in een andere. | Matrix: 2–3 DAW's × 44,1/48 kHz × realtime/offline bounce; meet met de analyzer uit de PoC. | Hoog | Fase-2-toets |
| U14 | `[T]` Een offline exportstap volstaat voor het eerste experiment; live-emissie is niet nodig om de artistieke vraag te beantwoorden. | Hypothese (ontwerpkeuze) | Praktisch aantrekkelijk, maar het is onbekend of een offline werkwijze dezelfde ervaring geeft als een live-installatie; ook de "little GLOW machine"-gedachte suggereert speelbaarheid. | Vergelijk offline WAV-afspelen op hardware met een realtime companion; observeer het verschil in speelgevoel. | Midden | Fase-1-toets |
| U15 | `[O]` De carriers zijn voor de luisteraar onhoorbaar en voor omstanders acceptabel. | Hypothese | Hoorbaarheid hangt af van leeftijd, gehoor, transducers en weergaveniveau; "onhoorbaar" is geen garantie, en bij luid afspelen kan 18–20 kHz alsnog hinderlijk of gehoorschadig zijn. | Sinustest met de doelgroep, spectrumanalyse op de hardware, niveau- en comfortmeting. | Midden | Open |
| U16 | `[I/T]` Licht is een zelfstandige wereld met eigen kleur, envelop en timing, geen illustratie van audio. Dat vraagt een aparte scheduler en toestand, niet een uitbreiding van `VisualState`. | Hypothese (intentie + techniek) | De intentie staat vast; de technische vorm (eigen engine vs. VisualState-uitbreiding) is een ontwerpkeuze die nog niet is gevalideerd. | PoC met losse licht-envelopes en een audio-envelop die bewust afwijkt; beoordeel of de verwantschap blijft. | Hoog | Fase-1-toets |

## D. Event-engine, realtime-veiligheid en budgets

| ID | Aanname | Feit/hypothese | Waarom onzeker | Hoe toetsen | Impact | Status |
|---|---|---|---|---|---|---|
| U17 | `[T]` Een bounded event-engine kan op de audio-thread zonder allocatie of locks, binnen het bestaande CPU-budget (≈1,9–2,2% bij 64 grains; `docs/VALIDATION.md:50-51`). | Hypothese | De bestaande engine bewijst 0 allocaties (`tests/DspTests.cpp:1060-1119`), maar een scheduler met queues, refractaire timers en een licht-engine is nieuwe code met eigen risico's. | Prototype met dezelfde allocatieteller; CPU-meting bij maximale budgets. | Hoog | Fase-2-toets |
| U18 | `[T]` Bounded queues met vaste capaciteit en een expliciete drop-policy volstaan; event-groei is nooit onbegrensd. | Hypothese | Drop-gedrag kan artistiek storend zijn (wegvallende lichtkorrels) of technisch onzichtbaar blijven; de juiste policy is onbekend. | Overload-test: 10× budget, meet drops, hoorbaarheid en stabiliteit; vergelijk drop-oldest/drop-newest/coalesce. | Hoog | Fase-2-toets |
| U19 | `[T]` Refractaire periodes, rate-budgets en causale diepte ≤ 3 voorkomen runaway feedback tussen audio en licht. | Hypothese | Een grens op papier is geen bewijs; een adversariële score kan budgets precies opeten of via `echo`-ketens omzeilen. | Adversariële scores, formele invariantencheck achteraf, property-based testing op de scheduler. | Hoog | Fase-2-toets |
| U20 | `[T]` De event-engine kan los van de bestaande DSP staan zonder `EngineParams`, `VisualState` of parameter-IDs te wijzigen. | Hypothese | In A is dit vanzelfsprekend; in B raakt de engine `src/plugin` en mogelijk de contracts. De grens is nog niet hard gemaakt. | Compile-time isolatie testen; diff van `src/core` en `src/plugin` moet leeg blijven buiten een flag. | Midden | Fase-2-toets |
| U21 | `[T]` v1 blijft bit-voor-bit onaangetast als v2 achter een build-flag of in een aparte binary zit. | Hypothese | Build-isolatie is aantoonbaar via artefact-hashes, maar een flag in gedeelde bronnen kan toch gedrag veranderen (initialisatie, layout, timing). | Hash van de v1.0.4-bundles vóór en na de toevoeging; tests opnieuw draaien; pluginval/auval. | Hoog | Te toetsen |

## E. Determinisme, reproduceerbaarheid en AI

| ID | Aanname | Feit/hypothese | Waarom onzeker | Hoe toetsen | Impact | Status |
|---|---|---|---|---|---|---|
| U22 | `[T]` Eén seed per render plus een RNG per `(seed, eventId)` geeft reproduceerbare audio- én licht-renders. | Deels feit | De bestaande grain-RNG is geseed (`src/dsp/GrainEngine.cpp:88`, "nagl") en de editorhero is deterministisch per build/platform (`README.md:148-150`), maar cross-platform bit-identiek is niet bewezen en float-volgorde en libm verschillen kunnen roet in het eten gooien. | Rerender op hetzelfde platform (byte-identiek) en op een tweede platform (vergelijk met tolerantie); noteer welke onderdelen niet bit-identiek zijn. | Hoog | Fase-1-toets |
| U23 | `[T]` AI blijft buiten de realtime loop: een score of parameterplan wordt offline gecompileerd, er is geen runtime-inferentie. | Hypothese (architectuurkeuze) | Nog geen implementatie; de grens "offline gegenereerd" vs. "runtime gegenereerd" moet expliciet in het ontwerp en in de documentatie staan, anders sluipt inferentie er alsnog in. | Score-compiler-PoC; controle dat de renderer geen netwerk/modelaanroepen bevat; review van de datastroom. | Hoog | Open |
| U24 | `[O]` Een offline render en een live-uitvoering voelen artistiek vergelijkbaar genoeg. | Hypothese | Offline is deterministisch en herhaalbaar; live is dat niet. Het is onbekend of het publiek dat verschil als verlies of als kwaliteit ervaart. | A/B: dezelfde score offline vs. live companion; observatie en gesprek met Lonn. | Midden | Open |

## F. Distributed en mens/privacy

> Topologie, protocol en faalmodi staan in `docs/v2/DISTRIBUTED.md`; veiligheid
> en consent in `docs/v2/SAFETY-PRIVACY.md`. Hier alleen de aannames.

| ID | Aanname | Feit/hypothese | Waarom onzeker | Hoe toetsen | Impact | Status |
|---|---|---|---|---|---|---|
| U25 | `[T]` Event-telegrammen op een lokaal netwerk volstaan voor "kamers als mede-luisteraars"; netwerkaudio is in fase 1 niet nodig. | Hypothese | Het is onbekend of event-uitwisseling zonder gedeelde audio voldoende samenhang geeft; ook OSC-op-vertrouwd-netwerk is niet vanzelf veilig of betrouwbaar. | Twee processen op één netwerk, OSC-telegrammen, meet jitter/verlies en beoordeel de ervaring. | Midden | Open |
| U26 | `[T]` Een losse klok (PTP/NTP/Ableton Link) is voldoende; sample-accurate sync is niet vereist. | Hypothese | Volgt uit U02/U03, maar is niet gemeten; als U03 anders uitpakt, kan dit kantelen. | Jitter- en latentiemeting tussen twee nodes; luistertest met verschillende sync-fouten. | Midden | Open |
| U27 | `[I/O]` Duplicaten, verlies en drift van events zijn artistiek acceptabel zolang ze begrensd zijn (intentionele drift). | Hypothese (intentie + onderzoek) | "Acceptabel" is een esthetisch oordeel dat per situatie verschilt; onbegrensde drift kan als fout klinken. | Chaos-test: pakketverlies 0/1/5/20%, duplicaten, vertraging; observeer en bevraag. | Midden | Open |
| U28 | `[I/T]` Consent en privacy zijn niet onderhandelbaar: opnemen van bezoekers/milieu alleen met toestemming, lokaal, geen PII in telegrams, minimale retentie. | Hypothese (intentie + ontwerp) | De intentie staat vast, maar de technische handhaving (dataminimalisatie, logging, retentie, verwijderen) is nog niet uitgewerkt of getest. | Ontwerpchecklist + review tegen `docs/v2/SAFETY-PRIVACY.md`; inspectie van events en logs op PII. | Hoog | Open |

---

## Bewezen feiten in deze checkout (basislijn)

Deze punten hoeven niet opnieuw getoetst te worden; ze begrenzen wél wat een
v2-ontwerp mag aannemen.

- Het audiopad is Sintel → Ruimte → EAR met eindlimiter en ±2,0-clamp
  (`src/dsp/EngineDSP.cpp:600-664`); latency 51 samples bij 48 kHz
  (`docs/VALIDATION.md:52-53`).
- 12 s capture-ring, 256 vooraf gealloceerde grains, per-sample scheduling
  (`src/dsp/GrainEngine.cpp:11-18,129-233`; `src/core/Version.h:17-18`).
- Control-rate 16 samples op een absolute sampleteller, dus blokgrootte-
  onafhankelijk (`src/dsp/EngineDSP.h:73`, `EngineDSP.cpp:580-598`); 2% RMS
  invariantie over 128/512/4096 (`tests/DspTests.cpp:699-735`).
- 0 allocaties in `process()`/`setParams()`/`getVisual()`
  (`tests/DspTests.cpp:1060-1119`).
- `VisualState` is een display-kopie per blok; de engine mag een subset sturen
  (`src/core/Contracts.h:27-57`, `docs/ARCHITECTURE.md:34`).
- De plugin accepteert geen MIDI (`src/plugin/PluginProcessor.h:32`) en de
  engine negeert de MIDI-buffer (`src/dsp/EngineDSP.cpp:502-504`).
- Parameters worden aan blokstart gelezen; hostautomation is blokgekwantiseerd
  (`src/core/Parameters.h:190-260`).
- De GUI bereikt de engine uitsluitend via `VisualSource` en APVTS
  (`README.md:184-186`, `docs/ARCHITECTURE.md:21-25`).

## Top 5 onzekerheden (grootste invloed op de richting)

1. **U02/U03 — waargenomen verwantschap bij delta-tijden.** Als gedeelde
   oorzaak zonder synchronie niet werkt, vervalt de artistieke kern van de
   hele verkenning; als het wél werkt, is sample-accuraatheid (U01/U04)
   waarschijnlijk overbodig.
2. **U10 — herinnering voorbij de 12 s-ring.** Zonder extra geheugen is
   "terugkeer na de stilte" beperkt tot wat de ring bevat; dit bepaalt of A
   alleen een encoder-test is of een echt compositie-experiment.
3. **U12/U13 — SpectraStrobe overleeft de keten/host niet.** Dit is de harde
   technische poort: zonder betrouwbare emissie is er geen licht om te
   componeren. Eerst meten, dan pas integreren.
4. **U19 — runaway tussen audio en licht.** De bestaande DSP is veilig
   bewezen, maar een nieuwe wederzijdse koppeling is precies het scenario dat
   issue #14 expliciet verbiedt te laten ontaarden.
5. **U23/U28 — AI en consent buiten de loop.** Beide zijn voorwaarden, geen
   features: geen runtime-inferentie en geen opname zonder toestemming. Ze
   moeten in het ontwerp hard zijn, niet in een richtlijn.

## Status van dit register

Dit register is een momentopname bij de start van de verkenning (november
2026). Het is pas geldig zolang er geen metingen zijn toegevoegd; elke
afgeronde toets hoort de status te wijzigen en de gebruikte meetopstelling te
noemen. Openstaande punten zijn geen blocker voor v1.0.4 — ze zijn input voor
de vraag of, en wanneer, Lonn de verkenning activeert.
