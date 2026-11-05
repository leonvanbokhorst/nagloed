# Distributed NAGLOED — telegrams between rooms

> **Status:** exploration within NAGLOED v2, not a specification and not an implementation assignment. This document belongs to [ARTISTIC-VISION.md](ARTISTIC-VISION.md) and [EXPERIMENT-01.md](EXPERIMENT-01.md); it describes the third experiment (two rooms, events only) as a thought experiment. NAGLOED 1.0.4 remains unchanged. No network code in the plugin, no DMX implementation, no change to `src/`, `CMakeLists.txt`, `presets/`, `README.md` or `.gitignore`.

Markers: **Intent** = artistic purpose; **Technique** = proposal or fact; **Open question** = testable, not yet answered.

## 1. Artistic intent

- **Rooms as partially autonomous listeners.** A room is a NAGLOED instance with its own audio, light, tempo and attention. It is not a slave to a central timeline: it listens, responds and remembers locally, and can pass something on. Autonomy is the default; connection is the exception.
- **Events as telegrams.** What travels is not audio but a message: short, incomplete, with a timestamp and a degree of certainty. The telegram is the memory of an event, not the event itself. (**Intent**)
- **Memory travels between rooms.** A gesture in room A can return later in room B: lowered, delayed, misunderstood or not at all. Distance is compositional material, not latency to be eliminated.
- **Intentional drift and delay.** Rooms do not need to run in lock-step. A small tempo drift, a fixed delay or a deliberately missed response is a choice. Perfect sync is the default we want to avoid; this connects to "time: now, later, never" in ARTISTIC-VISION.
- **Inequality between rooms as material.** One room denser, brighter or more attentive than the other is contrast, not unfairness. A room that stays silent is as important as a room that answers.
- **What this is not.** No distributed orchestra in lock-step, no remote control from one master, no cloud session, no sample-accurate network audio as a starting point. (**Intent**)

## 2. Topology options

Definitions: **star** = one hub/broker; **peer** = direct unicast links without forwarding; **mesh** = any-to-any with forwarding or multicast.

| Topology | Advantages | Disadvantages | Artistic fit |
|---|---|---|---|
| Star (hub) | Simple; one place for logging, rate limits and dedupe; the hub need not be a performer | Single point of failure; the hub unintentionally becomes "master"; +1 hop | Good for the first test and installations with a technical operator |
| Peer (direct links) | No broker; rooms remain autonomous; predictable path | N² configuration; harder to observe; each room must have its own limits | Suits 2-3 rooms and equal performers |
| Mesh (multicast/forwarding) | Robust under failure; scales; natural "bus" | Loop detection, TTL, duplicates, broadcast storms; hard to reason through | Interesting later; too complex as a first step |

**Technique.** First experiment: a logical star with a hub that mainly *logs* and does not compose. Telegrams remain stateless and hub-independent, so that the same software can later run peer or mesh. Multicast is attractive as a bus (OSC over UDP can do this), but makes debugging and per-sender rate limiting harder. **Open question:** does a room's experienced authority change if the hub disappears or falls silent?

## 3. Minimal event telegram

Goal: small, versionable, loss- and duplicate-tolerant, and independent of the transport. The event vocabulary (light, sound, gesture, silence, remember, forget, echo, ignore) is in `docs/v2/ARCHITECTURE-OPTIONS.md`; here only the wire format.

| Field | Type | Meaning |
|---|---|---|
| `v` | u8 | protocol version (start 1) |
| `id` | u64 | unique id: nonce + monotonic counter; dedupe key |
| `room` | string ≤32 | room id, opaque, no personal name |
| `kind` | u8 | event kind from the vocabulary |
| `seed` | u32 | deterministic local interpretation |
| `t_origin` | OSC timetag | absolute time of origin; informative, not authoritative |
| `duration_ms` | u16 | intended duration |
| `color` | u8[3] | RGB 0-255 for light; 0 for non-light |
| `intensity` | f32 0-1 | strength, locally clamped |
| `confidence` | f32 0-1 | how certain the sender is that this deserves attention |
| `urgency` | f32 0-1 | how urgent; must never rise when forwarding |
| `ttl` | u8 | remaining hops; 0 = do not forward |
| `parents` | id[] ≤4 | causal chain; loop detection |
| `payload` | blob | optional, version-bound; no audio, no PII |

All integers big-endian, as in OSC. One telegram = one OSC message with a blob payload; the debug variant uses separate arguments.

```
/nagloed/event/v1/room/<room>/event      ,b   telegram (binary, see table)
/nagloed/event/v1/room/<room>/hello      ,st  room-id, protocol version
/nagloed/event/v1/room/<room>/heartbeat  ,i   latest event counter
/nagloed/event/v1/room/<room>/digest     ,b   summary after reconnecting
/nagloed/event/v1/room/<room>/control    ,s   mute | stop | rate
/nagloed/event/v1/session/panic          ,    local stop command (idempotent)
```

Why OSC: open, transport-independent and common in the music world; bundles are atomic and carry an NTP timetag. The spec explicitly states that OSC does *not* provide clock synchronisation, so use time only when it is reliable ([OSC 1.0](https://ccrma.stanford.edu/groups/osc/spec-1_0.html)).

**Rules (Technique).**
- **Rate limits.** Soft ≤ 2 events/s per room, hard ≤ 10/s, burst ≤ 20. The receiver has its own ingress ceiling and an attention budget per minute; exceeding it attenuates, it does not buffer without bound.
- **Bounded queues.** Ingress 256 telegrams, egress 64, no growth. Full → lowest confidence first, then oldest dropped. Never block; network I/O lives outside the audio thread (ARCHITECTURE.md forbids network access in `process()`).
- **Loss.** UDP is best-effort: no retransmission, no ACK. Loss is normal; a missed telegram is a missed memory. Heartbeat and digest after reconnecting instead of replay.
- **Duplicates.** Dedupe on `id` with a bounded window (e.g. 1024 ids / 10 s). Duplicate = ignore, unless deliberately as an `echo` with lower confidence. Idempotence is a hard requirement.
- **No runaway feedback.** `ttl` decreases per hop (start 3); `confidence` × 0.5 per hop, drop below 0.2; `urgency` never rises; `parents` bounded to 4; own id in `parents` → drop; refractory per room per `kind` (250-500 ms); global circuit breaker at > 50 events/s ingress (30 s mute + log). "No answer" is the default, not the exception.

## 4. Time strategy

**Intent.** Local autonomy is the default. Every room has its own clock, tempo and breath. A shared clock is an option for a specific effect, not the foundation.

| Clock | Accuracy (order) | Usable for | Not for |
|---|---|---|---|
| Local monotonic clock | no mutual relation | all local scheduling and refractory periods | ordering between rooms |
| NTP (RFC 5905) | ms on LAN; tens of µs possible | logging, dedupe window, "roughly equal" | tight joint events |
| PTP / IEEE 1588 | sub-microsecond | network audio, sample-accurate arrangements | simple installation; requires PTP switch/NIC |
| Ableton Link | musical beat/phase, not wall clock | shared pulse without lock-step | absolute telegram time |

**Technique.** Use `t_origin` only as information; schedule locally with the monotonic clock and relative delay. On drift: 10 ppm ≈ 0.86 s/day, 50 ppm ≈ 4.3 s/day. Let a room deliberately run 1-5% apart from another if that works musically; make drift a parameter, not an error.

**What may be late, what may not.** Light on a slow colour field may be tens of milliseconds late (**Open question**, to be tested perceptually). A local sound must never wait for the network: the audio thread always keeps rendering. A remotely triggered sound is only possible with pre-planning and remains bounded by clock error plus jitter; without a shared clock, sample accuracy is an illusion. An emergency stop/panic is local and immediate and never depends on the network.

**Event-only versus network audio.**

| | Event-only | Network audio (AES67/Dante/AVB) |
|---|---|---|
| Clock | local, separate | one PTP domain required |
| Complexity | low | high: switches, QoS, latency budget, discovery |
| Failure mode | room falls back on itself | loss of clock master affects everyone |
| Artistic meaning | memory travels, audio stays local | shared acoustic material, true spatial composition |
| Advice | start here | later, only if event-only falls short artistically |

AES67 typically uses 1 ms packet time, 48 kHz and a PTP profile (IEEE 1588 default, AES67 media or 802.1AS). Dante is proprietary (Audinate; low latency, many channels). AVB is an IEEE standards set with gPTP and AVTP at layer 2. All three solve a different problem than "letting a telegram travel".

## 5. Failure modes

| Failure mode | Effect | Measure |
|---|---|---|
| Network loss | telegrams disappear; room becomes quieter | accept best-effort; heartbeat; digest on reconnect; no replay storm |
| Clock drift or jump | wrong order; "late" events | `t_origin` informative; monotonic clock locally; threshold on "too late" |
| Duplicate events | repetition; memory running wild | id dedupe, bounded window, echo only explicit |
| Feedback loop | event explosion between rooms | ttl/confidence/urgency/parents/refractory/budget/circuit breaker |
| Partitioning | two rooms diverge | both keep playing; on rejoin digest, no history replay; if necessary document divergence as an artistic effect |
| Discovery/config | room not found | manual room list first; mDNS/DNS-SD later |
| Wrong network | broadcast storm, latency | own VLAN/interface, no public network; rate limits |
| Clock master gone (audio) | audio glitch for everyone | no network audio in the first experiment |

## 6. Security and privacy

- **Trusted local network only.** OSC 1.0 has no authentication or encryption; Art-Net and sACN are broadcast/multicast without security. So: own interface/VLAN, no internet, no port forwarding, firewall on. Network mode explicitly on/off, default off.
- **No PII.** Room ids are opaque (nonce); no names, e-mail or voice recordings in telegrams. `room` and `id` are not identity. Logs local, rotating, with coarse timestamps.
- **No audio over the telegram bus.** A telegram refers to an event, not to samples. Audio stays local unless network audio is explicitly chosen later.
- **Consent.** Participants and bystanders know that sound is being captured and that messages travel between rooms. Mute/stop is local and always available; captured material is ephemeral unless deliberately kept. See `docs/v2/SAFETY-PRIVACY.md`.
- **No cloud.** No discovery service, no telemetry, no remote logging, no accounts. An installation must work fully without internet.
- **DMX/Art-Net/sACN only later via a bridge** on an isolated lighting network, never from the plugin and never on the public network. Art-Net (UDP 6454) and sACN (multicast) carry DMX512 universes over IP. The `priority` field in sACN is not security. If authentication turns out to be needed later: pre-shared key plus HMAC on the telegram, not in the first experiment.

## 7. Possible research order, explicitly not to build now

**Possible research order — no backlog, no phasing and no commitment.**
The order is only a logical sequence if this is ever taken up; it is not a
plan and not a release criterion. One step at a time, and only after an
explicit artistic question.
1. Two processes on one machine, loopback, with event log, replay and injection of loss and duplicates.
2. Two machines, wired, event-only; measure which delay and which loss still make a memory credible.
3. Three rooms, star versus peer, with a telegram viewer for observation.
4. Optional bridge to Art-Net/sACN on an isolated network.
5. Clock experiment: none/NTP/PTP/Link, with measured error and perceptual judgement.
6. Only then consider network audio (AES67-like).

**Explicitly NOT now.**
- No network code in the plugin, let alone in the audio thread.
- No DMX/Art-Net/sACN implementation, not even a "small start".
- No AES67/Dante/AVB endpoint, no PTP stack, no Ableton Link integration.
- No cloud, accounts, remote discovery or internet.
- No sample-accurate remote audio and no "distributed orchestra".
- No UI or parameters for distribution in v1.0.4.
- No change to `src/`, `CMakeLists.txt`, `presets/`, `README.md`, `.gitignore`.

## 8. Recommendation

1. Treat distribution as the **third experiment**, entirely separate from the plugin, in the line of EXPERIMENT-01 §9 (two rooms, events only).
2. Start **event-only**, OSC over UDP, as a **logical star** with a hub that logs and does not compose; keep the protocol peer-ready.
3. Three hard invariants: **local autonomy**, **bounded queues**, **no feedback amplification**. Local stop/panic always wins.
4. No shared clock in the first test; `t_origin` informative only. NTP for logs, PTP or Link only after measurement.
5. No network audio and no DMX in this phase. If event-only does not work artistically, the next step is a smaller experiment, not more infrastructure.
6. Publish the telegram schema as a versionable contract and keep the transport replaceable.

**Biggest risks.** Complexity that drowns out the artistic question; feedback loops that feel like a "system" rather than like music; the expectation of tight sync among performers and audience; unsecured OSC or DMX on a shared network. All four are manageable with the invariants above; none of them is a reason to build now.

## 9. Open research questions (testable)

1. At what delay does a remote event still feel "related" and not "coincidental"? (Ladder 0/50/100/200/500 ms and 2 s, N listeners.)
2. How much loss (10/30/50%) can a room tolerate before it feels "absent"?
3. Is intentional drift of 0.5/1/2/5% audible or actually productive?
4. What is the right attention budget per room (events/min) before it becomes "busy"?
5. Does a duplicate read as echo or memory, or as a bug?
6. Does star versus peer change the experienced authority of a room?
7. What is the minimum shared clock: none, NTP, PTP or Link, and what is the measured error?
8. Can partitioning be artistically meaningful (two diverging memories) or does it feel like a fault?
9. At how many rooms (2/3/5) does the whole become illegible?
10. Can you hear which room an event came from, or is origin deliberately unknowable?

## Sources

- OSC 1.0 — <https://ccrma.stanford.edu/groups/osc/spec-1_0.html> (messages, bundles, timetags, no clock sync)
- Art-Net 4 — <https://art-net.org.uk/downloads/art-net.pdf> (UDP port 6454, max 44 Hz DMX512)
- ANSI E1.31 (sACN) — <https://tsp.esta.org/tsp/documents/docs/E1-31_2009.pdf> (multicast, priority 0-200, keep-alive 800-1000 ms)
- AES67-2018 PICS — <https://aes.org/wp-content/uploads/2024/05/AES67-2018-PICS-Summary_Merging-Technologies_HORUS_3.9_Audio-interface-to-and-from-AoIP-for-Recording.pdf> (1 ms packet time, 48 kHz, PTP profiles)
- IEEE 1588-2019 (PTP) — <https://standards.ieee.org/ieee/1588/6825/> (sub-microsecond, profiles)
- NTPv4 RFC 5905 — <https://www.rfc-editor.org/info/rfc5905/> (tens of µs achievable on modern LANs)
- Ableton Link — <https://ableton.github.io/link/> and <https://github.com/Ableton/link> (beat/phase, independent timelines)
- Dante (proprietary) — <https://en.wikipedia.org/wiki/Dante_(networking)>
- AVB/TSN — <https://en.wikipedia.org/wiki/Audio_Video_Bridging> (802.1AS gPTP, 1722 AVTP)
