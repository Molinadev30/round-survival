// Round Survival

#pragma once

#include "Engine/EngineTypes.h"

/**
 * The project's own collision channels, declared in `DefaultEngine.ini`. The engine only knows
 * them as numbered slots, so the numbers live here once rather than in every query.
 */

/**
 * Object type of barricade boards and the window frame around them. Nothing responds to it by
 * default: a board stops what its `RSBarricade` profile blocks (shots, the interaction trace,
 * physics bodies) and never a capsule, so boards change what can be shot and reached through a
 * window but not who can walk up to it.
 */
#define RS_ObjectChannel_Barricade ECC_GameTraceChannel1

/**
 * What the interaction trace looks along. Blocked by default, like `Visibility`, so a wall still
 * hides a locker behind it. Its own channel because a window has to answer it across the whole
 * opening, gaps and all, while letting shots through those same gaps.
 */
#define RS_TraceChannel_Interaction ECC_GameTraceChannel2
