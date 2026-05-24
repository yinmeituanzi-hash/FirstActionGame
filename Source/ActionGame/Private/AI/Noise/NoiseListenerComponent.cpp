#include "AI/Noise/NoiseListenerComponent.h"

#include "AI/Alert/AlertComponent.h"
#include "AI/Noise/AINoiseSubsystem.h"
#include "Char/ActionMonsterCharacter.h"
#include "Common/ActionGameplayTags.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"

DEFINE_LOG_CATEGORY_STATIC(LogNoiseListener, Log, All);

UNoiseListenerComponent::UNoiseListenerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNoiseListenerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UAINoiseSubsystem* Subsystem = UAINoiseSubsystem::Get(this))
	{
		Subsystem->RegisterListener(this);
	}
	else
	{
		UE_LOG(LogNoiseListener, Warning, TEXT("NoiseListener[%s]: UAINoiseSubsystem not available; this listener will be deaf."), *GetNameSafe(GetOwner()));
	}
}

void UNoiseListenerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAINoiseSubsystem* Subsystem = UAINoiseSubsystem::Get(this))
	{
		Subsystem->UnregisterListener(this);
	}
	Super::EndPlay(EndPlayReason);
}

void UNoiseListenerComponent::HandleNoiseEvent(const FActionAINoiseEvent& Event)
{
	if (!ShouldRespondTo(Event))
	{
		return;
	}

	// 走默认行为（Owner 是 ActionMonsterCharacter 时才有 AlertState 概念）。
	AActionMonsterCharacter* Monster = Cast<AActionMonsterCharacter>(GetOwner());
	ApplyDefaultResponse(Event, Monster);

	// 启动 Hearing CD。
	if (const UWorld* World = GetWorld())
	{
		LastHearTime = World->GetTimeSeconds();
	}

	OnHearNoise.Broadcast(Event);
}

bool UNoiseListenerComponent::ShouldRespondTo(const FActionAINoiseEvent& Event) const
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !IsValid(Owner))
	{
		return false;
	}

	// 自己产生的声音不响应（避免怪物听到自己脚步 / 挥刀）。
	if (AActor* Instigator = Event.Instigator.Get())
	{
		if (Instigator == Owner)
		{
			return false;
		}
	}

	// CD 期内静默。
	if (GetHearingCooldownRemaining() > 0.0f)
	{
		return false;
	}

	// 距离过滤：取双方半径的较小值（"我能听多远" vs "声音能传多远"）。
	const float EffectiveRange = FMath::Min(Event.Loudness, HearingDistance);
	const float Dist = FVector::Dist(Owner->GetActorLocation(), Event.Location);
	if (Dist > EffectiveRange)
	{
		return false;
	}

	// 怪物专属过滤：死亡 / 被 AI 控制阻塞 / 已 Combat 状态。
	if (const AActionMonsterCharacter* Monster = Cast<AActionMonsterCharacter>(Owner))
	{
		const UAlertComponent* AlertComponent = Monster->GetAlertComponent();
		if (Monster->IsDead())
		{
			return false;
		}
		if (Monster->HasActionTag(ActionGameplayTags::Block_AIControl))
		{
			return false;
		}
		if (!bRespondWhenCombat && AlertComponent != nullptr && AlertComponent->GetAlertState() == EAIAlertState::Combat)
		{
			return false;
		}
	}

	return true;
}

void UNoiseListenerComponent::ApplyDefaultResponse(const FActionAINoiseEvent& Event, AActionMonsterCharacter* Monster)
{
	if (Monster == nullptr)
	{
		// Owner 不是 ActionMonsterCharacter（比如玩家也想做"听到"反应）时只 Broadcast 委托。
		return;
	}

	UAlertComponent* AlertComponent = Monster->GetAlertComponent();
	if (AlertComponent == nullptr)
	{
		return;
	}

	AlertComponent->SetLastNoiseLocation(Event.Location);

	if (bAutoPromoteAlertState)
	{
		// SetAlertState 内部带抖动保护：Combat → Alert 这种降级请求会被它根据冷却规则决定要不要立刻接受。
		AlertComponent->SetAlertState(EAIAlertState::Alert);
	}

	UE_LOG(
		LogNoiseListener,
		Verbose,
		TEXT("NoiseListener[%s]: heard %s noise at %s (dist=%.0f)"),
		*GetNameSafe(Monster),
		*UEnum::GetValueAsString(Event.Category),
		*Event.Location.ToString(),
		FVector::Dist(Monster->GetActorLocation(), Event.Location));
}

float UNoiseListenerComponent::GetHearingCooldownRemaining() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr || LastHearTime < 0.0f)
	{
		return 0.0f;
	}
	const float Elapsed = World->GetTimeSeconds() - LastHearTime;
	return FMath::Max(0.0f, HearingCooldown - Elapsed);
}
