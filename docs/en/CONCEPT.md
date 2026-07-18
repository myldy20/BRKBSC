# Product concept

## One sentence

BRKBSC is a handheld musical companion that listens to a performer, remembers a small amount of material, and takes useful musical roles around it.

## The problem

Most generative music tools have one of two weaknesses:

- they generate complete results while the musician becomes a spectator;
- they expose many synthesis parameters but do not understand the musical role the performer needs.

BRKBSC should remain an instrument. The performer supplies identity and intention. The machine supplies continuity, variation, and supporting roles.

## Two contexts, one musical memory

### Compose

A user hums or plays a short monophonic phrase. BRKBSC extracts pitch, note boundaries, dynamics, and onset timing. It estimates a plausible tonal centre and generates:

- a restrained drum pattern influenced by phrase attacks;
- a bass line that supports the inferred harmony;
- a chord pad;
- a short answering motif derived from the source phrase.

The result loops immediately. `B` creates another interpretation without deleting the phrase. The aim is not to finish a song automatically; it is to give the musician something concrete to react to within seconds.

### Live

The microphone remains open. BRKBSC follows the current pitch and attacks, then takes four possible roles:

- **Ground** — a slow tonal or noisy bed;
- **Shadow** — delayed and transformed remnants of the input;
- **Pulse** — sparse events driven by detected attacks;
- **Event** — occasional instability or contrast.

The first alpha implements simplified Ground, Shadow, and Pulse behaviours. Event logic will be added after the basic interaction proves useful.

## Product principles

1. **Input remains recognisable.** The output should feel genetically related to the musician.
2. **Silence is valid.** The system must not fill every gap.
3. **The machine takes roles, not control.** It supports rather than completes the composition.
4. **Every action is reversible.** A musician can freeze, mute, erase memory, or request another interpretation immediately.
5. **Failure must remain musical.** If analysis is uncertain, the system should hold a stable texture instead of producing erratic notes.
6. **No AI theatre.** A neural component is accepted only when it performs better than a simpler algorithm under Brick constraints.

## Core performance parameters

- **Agency** — how independently the system develops material;
- **Memory** — how far back it may reuse captured sound;
- **Density** — how much space it occupies;
- **Loyalty** — how recognisable the source remains;
- **Tension** — how likely it is to disturb the current state.

The alpha exposes Agency first. The others will appear only when their musical behaviour is defined and testable.

## Success criteria

BRKBSC is valuable when:

- a hummed phrase produces an accompaniment worth playing against;
- the live mode makes a solo passage or transition feel richer without becoming random noise;
- turning BRKBSC off leaves an obvious musical absence;
- the performer can understand the current state without reading a manual on stage;
- the device runs for an entire set without audio dropouts.
