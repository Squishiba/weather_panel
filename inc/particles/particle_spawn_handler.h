//
// Created by Jared on 6/7/2026.
//

#ifndef WEATHER_PARTICLE_SPAWN_HANDLER_H
#define WEATHER_PARTICLE_SPAWN_HANDLER_H

#include "display/segment/segment_geometry.h"
#include "display/segment/force_edge_map.h"
#include "physics/gravity.h"

// Owns the spawn/floor edge maps for a segment. Depends only on geometry and a
// direction vector (a ForceLike value) — it knows nothing about the force
// collection or how forces are stored. The one coupling to the force system is
// update_spawn_edge(), which is a plain value handoff.
class ParticleSpawnHandler
	{

	BoundLimits &seg_bounds_;			// Inner box where particles are allowed to exist.
	Oob_Limits &oob_limits_;			// The outer box where, once crossed, particles are reset.

public:

	ForceEdgeMap spawn_span; // Recomputed when the resultant direction changes.
	ForceEdgeMap floor_span; // Where particles land (opposite gravity).

	explicit ParticleSpawnHandler(Gravity& gravity, BoundLimits &bounds, Oob_Limits &oob_limits)
		: seg_bounds_(bounds), oob_limits_(oob_limits)
		{
		spawn_span.map_force_from_edge(seg_bounds_, gravity, oob_limits_.bounds_expansion_val);
		floor_span.map_force_to_edge(seg_bounds_, gravity, 0);
		}

	// Recompute the spawn edge from a resultant direction vector — produced by
	// SegmentWideForceHandler::calculate_combined(). Call whenever the uniform
	// forces change (gravity slider, wind field on/off).
	void update_spawn_edge(const ForceLike& resultant)
		{
		spawn_span.map_force_from_edge(seg_bounds_, resultant, oob_limits_.bounds_expansion_val);
		}

	};

#endif //WEATHER_PARTICLE_SPAWN_HANDLER_H
