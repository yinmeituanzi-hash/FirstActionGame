#include "Combat/HitReact/HitReceiverComponent.h"
#include "Char/ActionCharacterBase.h"
#include "Combat/Feedback/HitFeedbackComponent.h"
#include "Combat/HitReact/HitPhysicsComponent.h"
#include "Combat/HitReact/HitReactComponent.h"
#include "Common/ActionGameplayTags.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogHitReceiver, Log, All);

UHitReceiverComponent::UHitReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHitReceiverComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UHitReceiverComponent::ReceiveHit(const FHitContext& HitCtx)
{
	if (bSuppressAllHits)
	{
		UE_LOG(LogHitReceiver, Verbose, TEXT("HitReceiver: hit suppressed (bSuppressAllHits=true)."));
		return;
	}

	if (IsOwnerDead())
	{
		UE_LOG(LogHitReceiver, Verbose, TEXT("HitReceiver: hit ignored, owner is dead."));
		return;
	}

	// 调度三层。Feedback 永远播放，React 会受霸体影响，Physics 只受类型和冷却影响。
	DispatchFeedback(HitCtx);
	DispatchReact(HitCtx);
	DispatchPhysics(HitCtx);

	OnHitReceived.Broadcast(HitCtx);
}

void UHitReceiverComponent::DispatchFeedback(const FHitContext& HitCtx)
{
	// Feedback 资源在攻击者侧，例如玩家身上的 HitFeedbackComponent。
	ACharacter* AttackerChar = Cast<ACharacter>(HitCtx.Attacker);
	if (AttackerChar == nullptr)
	{
		return;
	}

	UHitFeedbackComponent* Feedback = AttackerChar->FindComponentByClass<UHitFeedbackComponent>();
	if (Feedback == nullptr)
	{
		return;
	}

	ACharacter* VictimChar = Cast<ACharacter>(GetOwner());
	Feedback->TriggerHitFeedback(VictimChar, HitCtx.HitLocation, AttackerChar, HitCtx.FeedbackScale);
}

void UHitReceiverComponent::DispatchReact(const FHitContext& HitCtx)
{
	// 霸体期间只跳过受击动画，不影响扣血、HitStop、震屏。
	if (IsOwnerInSuperArmor())
	{
		UE_LOG(LogHitReceiver, Verbose, TEXT("HitReceiver: react skipped due to SuperArmor."));
		return;
	}

	if (IsOwnerInRagdoll())
	{
		UE_LOG(LogHitReceiver, Verbose, TEXT("HitReceiver: react skipped due to Ragdoll."));
		return;
	}

	// 起身期间不打断起身动画。否则起身刚开始就被新一次轻击打断，视觉上变成"边起身边受击"的扭曲表现。
	// 例外：HitFly 在 DispatchPhysics 里会主动 CancelGetUp，因为击飞优先级高于起身。
	if (IsOwnerGettingUp())
	{
		UE_LOG(LogHitReceiver, Verbose, TEXT("HitReceiver: react skipped because owner is getting up."));
		return;
	}

	AActionCharacterBase* Char = Cast<AActionCharacterBase>(GetOwner());
	if (Char == nullptr)
	{
		return;
	}

	if (UHitReactComponent* React = Char->GetHitReactComponent())
	{
		React->RequestHitReact(HitCtx);
	}
}

void UHitReceiverComponent::DispatchPhysics(const FHitContext& HitCtx)
{
	if (IsOwnerInSuperArmor())
	{
		return;
	}

	if (IsOwnerInRagdoll())
	{
		return;
	}

	if (HitCtx.ReactType != EHitReactType::HitFly)
	{
		return;
	}

	AActionCharacterBase* Char = Cast<AActionCharacterBase>(GetOwner());
	if (Char == nullptr)
	{
		return;
	}

	UHitPhysicsComponent* Physics = Char->GetHitPhysicsComponent();
	if (Physics == nullptr)
	{
		UE_LOG(LogHitReceiver, Warning, TEXT("HitReceiver: HitFly requested but owner has no HitPhysicsComponent. Owner=%s"), *GetNameSafe(Char));
		return;
	}

	// 起身阶段被击飞：先把起身状态干净退出，再让 StartRagdoll/ApplyHitImpulse 接管。
	// 不直接 StopAllMontages：起身 Montage 会被 Ragdoll 流程自然顶掉（关闭 Mesh AnimInstance evaluation），
	// CancelGetUp 只负责清 Tag、清 Timer、广播 OnGetUpFinished。
	if (Physics->IsGettingUp())
	{
		Physics->CancelGetUp();
	}

	if (!Physics->CanApplyHitFly())
	{
		UE_LOG(LogHitReceiver, Verbose, TEXT("HitReceiver: HitFly skipped by HitPhysicsComponent. Owner=%s"), *GetNameSafe(Char));
		return;
	}

	if (HitCtx.bUseRagdoll)
	{
		const FVector InitialImpulse =
			HitCtx.HitDirection.GetSafeNormal2D() * FMath::Max(HitCtx.HitFlyXYStrength, 0.0f)
			+ FVector::UpVector * FMath::Max(HitCtx.HitFlyZStrength, 0.0f);
		Physics->StartRagdoll(InitialImpulse);
		return;
	}

	Physics->ApplyHitImpulse(HitCtx.HitDirection, HitCtx.HitFlyXYStrength, HitCtx.HitFlyZStrength);
}

bool UHitReceiverComponent::IsOwnerInSuperArmor() const
{
	const AActionCharacterBase* Char = Cast<AActionCharacterBase>(GetOwner());
	if (Char == nullptr)
	{
		return false;
	}
	return Char->HasActionTag(ActionGameplayTags::State_SuperArmor);
}

bool UHitReceiverComponent::IsOwnerInRagdoll() const
{
	const AActionCharacterBase* Char = Cast<AActionCharacterBase>(GetOwner());
	if (Char == nullptr)
	{
		return false;
	}
	return Char->HasActionTag(ActionGameplayTags::State_Ragdoll);
}

bool UHitReceiverComponent::IsOwnerGettingUp() const
{
	const AActionCharacterBase* Char = Cast<AActionCharacterBase>(GetOwner());
	if (Char == nullptr)
	{
		return false;
	}
	const UHitPhysicsComponent* Physics = Char->GetHitPhysicsComponent();
	return Physics != nullptr && Physics->IsGettingUp();
}

bool UHitReceiverComponent::IsOwnerDead() const
{
	const AActionCharacterBase* Char = Cast<AActionCharacterBase>(GetOwner());
	return Char != nullptr && Char->IsDead();
}
