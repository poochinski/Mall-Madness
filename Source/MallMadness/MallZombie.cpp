#include "MallZombie.h"

#include "AIController.h"
#include "MallAudio.h"
#include "MallBuilder.h"
#include "MallGameMode.h"
#include "MallMaterials.h"
#include "MallPlayer.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/BlendSpace.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AMallZombie::AMallZombie()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::Spawned;

	GetCapsuleComponent()->InitCapsuleSize(32.f, 90.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	USkeletalMeshComponent* Body = GetMesh();
	Body->SetRelativeLocationAndRotation(FVector(0, 0, -90.f), FRotator(0, -90.f, 0));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionObjectType(ECC_PhysicsBody);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Body->SetGenerateOverlapEvents(false);

	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* CM = GetCharacterMovement();
	CM->bOrientRotationToMovement = true;
	CM->RotationRate = FRotator(0, 300.f, 0);
	CM->MaxAcceleration = 900.f;
	CM->BrakingDecelerationWalking = 1200.f;
	CM->bUseRVOAvoidance = true;
	CM->AvoidanceConsiderationRadius = 140.f;
	CM->AvoidanceWeight = 0.5f;
	CM->MaxStepHeight = 40.f;
	CM->SetWalkableFloorAngle(48.f);
	CM->bUseControllerDesiredRotation = false;
}

AMallPlayer* AMallZombie::GetPlayer() const
{
	AMallGameMode* GM = GetWorld()->GetAuthGameMode<AMallGameMode>();
	return GM ? GM->GetPlayer() : nullptr;
}

const FMallNavGraph* AMallZombie::GetNav() const
{
	AMallGameMode* GM = GetWorld()->GetAuthGameMode<AMallGameMode>();
	return GM && GM->Builder ? &GM->Builder->Nav : nullptr;
}

void AMallZombie::Setup(EZombieKind InKind, int32 Seed)
{
	Kind = InKind;
	FRandomStream R(Seed);
	Variant = R.RandRange(0, 99);

	const bool bQuinn = R.FRand() < 0.45f;
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, bQuinn
		? TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple")
		: TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	GetMesh()->SetSkeletalMesh(Mesh);

	LocoAnim = LoadObject<UAnimationAsset>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run.BS_Idle_Walk_Run"));
	for (const TCHAR* P : {TEXT("MM_Attack_01"), TEXT("MM_Attack_02"), TEXT("MM_Attack_03")})
	{
		if (UAnimationAsset* A = LoadObject<UAnimationAsset>(nullptr, *FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/%s.%s"), P, P))) AttackAnims.Add(A);
	}
	for (const TCHAR* P : {TEXT("MM_HitReact_Front_Lgt_01"), TEXT("MM_HitReact_Front_Lgt_02"), TEXT("MM_HitReact_Front_Med_01"), TEXT("MM_HitReact_Front_Lgt_03")})
	{
		if (UAnimationAsset* A = LoadObject<UAnimationAsset>(nullptr, *FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/%s.%s"), P, P))) HitAnims.Add(A);
	}
	for (const TCHAR* P : {TEXT("MM_Death_Front_01"), TEXT("MM_Death_Front_02"), TEXT("MM_Death_Front_03"), TEXT("MM_Death_Back_01"), TEXT("MM_Death_Left_01"), TEXT("MM_Death_Right_01")})
	{
		if (UAnimationAsset* A = LoadObject<UAnimationAsset>(nullptr, *FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/Death/%s.%s"), P, P))) DeathAnims.Add(A);
	}

	// Look: grimy, desaturated skin with clothing colours chosen per archetype.
	UMallMaterials* Mats = UMallMaterials::Get();
	static const FLinearColor ShopperColors[] = {
		FLinearColor(0.35f, 0.08f, 0.07f), FLinearColor(0.10f, 0.16f, 0.30f), FLinearColor(0.42f, 0.40f, 0.34f),
		FLinearColor(0.12f, 0.22f, 0.14f), FLinearColor(0.30f, 0.22f, 0.12f), FLinearColor(0.20f, 0.20f, 0.22f),
		FLinearColor(0.45f, 0.30f, 0.32f), FLinearColor(0.06f, 0.06f, 0.07f)};
	FLinearColor Cloth = ShopperColors[R.RandRange(0, 7)];
	if (Kind == EZombieKind::Brute) Cloth = FLinearColor(0.05f, 0.07f, 0.12f);           // security uniform
	if (Kind == EZombieKind::Runner && R.FRand() < 0.5f) Cloth = FLinearColor(0.45f, 0.12f, 0.03f); // food-court staff
	const float SkinTone = R.FRandRange(0.75f, 1.15f);
	static const FLinearColor PantsColors[] = {
		FLinearColor(0.06f, 0.08f, 0.14f), FLinearColor(0.12f, 0.11f, 0.10f), FLinearColor(0.25f, 0.22f, 0.16f), FLinearColor(0.03f, 0.03f, 0.035f)};
	const FLinearColor Pants = Kind == EZombieKind::Brute ? FLinearColor(0.04f, 0.05f, 0.09f) : PantsColors[R.RandRange(0, 3)];
	const FLinearColor Skin = FLinearColor(0.34f, 0.35f, 0.28f) * SkinTone;
	for (int32 I = 0; I < GetMesh()->GetNumMaterials(); ++I)
	{
		GetMesh()->SetMaterial(I, Mats->MakeZombie(Skin, Cloth, Pants, 2.f + R.FRand() * 2.f));
	}

	float Scale = R.FRandRange(0.94f, 1.05f);
	switch (Kind)
	{
	case EZombieKind::Walker:
		Health = 100.f; Speed = R.FRandRange(105.f, 150.f); AttackDamage = 16.f; AttackCooldown = 1.6f;
		AttackWindup = 0.5f; AttackLength = 1.15f; SightRange = 2300.f; AnimRate = 0.72f;
		break;
	case EZombieKind::Runner:
		Health = 70.f; Speed = R.FRandRange(420.f, 480.f); AttackDamage = 11.f; AttackCooldown = 0.95f;
		AttackWindup = 0.3f; AttackLength = 0.75f; SightRange = 3200.f; AnimRate = 1.05f;
		GetCharacterMovement()->RotationRate = FRotator(0, 540.f, 0);
		GetCharacterMovement()->MaxAcceleration = 1800.f;
		break;
	case EZombieKind::Brute:
		Health = 320.f; Speed = R.FRandRange(165.f, 190.f); AttackDamage = 30.f; AttackCooldown = 1.9f;
		AttackWindup = 0.6f; AttackLength = 1.3f; SightRange = 2600.f; AnimRate = 0.85f; Scale = 1.13f;
		break;
	}
	SetActorScale3D(FVector(Scale));
	GetCharacterMovement()->MaxWalkSpeed = Speed;

	if (Kind == EZombieKind::Brute)
	{
		// Security cap so the tough variant reads at a glance.
		UStaticMeshComponent* Cap = NewObject<UStaticMeshComponent>(this);
		Cap->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		Cap->SetMaterial(0, Mats->Mat("fabric_black"));
		Cap->SetupAttachment(GetMesh(), TEXT("head"));
		Cap->SetRelativeTransform(FTransform(FRotator(0, 0, 90), FVector(10.f, 2.f, 0), FVector(0.21f, 0.21f, 0.07f)));
		Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cap->RegisterComponent();
	}

	WanderHome = GetActorLocation();
	VoiceTimer = R.FRandRange(1.f, 8.f);
	ThinkTimer = R.FRandRange(0.f, 0.3f);
	State = EZombieState::Wander;
	PlayLocomotion();
}

void AMallZombie::PlayLocomotion()
{
	if (LocoAnim)
	{
		GetMesh()->PlayAnimation(LocoAnim, true);
		if (UAnimSingleNodeInstance* SN = GetMesh()->GetSingleNodeInstance())
		{
			SN->SetPlayRate(AnimRate);
			SN->SetPosition(FMath::FRandRange(0.f, 1.f), false);
		}
		bLocomotionActive = true;
	}
}

void AMallZombie::PlayOnce(UAnimationAsset* Anim, float Rate)
{
	if (!Anim)
	{
		return;
	}
	bLocomotionActive = false;
	GetMesh()->PlayAnimation(Anim, false);
	if (UAnimSingleNodeInstance* SN = GetMesh()->GetSingleNodeInstance())
	{
		SN->SetPlayRate(Rate);
	}
}

bool AMallZombie::CanSeePlayer(AMallPlayer* P) const
{
	if (!P || P->IsDead())
	{
		return false;
	}
	const FVector Eye = GetActorLocation() + FVector(0, 0, 70.f);
	const FVector Target = P->GetEyeLocation() - FVector(0, 0, 20.f);
	const FVector To = Target - Eye;
	const float Dist = To.Size();
	float Range = SightRange * (P->bIsCrouched ? 0.65f : 1.f);
	if (State == EZombieState::Chase || State == EZombieState::Attack)
	{
		Range *= 1.5f;   // once locked on they keep tracking
	}
	if (Dist > Range)
	{
		return false;
	}
	if (Dist > 350.f && State != EZombieState::Chase)
	{
		if (FVector::DotProduct(GetActorForwardVector(), To / Dist) < 0.34f)
		{
			return false;
		}
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieSight), false, this);
	FCollisionObjectQueryParams Obj;
	Obj.AddObjectTypesToQuery(ECC_WorldStatic);
	return !GetWorld()->LineTraceSingleByObjectType(Hit, Eye, Target, Obj, Params);
}

bool AMallZombie::DirectReachable(const FVector& From, const FVector& To) const
{
	if (FMath::Abs(From.Z - To.Z) > 110.f)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieDirect), false, this);
	const FCollisionShape Shape = FCollisionShape::MakeSphere(26.f);
	FHitResult Hit;
	const FCollisionObjectQueryParams Obj(ECC_WorldStatic);
	if (GetWorld()->SweepSingleByObjectType(Hit, From - FVector(0, 0, 50), To - FVector(0, 0, 50), FQuat::Identity, Obj, Shape, Params))
	{
		return false;
	}
	if (GetWorld()->SweepSingleByObjectType(Hit, From + FVector(0, 0, 40), To + FVector(0, 0, 40), FQuat::Identity, Obj, Shape, Params))
	{
		return false;
	}
	for (int32 S = 1; S <= 3; ++S)
	{
		const FVector P = FMath::Lerp(From, To, S / 4.f);
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, P, P - FVector(0, 0, 170.f), Obj, Params))
		{
			return false;
		}
	}
	return true;
}

void AMallZombie::PlanTo(const FVector& Target)
{
	TargetLoc = Target;
	const FVector Loc = GetActorLocation();
	Path.Reset();
	PathIndex = 0;
	if (DirectReachable(Loc, Target))
	{
		Path.Add(Target);
		return;
	}
	const FMallNavGraph* Nav = GetNav();
	if (!Nav)
	{
		return;
	}
	const int32 A = Nav->Nearest(GetWorld(), Loc);
	const int32 B = Nav->Nearest(GetWorld(), Target);
	if (Nav->FindPath(A, B, Path))
	{
		// Skip the first node if we can already head straight for the second one.
		if (Path.Num() > 1 && DirectReachable(Loc, Path[1]))
		{
			Path.RemoveAt(0);
		}
		Path.Add(Target);
	}
}

void AMallZombie::HearNoise(const FVector& Location, float Radius)
{
	if (IsDead() || State == EZombieState::Attack)
	{
		return;
	}
	const float Dist = FVector::Dist(Location, GetActorLocation());
	if (Dist > Radius)
	{
		return;
	}
	if (Dist < Radius * 0.45f)
	{
		// Close and loud: they know roughly where the player is.
		LastSeenTime = Age;
		if (State != EZombieState::Chase && State != EZombieState::Stagger)
		{
			State = EZombieState::Chase;
			StateTimer = 0.f;
		}
		TargetLoc = Location;
		RepathTimer = 0.f;
		return;
	}
	if (State != EZombieState::Chase)
	{
		State = EZombieState::Investigate;
		StateTimer = 0.f;
		PlanTo(Location + FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), 0));
	}
}

bool AMallZombie::ApplyHit(float Damage, bool bHead, const FVector& Dir, AActor* Instigator)
{
	if (IsDead())
	{
		return false;
	}
	Health -= Damage;
	UMallAudio::Get()->PlayAt(GetWorld(), "hit_flesh", GetActorLocation() + FVector(0, 0, 40), 0.6f, FMath::FRandRange(0.85f, 1.15f), 2000.f);
	if (Health <= 0.f)
	{
		Die(Dir);
		return true;
	}
	if (Instigator)
	{
		TargetLoc = Instigator->GetActorLocation();
		LastSeenTime = Age;
		RepathTimer = 0.f;
	}
	const bool bStagger = Kind == EZombieKind::Brute ? (bHead && Damage > 50.f) : (Damage >= 24.f || bHead);
	if (bStagger && State != EZombieState::Attack && HitAnims.Num() > 0)
	{
		State = EZombieState::Stagger;
		StateTimer = 0.f;
		PlayOnce(HitAnims[FMath::RandRange(0, HitAnims.Num() - 1)], 1.3f);
	}
	else if (State != EZombieState::Attack && State != EZombieState::Stagger)
	{
		State = EZombieState::Chase;
	}
	return false;
}

void AMallZombie::Die(const FVector& Dir)
{
	State = EZombieState::Dead;
	DeathTime = Age;
	Path.Reset();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->SetAvoidanceEnabled(false);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
	if (DeathAnims.Num() > 0)
	{
		// Choose a fall direction from the shot direction relative to our facing.
		const float FwdDot = FVector::DotProduct(GetActorForwardVector(), Dir);
		const float RightDot = FVector::DotProduct(GetActorRightVector(), Dir);
		int32 Index = FMath::RandRange(0, 2);
		if (FwdDot > 0.5f) Index = 3;
		else if (RightDot > 0.6f) Index = 4;
		else if (RightDot < -0.6f) Index = 5;
		PlayOnce(DeathAnims[FMath::Min(Index, DeathAnims.Num() - 1)], 1.f);
	}
	UMallAudio::Get()->PlayAt(GetWorld(), "zdeath", GetActorLocation() + FVector(0, 0, 60), 0.7f, FMath::FRandRange(0.8f, 1.1f), 2500.f);
	if (AMallGameMode* GM = GetWorld()->GetAuthGameMode<AMallGameMode>())
	{
		GM->OnZombieKilled(this);
	}
}

void AMallZombie::BeginAttack()
{
	State = EZombieState::Attack;
	StateTimer = 0.f;
	bAttackLanded = false;
	CooldownTimer = AttackCooldown;
	if (AttackAnims.Num() > 0)
	{
		UAnimationAsset* A = AttackAnims[FMath::RandRange(0, AttackAnims.Num() - 1)];
		float Len = 1.f;
		if (UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(A))
		{
			Len = Seq->GetPlayLength();
		}
		PlayOnce(A, FMath::Clamp(Len / AttackLength, 0.6f, 2.5f));
	}
	UMallAudio::Get()->PlayAt(GetWorld(), Kind == EZombieKind::Runner ? "shriek" : "snarl", GetActorLocation() + FVector(0, 0, 60), 0.8f, FMath::FRandRange(0.85f, 1.15f), 2500.f);
}

void AMallZombie::Think()
{
	AMallPlayer* P = GetPlayer();
	const FVector Loc = GetActorLocation();
	if (!P || P->IsDead())
	{
		if (State == EZombieState::Chase || State == EZombieState::Attack)
		{
			State = EZombieState::Wander;
			Path.Reset();
		}
	}
	const bool bSee = P && CanSeePlayer(P);
	if (bSee)
	{
		LastSeenTime = Age;
		TargetLoc = P->GetActorLocation();
		if (State == EZombieState::Idle || State == EZombieState::Wander || State == EZombieState::Investigate)
		{
			State = EZombieState::Chase;
			StateTimer = 0.f;
			RepathTimer = 0.f;
			UMallAudio::Get()->PlayAt(GetWorld(), Kind == EZombieKind::Runner ? "shriek" : "snarl", Loc + FVector(0, 0, 60), 0.75f, FMath::FRandRange(0.85f, 1.1f), 3000.f);
		}
	}

	if (State == EZombieState::Chase && P)
	{
		if (Age - LastSeenTime > 7.f)
		{
			State = EZombieState::Investigate;
			StateTimer = 0.f;
			PlanTo(TargetLoc);
			return;
		}
		const FVector To = P->GetActorLocation() - Loc;
		if (bSee && To.Size2D() < 150.f && FMath::Abs(To.Z) < 100.f && CooldownTimer <= 0.f)
		{
			BeginAttack();
			return;
		}
		RepathTimer -= 0.25f;
		if (RepathTimer <= 0.f || Path.Num() == 0)
		{
			PlanTo(bSee ? P->GetActorLocation() : TargetLoc);
			RepathTimer = bSee ? 0.35f : 1.2f;
		}
	}
	else if (State == EZombieState::Investigate)
	{
		if (Path.Num() == 0 && StateTimer > 4.f)
		{
			State = EZombieState::Wander;
			StateTimer = 0.f;
			WanderHome = Loc;
		}
	}
	else if (State == EZombieState::Wander || State == EZombieState::Idle)
	{
		if (Path.Num() == 0 && StateTimer > FMath::FRandRange(3.f, 9.f))
		{
			StateTimer = 0.f;
			if (const FMallNavGraph* Nav = GetNav())
			{
				// Shamble to a random nearby waypoint.
				TArray<int32> Near;
				for (int32 I = 0; I < Nav->Nodes.Num(); ++I)
				{
					if (FVector::DistSquared(Nav->Nodes[I].Pos, WanderHome) < FMath::Square(1400.f) && FMath::Abs(Nav->Nodes[I].Pos.Z - Loc.Z) < 150.f)
					{
						Near.Add(I);
					}
				}
				if (Near.Num() > 0)
				{
					PlanTo(Nav->Nodes[Near[FMath::RandRange(0, Near.Num() - 1)]].Pos + FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-150.f, 150.f), 0));
				}
			}
		}
	}
}

void AMallZombie::FollowPath(float Dt)
{
	const FVector Loc = GetActorLocation();
	if (Path.Num() == 0)
	{
		ProgressTimer = 0.f;
		StuckTime = 0.f;
		LastProgressLoc = Loc;
		return;
	}
	PathIndex = FMath::Clamp(PathIndex, 0, Path.Num() - 1);
	const FVector Next = Path[PathIndex];
	FVector To = Next - Loc;
	const float DZ = FMath::Abs(To.Z);
	To.Z = 0;
	const float D = To.Size();
	const float Reach = PathIndex == Path.Num() - 1 ? 90.f : 70.f;
	if (D < Reach && DZ < 170.f)
	{
		++PathIndex;
		if (PathIndex >= Path.Num())
		{
			Path.Reset();
		}
		return;
	}
	FVector Dir = To / FMath::Max(D, 1.f);

	// Separation so a horde doesn't collapse into one point.
	if (AMallGameMode* GM = GetWorld()->GetAuthGameMode<AMallGameMode>())
	{
		FVector Push = FVector::ZeroVector;
		for (AMallZombie* Other : GM->Zombies)
		{
			if (Other && Other != this && !Other->IsDead())
			{
				FVector Away = Loc - Other->GetActorLocation();
				Away.Z = 0;
				const float AD = Away.Size();
				if (AD < 85.f && AD > 1.f)
				{
					Push += Away / AD * (1.f - AD / 85.f);
				}
			}
		}
		Dir = (Dir + Push * 0.8f).GetSafeNormal2D();
	}
	AddMovementInput(Dir, 1.f);

	// Stuck detection and recovery.
	ProgressTimer += Dt;
	if (ProgressTimer > 1.2f)
	{
		const float Moved = FVector::Dist2D(Loc, LastProgressLoc);
		if (Moved < 30.f)
		{
			StuckTime += ProgressTimer;
			if (StuckTime > 2.4f)
			{
				// Re-plan through the graph and nudge sideways.
				const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector) * (FMath::RandBool() ? 1.f : -1.f);
				AddMovementInput(Side, 1.f);
				PlanTo(TargetLoc);
				if (const FMallNavGraph* Nav = GetNav())
				{
					if (Path.Num() > 0)
					{
						const int32 N = Nav->Nearest(GetWorld(), Loc, false);
						if (Nav->Nodes.IsValidIndex(N))
						{
							Path.Insert(Nav->Nodes[N].Pos, 0);
						}
					}
				}
				StuckTime = 0.f;
			}
		}
		else
		{
			StuckTime = 0.f;
		}
		LastProgressLoc = Loc;
		ProgressTimer = 0.f;
	}
}

void AMallZombie::Tick(float Dt)
{
	Super::Tick(Dt);
	Dt = FMath::Min(Dt, 0.05f);
	Age += Dt;
	if (State == EZombieState::Dead)
	{
		return;
	}
	StateTimer += Dt;
	CooldownTimer = FMath::Max(0.f, CooldownTimer - Dt);
	ThinkTimer -= Dt;
	if (ThinkTimer <= 0.f)
	{
		ThinkTimer = 0.25f;
		Think();
	}

	AMallPlayer* P = GetPlayer();
	switch (State)
	{
	case EZombieState::Attack:
		if (P)
		{
			const FRotator Want = (P->GetActorLocation() - GetActorLocation()).GetSafeNormal2D().Rotation();
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), Want, Dt, 8.f));
			if (!bAttackLanded && StateTimer >= AttackWindup)
			{
				bAttackLanded = true;
				const FVector To = P->GetActorLocation() - GetActorLocation();
				FHitResult Hit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(ZombieMelee), false, this);
				const bool bBlocked = GetWorld()->LineTraceSingleByObjectType(Hit, GetActorLocation() + FVector(0, 0, 50), P->GetEyeLocation() - FVector(0, 0, 30),
					FCollisionObjectQueryParams(ECC_WorldStatic), Params);
				if (To.Size2D() < 190.f && FMath::Abs(To.Z) < 110.f && !bBlocked)
				{
					P->TakeHit(AttackDamage, GetActorLocation());
				}
			}
		}
		if (StateTimer >= AttackLength)
		{
			State = EZombieState::Chase;
			StateTimer = 0.f;
			RepathTimer = 0.f;
			PlayLocomotion();
		}
		break;
	case EZombieState::Stagger:
		if (StateTimer >= (Kind == EZombieKind::Runner ? 0.3f : 0.5f))
		{
			State = EZombieState::Chase;
			StateTimer = 0.f;
			RepathTimer = 0.f;
			PlayLocomotion();
		}
		break;
	default:
		FollowPath(Dt);
		break;
	}

	if (bLocomotionActive)
	{
		if (UAnimSingleNodeInstance* SN = GetMesh()->GetSingleNodeInstance())
		{
			SN->SetBlendSpacePosition(FVector(GetVelocity().Size2D() / FMath::Max(AnimRate, 0.3f), 0, 0));
		}
	}

	VoiceTimer -= Dt;
	if (VoiceTimer <= 0.f)
	{
		VoiceTimer = FMath::FRandRange(4.f, 10.f) * (State == EZombieState::Chase ? 0.6f : 1.f);
		if (P && FVector::DistSquared(P->GetActorLocation(), GetActorLocation()) < FMath::Square(3000.f))
		{
			UMallAudio::Get()->PlayAt(GetWorld(), *FString::Printf(TEXT("groan%d"), FMath::RandRange(0, 3)), GetActorLocation() + FVector(0, 0, 60), 0.55f, FMath::FRandRange(0.8f, 1.15f), 2200.f);
		}
	}
}
