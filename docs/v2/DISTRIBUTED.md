# Distributed NAGLOED — telegrammen tussen ruimtes

> **Status:** verkenning binnen NAGLOED v2, geen specificatie en geen implementatieopdracht. Dit document hoort bij [ARTISTIC-VISION.md](ARTISTIC-VISION.md) en [EXPERIMENT-01.md](EXPERIMENT-01.md); het beschrijft het derde experiment (twee kamers, alleen gebeurtenissen) als denkbeeld. NAGLOED 1.0.4 blijft ongewijzigd. Geen netwerkcode in de plug-in, geen DMX-implementatie, geen wijziging aan `src/`, `CMakeLists.txt`, `presets/`, `README.md` of `.gitignore`.

Markers: **Intentie** = artistieke bedoeling; **Techniek** = voorstel of feit; **Open vraag** = toetsbaar, nog niet beantwoord.

## 1. Artistieke intentie

- **Ruimtes als gedeeltelijk autonome luisteraars.** Een ruimte is een NAGLOED-instantie met eigen audio, licht, tempo en aandacht. Ze is geen slaaf van een centrale tijdlijn: ze hoort, reageert en herinnert lokaal, en kan iets doorgeven. Autonomie is de default; verbinding is de uitzondering.
- **Gebeurtenissen als telegrammen.** Wat reist is geen audio maar een bericht: kort, onvolledig, met tijdstempel en een mate van zekerheid. Het telegram is de herinnering aan een gebeurtenis, niet de gebeurtenis zelf. (**Intentie**)
- **Herinnering reist tussen ruimtes.** Een gebaar in kamer A kan in kamer B later terugkomen: verlaagd, vertraagd, verkeerd begrepen of helemaal niet. De afstand is compositorisch materiaal, geen latency die weg moet.
- **Intentionele drift en vertraging.** Kamers hoeven niet gelijk te lopen. Een kleine tempodrift, een vaste vertraging of een bewust gemiste reactie is een keuze. Perfecte sync is de default die we juist willen vermijden; dit sluit aan op "tijd: nu, later, nooit" in ARTISTIC-VISION.
- **Ongelijkheid tussen kamers als materiaal.** De ene kamer dichter, feller of aandachtiger dan de andere is contrast, geen oneerlijkheid. Een kamer die zwijgt is even belangrijk als een kamer die antwoordt.
- **Wat dit niet is.** Geen distributed orchestra in lock-step, geen remote control van één master, geen cloud-sessie, geen sample-nauwe netwerkaudio als startpunt. (**Intentie**)

## 2. Topologie-opties

Definities: **star** = één hub/broker; **peer** = directe unicast-links zonder doorgeven; **mesh** = any-to-any met forwarding of multicast.

| Topologie | Voordelen | Nadelen | Artistieke fit |
|---|---|---|---|
| Star (hub) | Eenvoudig; één plek voor loggen, rate limits en dedupe; hub hoeft geen performer te zijn | Single point of failure; hub wordt onbedoeld "master"; +1 hop | Goed voor de eerste test en installaties met een technische operator |
| Peer (directe links) | Geen broker; ruimtes blijven autonoom; voorspelbaar pad | N²-configuratie; lastiger observeren; elke ruimte moet eigen limieten hebben | Past bij 2-3 ruimtes en gelijkwaardige performers |
| Mesh (multicast/forwarding) | Robuust bij uitval; schaalt; natuurlijke "bus" | Loopdetectie, TTL, duplicaten, broadcast-stormen; moeilijk te doordenken | Interessant later; te complex als eerste stap |

**Techniek.** Eerste experiment: een logische star met een hub die vooral *logt* en niet componeert. Telegrammen blijven stateless en hub-onafhankelijk, zodat dezelfde software later peer of mesh kan draaien. Multicast is als bus aantrekkelijk (OSC over UDP kan dat), maar maakt foutopsporing en rate limiting per zender lastiger. **Open vraag:** verandert het ervaren gezag van een ruimte als de hub verdwijnt of zwijgt?

## 3. Minimaal event-telegram

Doel: klein, versioneerbaar, verlies- en duplicaat-tolerant, en los van het transport. Het gebeurtenis-vocabulaire (licht, geluid, gebaar, stilte, herinner, vergeet, echo, negeer) staat in `docs/v2/ARCHITECTURE-OPTIONS.md`; hier staat alleen de draad.

| Veld | Type | Betekenis |
|---|---|---|
| `v` | u8 | protocolversie (start 1) |
| `id` | u64 | unieke id: nonce + monotone teller; dedupe-sleutel |
| `room` | string ≤32 | ruimte-id, opaque, geen persoonsnaam |
| `kind` | u8 | gebeurtenis-soort uit het vocabulaire |
| `seed` | u32 | deterministische lokale interpretatie |
| `t_origin` | OSC-timetag | absolute tijd van ontstaan; informatief, niet leidend |
| `duration_ms` | u16 | bedoelde duur |
| `color` | u8[3] | RGB 0-255 voor licht; 0 voor niet-licht |
| `intensity` | f32 0-1 | sterkte, lokaal geclampt |
| `confidence` | f32 0-1 | hoe zeker de zender is dat dit aandacht verdient |
| `urgency` | f32 0-1 | hoe dringend; mag bij doorgeven nooit stijgen |
| `ttl` | u8 | resterende hops; 0 = niet doorgeven |
| `parents` | id[] ≤4 | causale keten; loopdetectie |
| `payload` | blob | optioneel, versiegebonden; geen audio, geen PII |

Alle integers big-endian, zoals OSC. Eén telegram = één OSC-message met een blob-payload; de debug-variant gebruikt losse argumenten.

```
/nagloed/event/v1/room/<room>/event      ,b   telegram (binair, zie tabel)
/nagloed/event/v1/room/<room>/hello      ,st  room-id, protocolversie
/nagloed/event/v1/room/<room>/heartbeat  ,i   laatste event-teller
/nagloed/event/v1/room/<room>/digest     ,b   samenvatting na herverbinden
/nagloed/event/v1/room/<room>/control    ,s   mute | stop | rate
/nagloed/event/v1/session/panic          ,    lokaal stopcommando (idempotent)
```

Waarom OSC: open, transport-onafhankelijk en gangbaar in de muziekwereld; bundles zijn atomair en dragen een NTP-timetag. De spec zegt expliciet dat OSC géén kloksynchronisatie levert, dus tijd alleen gebruiken als ze betrouwbaar is ([OSC 1.0](https://ccrma.stanford.edu/groups/osc/spec-1_0.html)).

**Regels (Techniek).**
- **Rate limits.** Zacht ≤ 2 events/s per ruimte, hard ≤ 10/s, burst ≤ 20. De ontvanger heeft een eigen ingress-plafond en een aandachtsbudget per minuut; overschrijding dempt, buffert niet tot oneindig.
- **Bounded queues.** Ingress 256 telegrammen, egress 64, geen groei. Vol → laagste confidence eerst, daarna oudste laten vallen. Nooit blokkeren; netwerk-I/O leeft buiten de audiodraad (ARCHITECTURE.md verbiedt netwerktoegang in `process()`).
- **Verlies.** UDP is best-effort: geen retransmissie, geen ACK. Verlies is normaal; een gemist telegram is een gemiste herinnering. Heartbeat en digest na herverbinden in plaats van replay.
- **Duplicaten.** Dedupe op `id` met een begrensd venster (bijv. 1024 ids / 10 s). Duplicaat = negeren, tenzij bewust als `echo` met lagere confidence. Idempotentie is een harde eis.
- **Geen runaway feedback.** `ttl` daalt per hop (start 3); `confidence` × 0,5 per hop, drop onder 0,2; `urgency` stijgt nooit; `parents` begrensd op 4; eigen id in `parents` → drop; refractory per ruimte per `kind` (250-500 ms); globaal circuit breaker bij > 50 events/s ingress (30 s mute + log). "Geen antwoord" is de default, niet de uitzondering.

## 4. Tijdstrategie

**Intentie.** Lokale autonomie is de default. Elke ruimte heeft haar eigen klok, tempo en adem. Een gedeelde klok is een optie voor een specifiek effect, niet de fundering.

| Klok | Nauwkeurigheid (orde) | Bruikbaar voor | Niet voor |
|---|---|---|---|
| Lokale monotone klok | geen onderlinge relatie | alle lokale scheduling en refractoire periodes | volgorde tussen ruimtes |
| NTP (RFC 5905) | ms op LAN; tientallen µs mogelijk | loggen, dedupe-venster, "ruwweg gelijk" | strakke gezamenlijke gebeurtenissen |
| PTP / IEEE 1588 | sub-microseconde | netwerkaudio, sample-nauwe afspraken | eenvoudige installatie; vereist PTP-switch/NIC |
| Ableton Link | muzikale beat/fase, geen wandklok | gedeelde puls zonder lock-step | absolute telegram-tijd |

**Techniek.** Gebruik `t_origin` alleen als informatie; plan lokaal met de monotone klok en relatieve vertraging. Bij drift: 10 ppm ≈ 0,86 s/dag, 50 ppm ≈ 4,3 s/dag. Laat een ruimte bewust 1-5% naast een andere lopen als dat muzikaal werkt; maak drift een parameter, geen fout.

**Wat mag laat zijn, wat niet.** Licht op een langzaam kleurveld mag tientallen milliseconden laat zijn (**Open vraag**, perceptueel te toetsen). Een lokaal geluid mag nooit op het netwerk wachten: de audiodraad rendert altijd door. Een remote getriggerd geluid kan alleen met vooruitplannen en blijft begrensd door klokfout plus jitter; zonder gedeelde klok is sample-nauwheid een illusie. Een noodstop/panic is lokaal en onmiddellijk en hangt nooit van het netwerk af.

**Event-only versus netwerkaudio.**

| | Event-only | Netwerkaudio (AES67/Dante/AVB) |
|---|---|---|
| Klok | lokaal, los | één PTP-domein nodig |
| Complexiteit | laag | hoog: switches, QoS, latency-budget, discovery |
| Faalwijze | ruimte valt terug op zichzelf | verlies van clock-master raakt iedereen |
| Artistieke betekenis | herinnering reist, audio blijft lokaal | gedeeld akoestisch materiaal, echte ruimtelijke compositie |
| Advies | start hier | pas later, alleen als event-only artistiek tekortschiet |

AES67 gebruikt doorgaans 1 ms packet time, 48 kHz en een PTP-profiel (IEEE 1588 default, AES67-media of 802.1AS). Dante is propriëtair (Audinate; lage latency, veel kanalen). AVB is een IEEE-standaardset met gPTP en AVTP op laag 2. Alle drie lossen een ander probleem op dan "een telegram laten reizen".

## 5. Faalmodi

| Faalmodus | Effect | Maatregel |
|---|---|---|
| Netwerkverlies | telegrammen verdwijnen; ruimte wordt stiller | best-effort accepteren; heartbeat; digest bij herverbinden; geen replay-storm |
| Clock drift of sprong | verkeerde volgorde; "late" events | `t_origin` informatief; monotone klok lokaal; drempel op "te laat" |
| Dubbele events | herhaling; op hol geslagen herinnering | id-dedupe, begrensd venster, echo alleen expliciet |
| Feedback-lus | event-explosie tussen ruimtes | ttl/confidence/urgency/parents/refractory/budget/circuit breaker |
| Partitionering | twee ruimtes divergeren | beide spelen door; bij rejoin digest, geen geschiedenis-replay; divergentie desnoods als artistiek effect documenteren |
| Discovery/config | ruimte niet gevonden | handmatige room-lijst eerst; mDNS/DNS-SD later |
| Verkeerd netwerk | broadcast-storm, latency | eigen VLAN/interface, geen publiek net; rate limits |
| Clock-master weg (audio) | audio-glitch bij iedereen | geen netwerkaudio in het eerste experiment |

## 6. Security en privacy

- **Alleen vertrouwd lokaal netwerk.** OSC 1.0 heeft geen authenticatie of encryptie; Art-Net en sACN zijn broadcast/multicast zonder beveiliging. Dus: eigen interface/VLAN, geen internet, geen port-forwarding, firewall aan. Netwerkmodus expliciet aan/uit, default uit.
- **Geen PII.** Ruimte-ids zijn opaque (nonce); geen namen, e-mail of stemopnames in telegrammen. `room` en `id` zijn geen identiteit. Logs lokaal, roterend, met grove tijdstempels.
- **Geen audio over de telegrambus.** Een telegram verwijst naar een gebeurtenis, niet naar samples. Audio blijft lokaal tenzij later expliciet netwerkaudio wordt gekozen.
- **Toestemming.** Deelnemers en omstanders weten dat er geluid wordt opgevangen en dat er berichten tussen ruimtes reizen. Mute/stop is lokaal en altijd beschikbaar; opgevangen materiaal is vluchtig tenzij bewust bewaard. Zie `docs/v2/SAFETY-PRIVACY.md`.
- **Geen cloud.** Geen discovery-service, geen telemetrie, geen remote logging, geen accounts. Een installatie moet zonder internet volledig werken.
- **DMX/Art-Net/sACN later alleen via een brug** op een geïsoleerd lighting-netwerk, nooit vanuit de plug-in en nooit op het publieke net. Art-Net (UDP 6454) en sACN (multicast) vervoeren DMX512-universes over IP. Het `priority`-veld in sACN is geen beveiliging. Als later auth nodig blijkt: pre-shared key plus HMAC op het telegram, niet in het eerste experiment.

## 7. Mogelijke onderzoeksvolgorde, nu expliciet niet bouwen

**Mogelijke onderzoeksvolgorde — geen backlog, geen fasering en geen commitment.**
De volgorde is alleen een logische volgorde als dit ooit wordt opgepakt; ze is
geen planning en geen releasecriterium. Eén stap tegelijk, en alleen na een
expliciete artistieke vraag.
1. Twee processen op één machine, loopback, met event-log, replay en injectie van verlies en duplicaten.
2. Twee machines, bekabeld, event-only; meten welke vertraging en welk verlies een herinnering nog geloofwaardig maken.
3. Drie ruimtes, star versus peer, met een telegram-viewer voor observatie.
4. Optionele brug naar Art-Net/sACN op een geïsoleerd net.
5. Klok-experiment: geen/NTP/PTP/Link, met gemeten fout en perceptief oordeel.
6. Pas daarna netwerkaudio (AES67-achtig) overwegen.

**Nu expliciet NIET.**
- Geen netwerkcode in de plug-in, laat staan in de audiodraad.
- Geen DMX/Art-Net/sACN-implementatie, ook geen "klein beginnetje".
- Geen AES67/Dante/AVB-endpoint, geen PTP-stack, geen Ableton Link-integratie.
- Geen cloud, accounts, remote discovery of internet.
- Geen sample-nauwe remote audio en geen "distributed orchestra".
- Geen UI of parameters voor distributie in v1.0.4.
- Geen wijziging aan `src/`, `CMakeLists.txt`, `presets/`, `README.md`, `.gitignore`.

## 8. Aanbeveling

1. Behandel distributie als **derde experiment**, volledig los van de plug-in, in de lijn van EXPERIMENT-01 §9 (twee kamers, alleen gebeurtenissen).
2. Start **event-only**, OSC over UDP, als **logische star** met een hub die logt en niet componeert; houd het protocol peer-ready.
3. Drie harde invarianten: **lokale autonomie**, **begrensde queues**, **geen feedback-amplificatie**. Lokaal stop/panic wint altijd.
4. Geen gedeelde klok in de eerste test; `t_origin` alleen informatief. NTP voor logs, PTP of Link pas na meting.
5. Geen netwerkaudio en geen DMX in deze fase. Werkt event-only artistiek niet, dan is de volgende stap een kleiner experiment, niet meer infrastructuur.
6. Publiceer het telegram-schema als versieerbaar contract en houd het transport vervangbaar.

**Grootste risico's.** Complexiteit die de artistieke vraag overschreeuwt; feedback-lussen die als "systeem" aanvoelen in plaats van als muziek; de verwachting van strakke sync bij performers en publiek; onbeveiligde OSC of DMX op een gedeeld netwerk. Alle vier zijn met de bovenstaande invarianten beheersbaar; geen ervan is een reden om nu te bouwen.

## 9. Open onderzoeksvragen (toetsbaar)

1. Bij welke vertraging voelt een remote gebeurtenis nog "verwant" en niet "toevallig"? (Ladder 0/50/100/200/500 ms en 2 s, N luisteraars.)
2. Hoeveel verlies (10/30/50%) verdraagt een ruimte voordat ze "afwezig" voelt?
3. Is intentionele drift van 0,5/1/2/5% hoorbaar of juist productief?
4. Wat is het juiste aandachtsbudget per ruimte (events/min) voordat het "druk" wordt?
5. Leest een duplicaat als echo of herinnering, of als bug?
6. Verandert star versus peer het ervaren gezag van een ruimte?
7. Wat is de minimale gedeelde klok: geen, NTP, PTP of Link, en wat is de gemeten fout?
8. Kan partitionering artistiek betekenisvol zijn (twee uiteenlopende herinneringen) of voelt ze als storing?
9. Bij hoeveel ruimtes (2/3/5) wordt het geheel onleesbaar?
10. Kun je horen uit welke ruimte een gebeurtenis kwam, of is herkomst expres onkenbaar?

## Bronnen

- OSC 1.0 — <https://ccrma.stanford.edu/groups/osc/spec-1_0.html> (messages, bundles, timetags, geen kloksync)
- Art-Net 4 — <https://art-net.org.uk/downloads/art-net.pdf> (UDP-poort 6454, max 44 Hz DMX512)
- ANSI E1.31 (sACN) — <https://tsp.esta.org/tsp/documents/docs/E1-31_2009.pdf> (multicast, priority 0-200, keep-alive 800-1000 ms)
- AES67-2018 PICS — <https://aes.org/wp-content/uploads/2024/05/AES67-2018-PICS-Summary_Merging-Technologies_HORUS_3.9_Audio-interface-to-and-from-AoIP-for-Recording.pdf> (1 ms packet time, 48 kHz, PTP-profielen)
- IEEE 1588-2019 (PTP) — <https://standards.ieee.org/ieee/1588/6825/> (sub-microseconde, profielen)
- NTPv4 RFC 5905 — <https://www.rfc-editor.org/info/rfc5905/> (tientallen µs haalbaar op moderne LAN's)
- Ableton Link — <https://ableton.github.io/link/> en <https://github.com/Ableton/link> (beat/fase, onafhankelijke tijdlijnen)
- Dante (propriëtair) — <https://en.wikipedia.org/wiki/Dante_(networking)>
- AVB/TSN — <https://en.wikipedia.org/wiki/Audio_Video_Bridging> (802.1AS gPTP, 1722 AVTP)
