#include "MallNav.h"

#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"

int32 FMallNavGraph::Add(const FVector& Pos)
{
	FMallNavNode N;
	N.Pos = Pos;
	return Nodes.Add(N);
}

void FMallNavGraph::Link(int32 A, int32 B)
{
	if (!Nodes.IsValidIndex(A) || !Nodes.IsValidIndex(B) || A == B)
	{
		return;
	}
	Nodes[A].Links.AddUnique(B);
	Nodes[B].Links.AddUnique(A);
}

void FMallNavGraph::AutoLink(UWorld* World, float MaxDist, float Radius)
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallNavLink), false);
	const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
	for (int32 A = 0; A < Nodes.Num(); ++A)
	{
		for (int32 B = A + 1; B < Nodes.Num(); ++B)
		{
			const FVector PA = Nodes[A].Pos, PB = Nodes[B].Pos;
			if (FMath::Abs(PA.Z - PB.Z) > 40.f || FVector::DistSquared(PA, PB) > MaxDist * MaxDist)
			{
				continue;
			}
			FHitResult Hit;
			// Sweep at knee and chest height so low fixtures and wall openings are both respected.
			const bool bLow = World->SweepSingleByObjectType(Hit, PA - FVector(0, 0, 50), PB - FVector(0, 0, 50), FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), Shape, Params);
			if (bLow)
			{
				continue;
			}
			const bool bHigh = World->SweepSingleByObjectType(Hit, PA + FVector(0, 0, 50), PB + FVector(0, 0, 50), FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), Shape, Params);
			if (bHigh)
			{
				continue;
			}
			// Make sure there is floor along the way (no linking across the atrium void).
			bool bFloor = true;
			for (int32 S = 1; S < 8 && bFloor; ++S)
			{
				const FVector P = FMath::Lerp(PA, PB, S / 8.f);
				FHitResult FloorHit;
				bFloor = World->LineTraceSingleByObjectType(FloorHit, P, P - FVector(0, 0, 160), FCollisionObjectQueryParams(ECC_WorldStatic), Params);
			}
			if (bFloor)
			{
				Link(A, B);
			}
		}
	}
}

int32 FMallNavGraph::Nearest(UWorld* World, const FVector& Pos, bool bRequireClear) const
{
	int32 Best = INDEX_NONE, BestAny = INDEX_NONE;
	float BestD = FLT_MAX, BestAnyD = FLT_MAX;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallNavNearest), false);
	// Collect a short list of candidates by distance, weighting vertical separation heavily.
	TArray<TPair<float, int32>> Cands;
	for (int32 I = 0; I < Nodes.Num(); ++I)
	{
		const FVector D = Nodes[I].Pos - Pos;
		const float Score = FVector(D.X, D.Y, D.Z * 4.f).SizeSquared();
		Cands.Add({Score, I});
	}
	Cands.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	for (int32 C = 0; C < FMath::Min(8, Cands.Num()); ++C)
	{
		const int32 I = Cands[C].Value;
		if (Cands[C].Key < BestAnyD)
		{
			BestAnyD = Cands[C].Key;
			BestAny = I;
		}
		if (!bRequireClear || !World)
		{
			return BestAny;
		}
		FHitResult Hit;
		if (!World->LineTraceSingleByObjectType(Hit, Pos, Nodes[I].Pos, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			if (Cands[C].Key < BestD)
			{
				BestD = Cands[C].Key;
				Best = I;
				break;
			}
		}
	}
	return Best != INDEX_NONE ? Best : BestAny;
}

bool FMallNavGraph::FindPath(int32 Start, int32 Goal, TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	if (!Nodes.IsValidIndex(Start) || !Nodes.IsValidIndex(Goal))
	{
		return false;
	}
	if (Start == Goal)
	{
		OutPoints.Add(Nodes[Goal].Pos);
		return true;
	}
	const int32 N = Nodes.Num();
	TArray<float> G;
	G.Init(FLT_MAX, N);
	TArray<int32> Came;
	Came.Init(INDEX_NONE, N);
	TArray<bool> Closed;
	Closed.Init(false, N);
	TArray<TPair<float, int32>> Open;
	G[Start] = 0;
	Open.HeapPush({FVector::Dist(Nodes[Start].Pos, Nodes[Goal].Pos), Start}, [](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	while (Open.Num() > 0)
	{
		TPair<float, int32> Cur;
		Open.HeapPop(Cur, [](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
		const int32 C = Cur.Value;
		if (Closed[C])
		{
			continue;
		}
		if (C == Goal)
		{
			break;
		}
		Closed[C] = true;
		for (int32 L : Nodes[C].Links)
		{
			const float NG = G[C] + FVector::Dist(Nodes[C].Pos, Nodes[L].Pos);
			if (NG < G[L])
			{
				G[L] = NG;
				Came[L] = C;
				Open.HeapPush({NG + FVector::Dist(Nodes[L].Pos, Nodes[Goal].Pos), L}, [](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
			}
		}
	}
	if (Came[Goal] == INDEX_NONE)
	{
		return false;
	}
	for (int32 C = Goal; C != INDEX_NONE; C = Came[C])
	{
		OutPoints.Insert(Nodes[C].Pos, 0);
	}
	return true;
}

int32 FMallNavGraph::NumLinks() const
{
	int32 Count = 0;
	for (const FMallNavNode& N : Nodes)
	{
		Count += N.Links.Num();
	}
	return Count / 2;
}

void FMallNavGraph::PruneToComponent(int32 Seed)
{
	if (!Nodes.IsValidIndex(Seed))
	{
		return;
	}
	TArray<bool> Keep;
	Keep.Init(false, Nodes.Num());
	TArray<int32> Stack;
	Stack.Add(Seed);
	Keep[Seed] = true;
	while (Stack.Num() > 0)
	{
		const int32 C = Stack.Pop();
		for (int32 L : Nodes[C].Links)
		{
			if (!Keep[L])
			{
				Keep[L] = true;
				Stack.Add(L);
			}
		}
	}
	TArray<int32> Remap;
	Remap.Init(INDEX_NONE, Nodes.Num());
	TArray<FMallNavNode> Kept;
	for (int32 I = 0; I < Nodes.Num(); ++I)
	{
		if (Keep[I])
		{
			Remap[I] = Kept.Add(Nodes[I]);
		}
	}
	for (FMallNavNode& N : Kept)
	{
		TArray<int32> Links;
		for (int32 L : N.Links)
		{
			if (Remap[L] != INDEX_NONE)
			{
				Links.Add(Remap[L]);
			}
		}
		N.Links = MoveTemp(Links);
	}
	Nodes = MoveTemp(Kept);
}
