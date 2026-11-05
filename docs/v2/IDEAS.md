# NAGLOED v2 — ideeën buiten de opdracht

Artistiek onderzoeksmateriaal bij issue #14. Dit bestand is additief en vrijblijvend: elke
gedachte hier is een **hypothese of uitnodiging**, geen eis, geen specificatie en geen
v1-scope. Niets hiervan vraagt om implementatie, contractwijziging of een release-criterium.
De guardrails van de issue gelden onverkort (geen één-op-één mapping, geen kleurenorgel,
geen therapie- of meditatieclaims, geen claims over neurologische effecten).

De ideeën hieronder zijn geen herhalingen van de schetsen in de issue. Waar een idee een
bestaande schets raakt, is de toevoeging de concrete dramaturgie, het geheugen of de
sociale vorm — niet het concept. Uitgangspunt blijft de muzikale praktijk van Master Lonn:
eenmalige takes, modular, field recordings, ruis en ruimte, en het beeld van het kind onder
de tafel op een familiefeest.

Leeswijzer per idee: gedachte, waarom het past, artistiek effect, ruwe technische vorm
(één alinea, geen spec), kleinste denkbare test, risico/onzekerheid.

---

## 1. De offstage componist

*Hypothese, geen eis.*

- **Gedachte** — Het stuk wordt niet in de kamer bedacht. Een componist (script of AI)
  werkt offline, buiten de installatie, en levert een partituur: gebeurtenissen, rusten,
  weigeringen en regels voor wat mag afwijken. Op de vloer speelt alleen de partituur en
  de kamer. Geen model, geen inferentie, geen netwerk in de keten.
- **Waarom het past** — Een eenmalige take is ook een partituur die zichzelf schrijft
  terwijl hij klinkt. De installatie wordt dan de uitvoering van iets dat al bestond, zoals
  het gesprek op het feest al gaande was voordat het kind eronder kroop.
- **Artistiek effect** — De kamer voelt niet als een reagerende machine maar als iemand die
  iets uitvoert. Bezoekers kunnen een auteur vermoeden zonder die ooit te zien.
- **Ruwe technische vorm** — Een partituur als tekst/JSON met een kleine eventwoordenschat
  (klank, licht, rust, weiger, herinner, vergeet), tijd in seconden, een seed en een
  validatiestap. Een compiler buiten de plugin zet een korte brief om in een partituur,
  rendert die deterministisch naar audio en een event-spoor, en bewaart beide. De
  installatie leest alleen een vooraf gevalideerde, read-only partituur; iteratie gebeurt
  offline (brief → partituur → render → luisteren), niet tijdens de voorstelling.
- **Kleinste test** — Drie korte briefs ("drie minuten, één stem, niets doen"; "een kamer
  die leegloopt"; "twee antwoorden die te laat komen"), twee renders per partituur met
  dezelfde seed, byte-vergelijk. Daarna laat je twee luisteraars blind de partituren aan de
  briefs koppelen. Lukt dat, dan is er een signatuur; lukt dat niet, dan is het ruis.
- **Risico/onzekerheid** — Een partituur die te veel vastlegt wordt dode muziek; een die te
  weinig vastlegt is weer kans. En "AI" is snel een sticker: de componist moet een
  waarneembare stem hebben of helemaal wegblijven.

## 2. Het vergeetboek

*Hypothese, geen eis.*

- **Gedachte** — Vergeten is geen ontbrekende data maar een actief proces met regels. Elk
  opgenomen fragment krijgt een leeftijd en een aantal keren dat het is teruggehaald. Elke
  terugkeer kost detail; sommige fragmenten verdwijnen definitief uit de index. De
  vergeetcurve is een compositieparameter, geen bug.
- **Waarom het past** — Geborgen geluid is poreus en tijdelijk; het kind weet dat het feest
  voorbijgaat. Nagloed is wat overblijft, maar niet alles blijft, en wat blijft is niet meer
  hetzelfde.
- **Artistiek effect** — Je hoort een herinnering die zichzelf verminkt. Na een paar keer
  terugkeren is het een andere plek. Dat is ontroerend zonder sentimenteel te worden; het
  publiek kan het verlies bijna aanwijzen.
- **Ruwe technische vorm** — Per capture-segment een memory-record (id, eerste gehoord,
  recall-count, laatste recall, vervalstand). Elke recall trekt een cumulatieve
  degrade-factor (bandbreedte, korte dropouts, quantisatie, pitch-drift). Een
  deterministische drop-regel verwijdert een deel van de records permanent na een aantal
  recalls of na verstreken tijd. Alles leeft in de partituur en de state, niets in een
  random generator per playback; een offline render kan het met dezelfde seed reproduceren.
- **Kleinste test** — Eén fragment van acht seconden, zes recalls in twee minuten. Render de
  zesde recall ook als "eerste recall" (controle) en laat drie luisteraars zeggen welke van
  de twee de oude is. Verwacht: de degrade-versie klinkt ouder, maar niet kapot.
- **Risico/onzekerheid** — Degradatie wordt een effect (LoFi) in plaats van een herinnering.
  Het verschil moet in de structuur zitten — welke fragmenten, wanneer, en wat nooit
  terugkomt — niet in de klank.

## 3. De kunst van het negeren

*Hypothese, geen eis.*

- **Gedachte** — Het systeem krijgt het recht om te weigeren. Elke gebeurtenis kan expliciet
  onbeantwoord blijven; weigeringen worden bijgehouden. Een genegeerde lichtvraag kan later
  terugkomen als auditieve schim, of nooit. Niet-reageren is een zet met geheugen, geen
  dode probabiliteit.
- **Waarom het past** — Het kind onder de tafel hoort stemmen die niet tegen hem praten; de
  wereld reageert niet op hem en juist dat maakt het geborgen. De issue zegt "intervening
  less often than not"; dit maakt daar een dramaturgie van in plaats van een instelling.
- **Artistiek effect** — Spanning zonder gebeurtenis. Het publiek gaat wachten, en het
  wachten wordt de ervaring. Een late reactie voelt als erkenning.
- **Ruwe technische vorm** — Een event-bus met "open vragen" per domein. Een lichtgebeurtenis
  kan een auditief verzoek achterlaten met een houdbaarheid; de mixer beslist niet per
  gebeurtenis maar per frase ("deze minuut antwoorden we niet"). Geweigerde events gaan
  naar een lade met vervaltermijn; de partituur kan ze later opgraven of wissen. Alles
  bounded, geen onbeperkte groei, geen feedback-lus.
- **Kleinste test** — Drie minuten met tien lichtgebeurtenissen, waarvan acht genegeerd;
  één variant met vier genegeerd. Vraag luisteraars welke versie meer "wacht" voelt en
  welke meer "vol". Verwacht: acht op tien geeft meer spanning maar verveelt sneller.
- **Risico/onzekerheid** — Het publiek merkt het verschil niet en leest het als kapot; of de
  lade wordt een gimmick. Nodig: iemand die de partituur kent en kan controleren of de
  weigering klopt met wat je hoort.

## 4. Contrapunt tussen de zintuigen

*Hypothese, geen eis.*

- **Gedachte** — Licht en geluid zijn twee stemmen in een contrapunt, niet een paar. Regels:
  wanneer de een spreekt, rust de ander; ze volgen elkaars motief maar niet elkaars timing;
  een canon over de zintuigen heen, waarbij licht begint en geluid een frase later volgt met
  andere intervallen.
- **Waarom het past** — De premisse zegt: verwant, maar geen letterlijke illustratie.
  Contrapunt is de muzikale vorm van verwantschap zonder synchronie. Het kind onder de tafel
  hoort de kamer ook als twee lagen die elkaar niet napraten.
- **Artistiek effect** — Een gesprek tussen twee media die elkaar kennen maar niet imiteren.
  De ruimte wordt polyfoon; je voelt dat er iets bedacht is in plaats van gekoppeld.
- **Ruwe technische vorm** — Een klein regelsysteem met twee stemmen, elk met een motief
  (intervallen, kleurverloop, envelopvorm) en een transformatie per frase (vertraging,
  inversie, retrograde). Een gedeelde klok alleen voor frases, niet voor samples. De
  partituur noteert de stemmen en de regels; de engine voert ze uit. Geen per-sample
  koppeling, geen mapping-tabel.
- **Kleinste test** — Zestig seconden canon: licht begint met een motief van drie gebaren,
  geluid volgt vier seconden later met dezelfde ritmische vorm maar andere toonhoogte,
  daarna inversie. Vraag luisteraars of ze een verband horen zonder te kunnen zeggen wat
  het is. Niemand hoort iets: te subtiel. Iedereen zegt "sync": te letterlijk.
- **Risico/onzekerheid** — Licht heeft geen toonhoogte; de regels kunnen arbitrair voelen.
  En een oefening is nog geen stuk: het contrapunt moet hoorbaar ergens naartoe gaan.

## 5. De lamp verveelt zich

*Hypothese, geen eis.*

- **Gedachte** — De lichtinstallatie is een performer met een eigen toestand: rusteloos,
  tevreden, verveeld, oplettend. Die toestand komt uit de geschiedenis — hoe lang was er
  niets, hoeveel werd er genegeerd — niet uit een random generator. Een verveelde lamp
  initieert zelf een gebaar; een tevreden lamp laat stilte toe.
- **Waarom het past** — De installatie is een aanwezigheid in de kamer, geen projectie van
  geluid. Het kind onder de tafel kent ook de lamp die aangaat zonder dat er iemand praat.
- **Artistiek effect** — Het licht voelt eigenzinnig. Bezoekers gaan het licht "lezen" en
  krijgen het gevoel dat de kamer iets van hen vindt: niet reagerend, wel aanwezig.
- **Ruwe technische vorm** — Een kleine toestandsmachine met vier of vijf toestanden, trage
  overgangen en een aandrift die groeit bij stilte. Initiatief alleen als de toestand het
  toestaat en het aandachtsbudget het toelaat. Alle beslissingen in de partituur en de
  state, reproduceerbaar met een seed; geen model, alleen regels en drempels.
- **Kleinste test** — Tien minuten zonder audio-input. Kijk of de lamp vijf tot acht eigen
  gebaren maakt die van elkaar verschillen. Laat drie mensen achteraf de volgorde
  navertellen: als ze een verhaal kunnen reconstrueren zonder dat het random aanvoelt, zit
  er agency in. Niet beweren dat het leeft.
- **Risico/onzekerheid** — Antropomorfisme wordt klef, en "verveling" is moeilijk te
  onderscheiden van random. De toestand moet gevolgen hebben — een verveelde lamp maakt
  kortere, fellere gebaren; een tevreden lamp langzame — anders is het decoratie.

## 6. Weer en gebeurtenis

*Hypothese, geen eis.*

- **Gedachte** — Twee tijdschalen: een langzaam "weer" (dichtheid, demping, helderheid,
  druk) en plotselinge gebeurtenissen. Het weer verandert niet de klank van een gebeurtenis
  maar haar betekenis: dezelfde inslag klinkt anders in mist dan in helder licht. Weer is
  dramaturgie, geen LFO.
- **Waarom het past** — De praktijk is eenmalig, modular, ruimte; een kamer heeft een
  stemming die langzaam verschuift. Het kind onder de tafel voelt de sfeer van het feest
  eerder dan de woorden.
- **Artistiek effect** — Luisteraars krijgen het gevoel dat er een dag verstrijkt, ook al is
  het stuk kort. Gebeurtenissen voelen betekenisvol omdat ze in een toestand vallen.
- **Ruwe technische vorm** — Een weather-state met drie of vier trage dimensies die via
  regels beweegt en alleen via drempels events beïnvloedt: welke events mogen, welke
  kleuren, welke ruimte, welke dichtheid. De gebeurtenissen zelf komen uit de partituur; het
  weer wordt als tweede spoor geëxporteerd zodat je het los kunt inspecteren.
- **Kleinste test** — Dezelfde vijf gebeurtenissen in twee weercondities (laag en dicht
  tegenover hoog en leeg), elk negentig seconden. Laat luisteraars in één woord zeggen wat
  de ruimte is, niet welke versie mooier is. Als de woorden systematisch verschillen, werkt
  het weer als dramaturgie.
- **Risico/onzekerheid** — Weer wordt gewoon modulatie (Auto, LFO) en dan is het niets
  nieuws. Het verschil zit in de koppeling aan betekenis — welke gebeurtenissen worden
  toegelaten — niet in continue klankbeweging.

## 7. Het aandachtsbudget

*Hypothese, geen eis.*

- **Gedachte** — In plaats van een kans per gebeurtenis heeft het systeem een budget: per
  minuut mag de kamer een beperkt aantal dingen doen. Budget kun je uitgeven of sparen;
  ongebruikt budget bouwt schuld op die op een onverwacht moment wordt afbetaald. Zo
  ontstaan clusters en stiltes die als gecomponeerd voelen.
- **Waarom het past** — "Intervening less often than not" is een kwantiteit die je hoort,
  maar een budget maakt van restraint een vorm met geheugen. Een gast op een feest praat
  ook in vlagen, en zwijgt daarna weer.
- **Artistiek effect** — Het publiek leert het ritme van de kamer kennen: lange stilte, dan
  een concentratie van gebeurtenissen. Dat voelt als iemand die iets te zeggen heeft, niet
  als een generator.
- **Ruwe technische vorm** — Een budgetmeter per minuut met maximum en carry-over, plus een
  prioriteit per kandidaat-event. De partituur markeert wat "duur" is; de scheduler geeft
  uit tot het budget op is. Alles deterministisch en exporteerbaar als event-spoor. Geen
  complexe planning, alleen een teller en een queue.
- **Kleinste test** — Twee sporen van één minuut met dezelfde gemiddelde dichtheid:
  Bernoulli tegenover het budgetmodel, met dezelfde klank gerenderd. Vraag luisteraars welk
  spoor "bedoeld" klinkt. Herhaal met vijf mensen; statistiek alleen zegt hier niets.
- **Risico/onzekerheid** — De scheduler kan hebberig worden en alles in de eerste seconde
  uitgeven, of het budget is onzichtbaar. De kunst zit in de regels voor sparen en uitgeven,
  bijvoorbeeld een minimum aan stilte na een uitgave. Het mag geen spelletje worden.

## 8. Aanwezigheid zonder beelden

*Hypothese, geen eis.*

- **Gedachte** — De kamer weet hoeveel mensen er zijn en hoe ze bewegen, zonder camera's en
  zonder audio te bewaren: alleen een paar scalairen (dichtheid, flux, stilte) uit de
  microfoon. Meer mensen betekent niet meer geluid maar een ander regime: voller publiek
  vraagt meer restraint, of juist meer feest — de kamer mag kiezen. Een fysiek teken (een
  kaart op tafel) maakt van luisteren een afgesproken toestand.
- **Waarom het past** — Het familiefeest is vol mensen; het kind onder de tafel is omringd
  maar niet aangesproken. Geen gezichtsherkenning, geen opname, geen namen: dat past bij de
  intieme en tijdelijke aard. De kaart maakt toestemming zichtbaar zonder surveillance.
- **Artistiek effect** — De installatie voelt sociaal zonder te kijken. Bezoekers merken dat
  de kamer anders is als ze met velen zijn, zonder dat ze kunnen aanwijzen wat er "slims"
  gebeurt. De kaart geeft een klein ritueel: je legt hem neer en de kamer gaat luisteren.
- **Ruwe technische vorm** — Audio-only occupancy-schatting (energie, spectrale flux,
  stemactiviteit) levert twee of drie scalairen, geen opname en geen retentie. Een
  toestandsregel "veel/weinig/leeg" beïnvloedt de dramaturgie. Een fysieke token schakelt de
  luistermodus met een vaste begin- en eindhandeling. Geen camera's, geen namen, geen
  opslag buiten de sessie.
- **Kleinste test** — Dertig minuten microfoon in een kamer met wisselend nul, twee en zes
  mensen; log alleen de scalairen en vergelijk met een handmatige telling op papier. Daarna
  één sessie van twintig minuten met de kaart op tafel en achteraf alleen woorden, geen
  data.
- **Risico/onzekerheid** — De schatting is onbetrouwbaar en kan als surveillance voelen als
  mensen hem doorhebben; de kaart kan een ding worden in plaats van een afspraak. De grootste
  winst is misschien het ritueel, niet de sensing.

## 9. Poste restante

*Hypothese, geen eis.*

- **Gedachte** — Twee of meer kamers zijn geen netwerk maar correspondenten. Ze sturen
  elkaar korte telegrammen over wat er gebeurde — geen audio, alleen een beschrijving of id
  — en de ontvanger mag het interpreteren zoals hij wil. Een kamer kan een telegram weigeren
  of in een la leggen en het pas veel later openen. Onbestelbare berichten blijven rondwaren.
- **Waarom het past** — Een feest heeft meerdere kamers; het kind onder de tafel hoort de
  keuken en de woonkamer als twee werelden die via deuren met elkaar praten. Vertraging en
  misinterpretatie horen bij herinnering.
- **Artistiek effect** — De kamers voelen als buren die elkaar brieven schrijven. Een
  gebeurtenis in kamer A landt in kamer B pas veel later, en dan anders. Bezoekers kunnen
  van kamer naar kamer lopen en de correspondentie horen.
- **Ruwe technische vorm** — Telegrammen met een minimale set velden (id, tijd, soort, duur,
  kleur of beschrijving, seed), geen audio. Bounded queue, rate limit, eigen regels per
  ontvanger voor openen, uitstellen of weigeren, en een dead-letter-lade met verval. Elke
  kamer is lokaal autonoom; geen gedeelde klok. Alleen als los experiment, niet in de plugin.
- **Kleinste test** — Twee laptops, één telegram per minuut, kunstmatig vijf seconden
  vertraging en twintig procent verlies. Luister of de kamers als correspondenten voelen of
  als kapotte kopie. Eén telegram gaat "poste restante" en wordt pas na vijf minuten
  geopend.
- **Risico/onzekerheid** — Dit is zo goed als een nieuw project; scope-explosie. De kunst
  zit in de vertraging en de misinterpretatie, niet in het protocol. Als het technisch te
  veel vraagt, doe het met twee mensen en een telefoon.

## 10. De bijna-gebeurtenis

*Hypothese, geen eis.*

- **Gedachte** — Sommige gebeurtenissen worden voorbereid en dan afgebroken: een lichtgrain
  die zeventig procent van zijn helderheid haalt en dooft, een klank die begint en wordt
  teruggetrokken. Het publiek ziet de schaduw van een gebeurtenis. Bijna-gebeurtenissen
  vormen een eigen categorie in de partituur, met eigen regels: hoe vaak, hoe dicht bij "af".
- **Waarom het past** — Het kind onder de tafel hoort de volwassenen vaak een zin beginnen
  die niet afkomt. Geborgen geluid zit vol onaffe dingen. In plaats van nog een event is dit
  de dramaturgie van het uitgestelde en het nagelaten.
- **Artistiek effect** — Meer spanning dan een volledige gebeurtenis; bezoekers vullen de
  bijna-gebeurtenis zelf in, waardoor het stuk persoonlijk wordt. Een reeks bijna's bouwt
  een verwachting op die nooit wordt betaald, of één keer wel.
- **Ruwe technische vorm** — Events met een abort-punt en een envelope die voor het
  hoogtepunt stopt. De engine speelt de aanloop en stopt op een fractie van de duur;
  meetbaar is hoeveel procent van de envelope is gehaald en hoe vaak. Licht: een korte,
  onvolledige stijging. Geluid: een aangezette maar niet-ontwikkelde grain. Deterministisch
  en exporteerbaar.
- **Kleinste test** — Zestig seconden met zes volledige en zes bijna-gebeurtenissen. Laat
  luisteraars achteraf de volgorde proberen te reconstrueren. Als ze de bijna's als gemiste
  momenten benoemen, werkt het; als ze het als foutjes horen, is het te ver doorgeschoten.
- **Risico/onzekerheid** — De grens tussen "bijna" en "kapot" is klein en verschilt per
  apparaat. Licht heeft misschien geen merkbare aanloop; dan is "bijna" alleen voor geluid
  en moet het idee smaller worden.

---

## Dwarsverbanden

- 1 en 9 delen de gedachte dat een gebeurtenis een boodschap is die onderweg kan veranderen.
- 2, 3 en 10 zijn alle drie varianten van hetzelfde materiaal: wat niet gebeurt, wat
  geweigerd wordt en wat half gebeurt. Ze kunnen als één dramaturgische familie worden
  getest, maar niet als één feature.
- 6, 7 en 8 raken elkaar in de trage toestand die het gedrag van snelle gebeurtenissen
  verandert; 8 voegt de sociale laag toe.
- 4 en 5 gaan over eigenaarschap van het licht: de een door regels, de ander door toestand.

## Status

Alles in dit bestand is hypothese of uitnodiging. Geen enkel idee is een v1-vereiste, een
contract, een parameter of een release-criterium. Wie hier iets van wil uitproberen, doet dat
in een los experiment buiten de plugin, met een eigen kleine test, en pas na expliciete
activering door Lonn.

## Reviewnoot (na onafhankelijke review)

De onafhankelijke review vond #2 (**het vergeetboek**) en #3 (**de kunst van het
negeren**) de sterkste en meest toetsbare ideeën; die twee zijn de aangewezen
kandidaten voor een tweede experiment. #5 (de verveelde lamp), #10 (de
bijna-gebeurtenis) en #8 (aanwezigheid zonder beelden) werden als zwakker
beoordeeld: #5 en #10 als effect-trucs, #8 omdat de sensing onbetrouwbaar en
privacygevoelig is terwijl het sterkste deel — de kaart op tafel als ritueel —
geen sensing nodig heeft. Dat is een notitie, geen schrapping: de ideeën blijven
uitnodigingen, maar ze zijn nu geprioriteerd. Zie
[REVIEWS.md](REVIEWS.md).
