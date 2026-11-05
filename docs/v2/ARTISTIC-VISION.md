# Geborgen Geluid — aangescherpte artistieke visie

> **Status:** levend onderzoeksdocument bij de v2-verkenning. Dit is geen
> v2-specificatie en geen releaseplan. NAGLOED 1.0.4 blijft de stabiele
> referentie; de bestaande DSP-contracten, parameters en releasebranch blijven
> onaangetast. De verkenning is expliciet geopend door Lonn; de guardrails uit
> issue #14 blijven gelden.

## 1. Waar we vandaan komen

NAGLOED 1.0.4 is een instrument van residu: een capture buffer van twaalf
seconden, Sintel-grains, Ruimte en EAR. Je speelt er iets in, het instrument
houdt het vast, breekt het in sintels en laat ze nagloeien. *The sound after the
sound.* Het instrument is ontstaan uit een muzikale praktijk van eenmalige takes,
modular, field recordings en ruimte — niet uit een audiovisueel concept.

De audiovisuele richting kwam later, als artistieke ontdekking. Issue #14
bewaart die ontdekking: een wereld die *geborgen geluid* heet, naar het kind
onder de tafel op een familiefeest. De stemmen om hem heen zijn vertrouwd,
vreemd en voorbijgaand. De wereld gaat door, maar hij is er even anders in.

Deze verkenning bouwt daarop voort en **scherpt de vraag aan**. Niet: welke
features kunnen we toevoegen? Wel: **hoe maken we van NAGLOED een omgeving
waarin geluid, licht, machines en mensen elkaar werkelijk kunnen verrassen?**
Die vraag is al de kern van issue #14; wat hier wordt toegevoegd is niet een
nieuwe vraag, maar een scherpere uitwerking. Vier dingen zijn werkelijk nieuw:
de dramaturgie van het niet-antwoorden (§8), geheugen en vergeten als
compositorisch materiaal (§4.3), het aandachtsbudget in plaats van een duty
cycle (§4.2), en de keuze om eerst offline te experimenteren voordat de
plug-in wordt aangeraakt. De rest is ordening, verdieping of herhaling van wat
er al stond.

## 2. De kern in één beweging

NAGLOED wordt geen audiovisueel instrument dat geluid vertaalt naar licht. Het
**wil** een omgeving worden met een eigen geheugen en een eigen aandacht,
waarin geluid en licht elkaar kunnen herinneren, imiteren, missen, negeren en
vergeten. Dat is een intentie en een onderzoeksrichting, geen bewezen
eigenschap van de huidige software.

De wereld is niet volledig gesloten. De echte kamer blijft poreus hoorbaar; het
systeem hoeft geen kunstmatige always-on ambient passthrough te maken. Soms
transformeert het een fragment uit diezelfde omgeving. Soms klinkt er iets dat
nooit in de kamer is geweest. Soms gebeurt er niets.

## 3. Van synesthesie naar verwantschap

Het gevaar van audiovisueel werk is de kleurenorgel-reflex: elke bas een rode
flits, elke piek een witte flits. Dat is illustratie, geen verwantschap. Het
levert een demo van een effect op, geen wereld.

Wij zoeken **familiegelijkenis**: geluid en licht lijken op elkaar omdat ze uit
dezelfde gebeurtenis, dezelfde herinnering of dezelfde ruimte komen — daarna
leven ze hun eigen leven. Een lichtgrain kan een geluid veroorzaken, een geluid
kan een kleurveld nalaten dat het geluid overleeft, en twee gebeurtenissen
kunnen verwant zijn zonder samen te vallen.

Concreet betekent dat:

- **Geen frame-synchronie.** Een gedeeld gebaar mag immediate, vertraagd,
  probabilistisch of helemaal niet doorwerken.
- **Geen vaste mapping.** Relaties worden gecomponeerd, niet als functie van
  RMS of FFT opgelegd.
- **Wel herkenbaarheid.** Als er een relatie is, moet een luisteraar die kunnen
  voelen — anders is het willekeur in plaats van verwantschap.
- **Twee werelden, één familie.** Licht en geluid zijn geen twee kanalen van
  hetzelfde signaal. Het zijn verwanten met een gedeelde geschiedenis.

## 4. Vier compositorische materialen

NAGLOED 1.0.4 componeert met klank, tijd en ruimte. De volgende laag voegt
daar drie soorten materiaal aan toe, plus de mens als vierde.

### 4.1 Tijd: nu, later, nooit

Een gebeurtenis kan onmiddellijk doorwerken, na een waarneembare vertraging, op
een onvoorspelbaar moment, of nooit. Vertraging is geen latency die weg moet
worden geoptimaliseerd; het is compositorisch materiaal. De vraag is niet alleen
*hoe snel*, maar *wanneer is een reactie nog een reactie en wanneer wordt ze een
herinnering?*

### 4.2 Aandacht: een budget, geen duty cycle

Het systeem is niet verplicht op elke gebeurtenis te reageren. Terughoudendheid
is geen parameter die een vaste kans instelt, maar een aandachtsbudget: een
begrensde hoeveelheid interventies per tijdseenheid, per ruimte, per stem. Soms
is het antwoord een stilte. Soms is dichtheid juist het antwoord — een dikke,
drukkende wolk als expressie van pressie of overweldiging. Contrast tussen
terughoudendheid en overvloed is de compositie.

### 4.3 Geheugen: herinneren, vervormen, vergeten

Een fragment kan terugkeren. Niet als loop, maar als herinnering: verlaagd,
omgekeerd, korter, verder weg, kleuriger of juist grauwer dan het origineel. Wat
herinnerd wordt, is niet hetzelfde als wat gebeurde. Vergeten is even belangrijk
als herinneren: een fragment dat langzaam uit de aandacht valt, mag echt
verdwijnen. Een herinnering die altijd terugkomt is een preset, geen geheugen.

### 4.4 Aanwezigheid: gedeelde agency

Bezoekers, performers, instrumenten en agents kunnen gebeurtenissen initiëren,
veranderen, imiteren of negeren. Geen van hen heeft volledige controle. De mens
is geen gebruiker die het systeem aanstuurt, en het systeem is geen animatie die
de mens vermaakt. Het is een gedeelde situatie met meerdere stemmen.

Dat betekent ook: het systeem hoeft aanwezigheid niet te detecteren om erop te
reageren. Een kamer kan zich anders gedragen omdat er iemand is, zonder camera's
of sensoren. Aanwezigheid kan ook sociaal en situationeel zijn: een publiek, een
performer, een open deur, een late avond.

## 5. De ruimte als medespeler

De ruimte is geen decor. In 1.0.4 is Ruimte al een eigen engine; in de
verkenning wordt de *fysieke* ruimte een medespeler. Een geluid kan door een
ruimte reizen, door een andere instantie worden nagebootst, veranderen en ergens
anders onverwacht terugkeren.

De echte omgeving blijft daarbij poreus: de luisteraar hoort de kamer nog
steeds met eigen oren. NAGLOED hoeft de wereld niet te dempen om geborgenheid te
maken. Soms is de mooiste gebeurtenis een fragment dat uit de echte kamer lijkt
te komen en na de stilte terugkeert — maar dan anders.

## 6. Licht als materiaal, niet als display

Licht is in deze verkenning geen visualisatie van audio. Een lichtgrain heeft
een eigen kleur (RGB), envelope/helderheid, duur, positie (links/rechts, of
twee ogen), beweging, persistentie en timing. Het kan ontstaan naar aanleiding
van geluid, maar daarna zelfstandig verder leven.

SpectraStrobe, Kasina en DeepVision Ganzfeld zijn de eerste concrete
experimenteeromgeving: een output-medium, niet het conceptuele model. De
Ganzfeld maakt van licht een diffuus kleurveld in plaats van een beeld van
afzonderlijke LED's. Dat past bij *geborgen*: geen scherm, geen pixel, maar een
kamer die van kleur verandert.

Belangrijk is de omkering: een lichtgebeurtenis mag ook een geluid veroorzaken.
Een lichtgrain dat links wegschiet en scherp dooft, kan een lage impact,
schraap of losgelaten fragment uit een capture buffer laten horen. De causale
richting is geen vast gegeven; ze is deel van de compositie.

## 7. De menselijke maat

*Geborgen* betekent warm en beschut, niet klinisch en niet therapeutisch; het
woord is hier een artistieke en sociale kwaliteit, geen gezondheidsclaim. Er
worden geen wellness-, meditatie- of entrainment-claims gedaan. Er is geen
medische belofte en geen neurologische claim. Wat er is, is een artistieke
situatie.

Daar hoort zorg bij:

- **Fotosensibiliteit.** Conservatieve lichtintensiteit, langzame default,
  duidelijke waarschuwing, directe stop/uitschakeling.
- **Toestemming en privacy.** Wie geluid inbrengt, moet weten wat er wordt
  opgevangen, hoe lang het blijft en wat ermee gebeurt.
- **Tijdelijkheid.** Opgevangen geluid is in principe vluchtig. Bewaren is een
  bewuste keuze, geen standaard.
- **Local-first.** Geen cloud, geen stilzwijgende upload, geen profilering.
- **Omstanders.** In een publieke of gedeelde ruimte is niet iedereen een
  deelnemer; het systeem moet ook kunnen zwijgen.

## 8. Contrast als structuur

De principes uit de opdracht zijn geen effectenlijst maar een compositorische
ruimte: hoog/laag, nabij/veraf, zacht/overstuurd, stil/bewegend, langzame
evolutie/plotselinge verandering. Contrast is pas muzikaal als het ergens
naartoe beweegt en als de stilte erna even belangrijk is als de gebeurtenis.

Daarom is een tweede beweging denkbaar die in de oorspronkelijke opdracht niet
staat: **de dramaturgie van het niet-antwoorden**. Een lichtgrain dat geen
geluid krijgt. Een geluid dat geen licht krijgt. Een lange stilte die niet als
fout voelt. Als het systeem altijd antwoordt, is er geen verwantschap meer —
alleen een mechanisme.

## 9. Wat dit expliciet niet is

- Geen kleurenorgel en geen VJ-tool.
- Geen one-to-one audio-reactive visualizer.
- Geen synesthesie-machine die kleur aan frequentie koppelt.
- Geen altijd-aan ambient passthrough.
- Geen therapie-, meditatie- of entrainmentproduct.
- Geen AI-inferentie in de realtime audiopijplijn.
- Geen productroadmap voordat er iets muzikaal de moeite waard is ontdekt.
- Geen wijziging aan NAGLOED 1.0.4, zijn DSP-contracten of zijn releasebranch.

## 10. Artistieke succescriteria

Deze criteria zijn een **kompas, geen meetlat**. Ze zijn bewust kwalitatief en
moeten per experiment worden geoperationaliseerd in waarneembare vragen,
condities en beslissingsregels (zie
[EXPERIMENT-01.md](EXPERIMENT-01.md) §5). Zonder die vertaling zijn ze
richtinggevend, niet toetsbaar.

1. **Het moment van vergeetachtigheid.** Je merkt even niet meer of het licht
   bij het geluid hoort of andersom.
2. **Een stilte die klopt.** Er gebeurt niets en dat voelt als compositie, niet
   als storing.
3. **Een herkenbare terugkeer.** Iets komt terug dat je kent, maar je kunt het
   niet precies plaatsen.
4. **Contrast dat muzikaal werkt.** Dichtheid of leegte is een keuze met
   gewicht, geen parameter die toevallig zo staat. (Niet getest door *Eerste
   Gloed*, dat bewust schaars is; vraagt een tweede, dichtere variant.)
5. **Gedeelde agency.** Een performer of bezoeker kan iets veroorzaken zonder
   dat het systeem verplicht antwoordt. (Niet getest door *Eerste Gloed*, dat
   een geschreven partituur is; vraagt een latere, interactieve opzet.)
6. **Geen demo-gevoel.** Na afloop denk je niet "mooi effect", maar "wat was
   dat".

## 11. Open artistieke vragen

Deze vragen zijn bewust nog niet beantwoord. Ze sturen de experimenten.

- Is verwantschap sterker wanneer licht en geluid uit dezelfde gebeurtenis
  komen, of wanneer ze elkaars oorzaak zijn?
- Hoe lang mag een herinnering duren voordat ze sentimenteel wordt?
- Hoeveel stilte verdraagt een installatie, een concert, een huiskamer? Is dat
  contextafhankelijk?
- Kan licht een eigen stem hebben die niet herleidbaar is tot geluid — en is
  dat wenselijk, of wordt het dan decoratie?
- Wat betekent aanwezigheid als het systeem geen mensen detecteert? Is
  aanwezigheid een sociale afspraak in plaats van een sensor?
- Wanneer voelt een niet-reactie als intentie en wanneer als bug?
- Hoe verhouden geluid, licht en ruimte zich tot de mens als vierde stem,
  zonder dat de mens alles bestuurt?

## 12. Wat deze visie vraagt van de techniek

Niet meer features, maar een paar harde eigenschappen:

- Een **gebeurtenis- en geheugenmodel** dat vertraging, waarschijnlijkheid,
  herhaling, vergeten en stilte kan representeren — begrensd, deterministisch
  waar nodig, en zonder de realtime DSP afhankelijk te maken van AI.
- Een **zelfstandige lichtwereld** die los van de audio kan leven en toch
  verwant kan blijven.
- Een **netwerk- en ruimtelaag** die later meerdere instanties en kamers kan
  verbinden, zonder dat lokale autonomie verdwijnt.
- **Signaalintegriteit en veiligheid** als eerste-klas eisen, niet als
  nabrander.
- **Toestemming, privacy en tijdelijkheid** als ontwerpmateriaal.

De technische uitwerking staat in
[ARCHITECTURE-OPTIONS.md](ARCHITECTURE-OPTIONS.md),
[TECHNICAL-UNCERTAINTIES.md](TECHNICAL-UNCERTAINTIES.md),
[SPECTRASTROBE.md](SPECTRASTROBE.md) en
[SAFETY-PRIVACY.md](SAFETY-PRIVACY.md). Het eerste experiment staat in
[EXPERIMENT-01.md](EXPERIMENT-01.md).
