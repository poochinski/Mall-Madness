#pragma once

#include "CoreMinimal.h"

class UWorld;

struct FMallNavNode
{
	FVector Pos;            // capsule-centre height (cm)
	TArray<int32> Links;
};

/**
 * Hand-placed waypoint graph for the fixed mall layout. Nodes are placed by the builder;
 * same-floor edges are generated automatically with capsule sweeps, while stair and
 * escalator edges are linked explicitly. A* runs over a few hundred nodes, so it is cheap.
 */
struct FMallNavGraph
{
	TArray<FMallNavNode> Nodes;

	int32 Add(const FVector& Pos);
	void Link(int32 A, int32 B);
	/** Connect every pair of nodes on the same level within MaxDist whose sweep is unobstructed. */
	void AutoLink(UWorld* World, float MaxDist, float Radius);
	/** Closest node with a clear line from Pos (falls back to closest overall). */
	int32 Nearest(UWorld* World, const FVector& Pos, bool bRequireClear = true) const;
	bool FindPath(int32 Start, int32 Goal, TArray<FVector>& OutPoints) const;
	int32 NumLinks() const;
	/** Drop every node not reachable from Seed (furniture tops, sealed pockets) and reindex links. */
	void PruneToComponent(int32 Seed);
};
