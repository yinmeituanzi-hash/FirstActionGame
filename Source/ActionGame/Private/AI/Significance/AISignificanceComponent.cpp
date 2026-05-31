#include "AI/Significance/AISignificanceComponent.h"

#include "AI/ActionMonsterAIController.h"
#include "AI/ActionAITypes.h"
#include "AI/Alert/AlertComponent.h"
#include "AI/Significance/AISignificanceSubsystem.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Char/ActionMonsterCharacter.h"
#include "Common/ActionGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

static TAutoConsoleVariable<int32> CVarAISignificanceDebug(
	TEXT("AI.SignificanceDebug"),
	0,
	TEXT("Draw AI significance level above monster heads. 0=off, 1=on."),
	ECVF_Cheat);

DEFINE_LOG_CATEGORY_STATIC(LogAISignificance, Log, All);

namespace
{
FString SignificanceLevelToString(EAISignificanceLevel Level)
{
	switch (Level)
	{
	case EAISignificanceLevel::High:
		return TEXT("High");
	case EAISignificanceLevel::Medium:
		return TEXT("Medium");
	case EAISignificanceLevel::Low:
		return TEXT("Low");
	case EAISignificanceLevel::Dormant:
		return TEXT("Dormant");
	default:
		return TEXT("Unknown");
	}
}

FColor SignificanceLevelToColor(EAISignificanceLevel Level)
{
	switch (Level)
	{
	case EAISignificanceLevel::High:
		return FColor::Green;
	case EAISignificanceLevel::Medium:
		return FColor::Yellow;
	case EAISignificanceLevel::Low:
		return FColor::Orange;
	case EAISignificanceLevel::Dormant:
		return FColor::Silver;
	default:
		return FColor::White;
	}
}

bool IsLevelLowerThan(EAISignificanceLevel A, EAISignificanceLevel B)
{
	return static_cast<uint8>(A) > static_cast<uint8>(B);
}
}

UAISignificanceComponent::UAISignificanceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	HighConfig.BehaviorTreeTickInterval = 0.0f;
	HighConfig.ControllerTickInterval = 0.0f;
	HighConfig.MovementTickInterval = 0.0f;
	HighConfig.MeshTickInterval = 0.0f;
	HighConfig.bEnableAnimUpdateRateOptimizations = false;
	HighConfig.VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	MediumConfig.BehaviorTreeTickInterval = 0.1f;
	MediumConfig.ControllerTickInterval = 0.0f;
	MediumConfig.MovementTickInterval = 0.0f;
	MediumConfig.MeshTickInterval = 0.0f;
	MediumConfig.bEnableAnimUpdateRateOptimizations = true;
	MediumConfig.VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	LowConfig.BehaviorTreeTickInterval = 0.35f;
	LowConfig.ControllerTickInterval = 0.1f;
	LowConfig.MovementTickInterval = 0.1f;
	LowConfig.MeshTickInterval = 0.1f;
	LowConfig.bEnableAnimUpdateRateOptimizations = true;
	LowConfig.VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;

	DormantConfig.BehaviorTreeTickInterval = 0.5f;
	DormantConfig.ControllerTickInterval = 0.5f;
	DormantConfig.MovementTickInterval = 0.5f;
	DormantConfig.MeshTickInterval = 0.5f;
	DormantConfig.bEnableBehaviorTreeTick = false;
	DormantConfig.bEnableMovementTick = false;
	DormantConfig.bEnableMeshTick = false;
	DormantConfig.bEnableControllerTick = false;
	DormantConfig.bEnableAnimUpdateRateOptimizations = true;
	DormantConfig.VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
}

void UAISignificanceComponent::BeginPlay()
{
	Super::BeginPlay();

	CaptureDefaultsIfNeeded();

	if (UAISignificanceSubsystem* Subsystem = UAISignificanceSubsystem::Get(this))
	{
		Subsystem->RegisterComponent(this);
	}
}

void UAISignificanceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAISignificanceSubsystem* Subsystem = UAISignificanceSubsystem::Get(this))
	{
		Subsystem->UnregisterComponent(this);
	}

	Super::EndPlay(EndPlayReason);
}

void UAISignificanceComponent::ApplyBudgetEnabled(bool bEnabled)
{
	bBudgetBTDisabled = !bEnabled;
	ApplyLevel(CurrentLevel);
}

void UAISignificanceComponent::EvaluateAndApply(APawn* PlayerPawn)
{
	AActionMonsterCharacter* Monster = GetOwnerMonster();
	if (Monster == nullptr || Monster->IsDead())
	{
		return;
	}

	CaptureDefaultsIfNeeded();
	if (!bDefaultsCaptured)
	{
		return;
	}

	const EAISignificanceLevel NewLevel = CalculateLevel(PlayerPawn);
	ApplyLevel(NewLevel);

	if (ShouldDebugDraw())
	{
		DrawDebugInfo();
	}
}

AActionMonsterCharacter* UAISignificanceComponent::GetOwnerMonster() const
{
	return Cast<AActionMonsterCharacter>(GetOwner());
}

AActionMonsterAIController* UAISignificanceComponent::GetMonsterAIController() const
{
	const AActionMonsterCharacter* Monster = GetOwnerMonster();
	return Monster != nullptr ? Cast<AActionMonsterAIController>(Monster->GetController()) : nullptr;
}

UBehaviorTreeComponent* UAISignificanceComponent::GetBehaviorTreeComponent() const
{
	const AActionMonsterAIController* AIC = GetMonsterAIController();
	return AIC != nullptr ? AIC->BehaviorComp.Get() : nullptr;
}

void UAISignificanceComponent::CaptureDefaultsIfNeeded()
{
	if (bDefaultsCaptured)
	{
		return;
	}

	AActionMonsterCharacter* Monster = GetOwnerMonster();
	AActionMonsterAIController* AIC = GetMonsterAIController();
	UBehaviorTreeComponent* BTComp = GetBehaviorTreeComponent();
	UCharacterMovementComponent* Movement = Monster != nullptr ? Monster->GetCharacterMovement() : nullptr;
	USkeletalMeshComponent* Mesh = Monster != nullptr ? Monster->GetMesh() : nullptr;

	if (Monster == nullptr || AIC == nullptr || BTComp == nullptr || Movement == nullptr || Mesh == nullptr)
	{
		return;
	}

	bDefaultControllerTickEnabled = AIC->PrimaryActorTick.IsTickFunctionEnabled();
	bDefaultBehaviorTreeTickEnabled = BTComp->PrimaryComponentTick.IsTickFunctionEnabled();
	bDefaultMovementTickEnabled = Movement->PrimaryComponentTick.IsTickFunctionEnabled();
	bDefaultMeshTickEnabled = Mesh->PrimaryComponentTick.IsTickFunctionEnabled();

	DefaultControllerTickInterval = AIC->PrimaryActorTick.TickInterval;
	DefaultBehaviorTreeTickInterval = BTComp->PrimaryComponentTick.TickInterval;
	DefaultMovementTickInterval = Movement->PrimaryComponentTick.TickInterval;
	DefaultMeshTickInterval = Mesh->PrimaryComponentTick.TickInterval;
	bDefaultEnableAnimURO = Mesh->bEnableUpdateRateOptimizations;
	DefaultVisibilityBasedAnimTickOption = Mesh->VisibilityBasedAnimTickOption;

	bDefaultsCaptured = true;
}

EAISignificanceLevel UAISignificanceComponent::CalculateLevel(APawn* PlayerPawn)
{
	AActionMonsterCharacter* Monster = GetOwnerMonster();
	if (Monster == nullptr || PlayerPawn == nullptr)
	{
		DistanceToPlayer = TNumericLimits<float>::Max();
		bHasLineOfSightToPlayer = false;
		bWasRecentlyRendered = false;
		DistanceScore = 0.0f;
		RenderScore = 0.0f;
		CurrentScore = 0.0f;
		return EnforceAlertStateFloor(EAISignificanceLevel::Dormant);
	}

	CurrentScore = CalculateSignificanceScore(Monster, PlayerPawn);
	EAISignificanceLevel Level = ScoreToLevel(CurrentScore);

	if (Monster->IsAttacking() || IsAIControlBlocked())
	{
		Level = EAISignificanceLevel::High;
	}

	return EnforceAlertStateFloor(Level);
}

float UAISignificanceComponent::CalculateSignificanceScore(AActionMonsterCharacter* Monster, APawn* PlayerPawn)
{
	check(Monster != nullptr);
	check(PlayerPawn != nullptr);

	DistanceToPlayer = FVector::Dist(Monster->GetActorLocation(), PlayerPawn->GetActorLocation());

	float RealDistanceScore = 0.0f;
	DistanceScore = CalculateDistanceScore(DistanceToPlayer, RealDistanceScore);
	RenderScore = CalculateRenderScore(Monster, RealDistanceScore);

	bHasLineOfSightToPlayer = false;
	if (const AActionMonsterAIController* AIC = GetMonsterAIController())
	{
		bHasLineOfSightToPlayer = AIC->LineOfSightTo(PlayerPawn);
	}

	// 参考 010：距离分和渲染分都可能限制重要度，取较小值避免单一因素把远处 / 不可见单位过度抬高。
	return bUseRenderScore ? FMath::Min(DistanceScore, RenderScore) : DistanceScore;
}

float UAISignificanceComponent::CalculateDistanceScore(float InDistance, float& OutRealDistanceScore) const
{
	const float ClampedDistance = FMath::Min(MaxDistanceScoreDistance, FMath::Max(InDistance, 0.0f));
	OutRealDistanceScore = FMath::Max(0.0f, (1.0f - ClampedDistance / MaxDistanceScoreDistance) * 10.0f);

	if (InDistance <= HighDistance)
	{
		return HighBaseScore + OutRealDistanceScore;
	}

	if (InDistance <= MediumDistance)
	{
		return MediumBaseScore + OutRealDistanceScore;
	}

	if (InDistance <= LowDistance)
	{
		return LowBaseScore + OutRealDistanceScore;
	}

	return 0.0f;
}

float UAISignificanceComponent::CalculateRenderScore(const AActionMonsterCharacter* Monster, float RealDistanceScore)
{
	bWasRecentlyRendered = false;

	const UWorld* World = GetWorld();
	const bool bDedicatedServer = World != nullptr && World->GetNetMode() == NM_DedicatedServer;
	if (bDedicatedServer)
	{
		return RenderedBaseScore + RealDistanceScore;
	}

	const USkeletalMeshComponent* Mesh = Monster != nullptr ? Monster->GetMesh() : nullptr;
	bWasRecentlyRendered = Mesh != nullptr && Mesh->WasRecentlyRendered(RenderRecentlyTolerance);

	return (bWasRecentlyRendered ? RenderedBaseScore : NotRenderedBaseScore) + RealDistanceScore;
}

EAISignificanceLevel UAISignificanceComponent::ScoreToLevel(float Score) const
{
	if (Score >= HighScoreThreshold)
	{
		return EAISignificanceLevel::High;
	}

	if (Score >= MediumScoreThreshold)
	{
		return EAISignificanceLevel::Medium;
	}

	if (Score > LowScoreThreshold)
	{
		return EAISignificanceLevel::Low;
	}

	return EAISignificanceLevel::Dormant;
}

EAISignificanceLevel UAISignificanceComponent::EnforceAlertStateFloor(EAISignificanceLevel InLevel) const
{
	const AActionMonsterCharacter* Monster = GetOwnerMonster();
	const UAlertComponent* Alert = Monster != nullptr ? Monster->GetAlertComponent() : nullptr;
	if (Alert == nullptr)
	{
		return InLevel;
	}

	const EAIAlertState AlertState = Alert->GetAlertState();
	if (AlertState == EAIAlertState::Combat && IsLevelLowerThan(InLevel, EAISignificanceLevel::Medium))
	{
		return EAISignificanceLevel::Medium;
	}

	if (AlertState == EAIAlertState::Alert && IsLevelLowerThan(InLevel, EAISignificanceLevel::Low))
	{
		return EAISignificanceLevel::Low;
	}

	return InLevel;
}

void UAISignificanceComponent::ApplyLevel(EAISignificanceLevel NewLevel)
{
	const EAISignificanceLevel OldLevel = CurrentLevel;
	CurrentLevel = NewLevel;

	ApplyChannels(GetConfigForLevel(CurrentLevel));

	if (OldLevel != CurrentLevel)
	{
		UE_LOG(
			LogAISignificance,
			Log,
			TEXT("AISignificance: %s %s -> %s, score=%.1f distance=%.0f distScore=%.1f renderScore=%.1f rendered=%d los=%d"),
			*GetNameSafe(GetOwner()),
			*SignificanceLevelToString(OldLevel),
			*SignificanceLevelToString(CurrentLevel),
			CurrentScore,
			DistanceToPlayer,
			DistanceScore,
			RenderScore,
			bWasRecentlyRendered ? 1 : 0,
			bHasLineOfSightToPlayer ? 1 : 0);

		OnLevelChanged.Broadcast(OldLevel, CurrentLevel);
	}
}

void UAISignificanceComponent::ApplyChannels(const FAISignificanceLevelConfig& Config)
{
	AActionMonsterCharacter* Monster = GetOwnerMonster();
	if (Monster == nullptr || Monster->IsDead())
	{
		return;
	}

	ApplyBehaviorTreeTick(Config);

	if (AActionMonsterAIController* AIC = GetMonsterAIController())
	{
		AIC->SetActorTickEnabled(Config.bEnableControllerTick && bDefaultControllerTickEnabled);
		AIC->SetActorTickInterval(Config.bEnableControllerTick ? Config.ControllerTickInterval : DefaultControllerTickInterval);
	}

	if (UCharacterMovementComponent* Movement = Monster->GetCharacterMovement())
	{
		Movement->SetComponentTickEnabled(Config.bEnableMovementTick && bDefaultMovementTickEnabled);
		Movement->SetComponentTickInterval(Config.bEnableMovementTick ? Config.MovementTickInterval : DefaultMovementTickInterval);
	}

	if (USkeletalMeshComponent* Mesh = Monster->GetMesh())
	{
		Mesh->SetComponentTickEnabled(Config.bEnableMeshTick && bDefaultMeshTickEnabled);
		Mesh->SetComponentTickInterval(Config.bEnableMeshTick ? Config.MeshTickInterval : DefaultMeshTickInterval);
		Mesh->bEnableUpdateRateOptimizations = Config.bEnableMeshTick
			? Config.bEnableAnimUpdateRateOptimizations
			: bDefaultEnableAnimURO;
		Mesh->VisibilityBasedAnimTickOption = Config.bEnableMeshTick
			? Config.VisibilityBasedAnimTickOption
			: DefaultVisibilityBasedAnimTickOption;
	}
}

void UAISignificanceComponent::ApplyBehaviorTreeTick(const FAISignificanceLevelConfig& Config)
{
	UBehaviorTreeComponent* BTComp = GetBehaviorTreeComponent();
	if (BTComp == nullptr)
	{
		return;
	}

	bSignificanceBTDisabled = !Config.bEnableBehaviorTreeTick;
	const bool bFinalEnabled = Config.bEnableBehaviorTreeTick
		&& !bBudgetBTDisabled
		&& bDefaultBehaviorTreeTickEnabled;

	BTComp->SetComponentTickEnabled(bFinalEnabled);
	BTComp->SetComponentTickInterval(bFinalEnabled ? Config.BehaviorTreeTickInterval : DefaultBehaviorTreeTickInterval);
}

const FAISignificanceLevelConfig& UAISignificanceComponent::GetConfigForLevel(EAISignificanceLevel Level) const
{
	switch (Level)
	{
	case EAISignificanceLevel::High:
		return HighConfig;
	case EAISignificanceLevel::Medium:
		return MediumConfig;
	case EAISignificanceLevel::Low:
		return LowConfig;
	case EAISignificanceLevel::Dormant:
		return DormantConfig;
	default:
		return HighConfig;
	}
}

bool UAISignificanceComponent::IsAIControlBlocked() const
{
	const AActionMonsterCharacter* Monster = GetOwnerMonster();
	return Monster != nullptr && Monster->HasActionTag(ActionGameplayTags::Block_AIControl);
}

bool UAISignificanceComponent::ShouldDebugDraw() const
{
	return CVarAISignificanceDebug.GetValueOnGameThread() != 0;
}

void UAISignificanceComponent::DrawDebugInfo() const
{
	const AActionMonsterCharacter* Monster = GetOwnerMonster();
	const UWorld* World = GetWorld();
	if (Monster == nullptr || World == nullptr)
	{
		return;
	}

	const UAlertComponent* Alert = Monster->GetAlertComponent();
	const uint8 AlertValue = Alert != nullptr ? static_cast<uint8>(Alert->GetAlertState()) : 255;
	const FString DebugText = FString::Printf(
		TEXT("%s | %.1f | D%.0f | DS%.1f RS%.1f | R%d LOS%d | A%d | BT=%s"),
		*SignificanceLevelToString(CurrentLevel),
		CurrentScore,
		DistanceToPlayer,
		DistanceScore,
		RenderScore,
		bWasRecentlyRendered ? 1 : 0,
		bHasLineOfSightToPlayer ? 1 : 0,
		AlertValue,
		IsBehaviorTreeAllowedBySignificance() && IsBehaviorTreeAllowedByBudget() ? TEXT("On") : TEXT("Off"));

	DrawDebugString(
		World,
		Monster->GetActorLocation() + FVector(0.0f, 0.0f, 160.0f),
		DebugText,
		nullptr,
		SignificanceLevelToColor(CurrentLevel),
		0.25f,
		true);
}
