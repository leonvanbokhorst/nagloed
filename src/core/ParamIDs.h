#pragma once

// Stable parameter identifiers. NEVER rename these in a release: automation,
// presets and saved sessions all address parameters by these strings.

namespace nagloed::pid
{
// --- Sintel: the grain engine -----------------------------------------------
inline constexpr const char* density     = "grain.density";
inline constexpr const char* size        = "grain.size";
inline constexpr const char* spray       = "grain.spray";
inline constexpr const char* scan        = "grain.scan";
inline constexpr const char* pitch       = "grain.pitch";
inline constexpr const char* spread      = "grain.spread";
inline constexpr const char* reverse     = "grain.reverse";
inline constexpr const char* jitter      = "grain.jitter";
inline constexpr const char* shape       = "grain.shape";
inline constexpr const char* width       = "grain.width";
inline constexpr const char* feedback    = "grain.feedback";
inline constexpr const char* focus       = "grain.focus";
inline constexpr const char* freeze      = "grain.freeze";
inline constexpr const char* tune        = "grain.tune";
inline constexpr const char* source      = "grain.source";

// --- Ruimte: the space ------------------------------------------------------
inline constexpr const char* spaceMix    = "space.mix";
inline constexpr const char* spaceSize   = "space.size";
inline constexpr const char* spaceDamp   = "space.damp";
inline constexpr const char* spaceMod    = "space.mod";

// --- EAR: emotional analog resonance ---------------------------------------
inline constexpr const char* drive       = "ear.drive";
inline constexpr const char* bias        = "ear.bias";
inline constexpr const char* tone        = "ear.tone";
inline constexpr const char* cutoff      = "ear.cutoff";
inline constexpr const char* reso        = "ear.reso";
inline constexpr const char* drift       = "ear.drift";
inline constexpr const char* wow         = "ear.wow";
inline constexpr const char* tape        = "ear.tape";
inline constexpr const char* lofi        = "ear.lofi";
inline constexpr const char* hiss        = "ear.hiss";
inline constexpr const char* input       = "ear.input";
inline constexpr const char* output      = "ear.output";
inline constexpr const char* mix         = "ear.mix";

// --- Gesture / modulation ---------------------------------------------------
inline constexpr const char* macroGlow    = "macro.glow";
inline constexpr const char* macroDrift   = "macro.drift";
inline constexpr const char* macroGrain   = "macro.grain";
inline constexpr const char* macroSpace   = "macro.space";
inline constexpr const char* macroShimmer = "macro.shimmer";

inline constexpr const char* lfoRate      = "mod.lfoRate";
inline constexpr const char* lfoShape     = "mod.lfoShape";
inline constexpr const char* lfoDest      = "mod.lfoDest";
inline constexpr const char* lfoAmount    = "mod.lfoAmount";
inline constexpr const char* autoMotion   = "mod.auto";
inline constexpr const char* pressureDest = "mod.pressure";
inline constexpr const char* slideDest    = "mod.slide";
inline constexpr const char* expression   = "mod.expression";
} // namespace nagloed::pid
