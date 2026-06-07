#include "Combat/Skills/ActionSkillObject.h"

#include "Char/ActionCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"

// Day1 的 SkillObject 故意保持很小：先为冷却、激活、取消状态建立稳定运行时归属，
// Montage 节点和效果执行后续再叠加进来。
void UActionSkillObject::InitFromData(AActionCharacterBase* InOwner, FName InSkillId, const FActionSkillRow& InSkillData)
{
	OwnerCharacter = InOwner;
	SkillId = InSkillId;
	SkillData = InSkillData;
	if (SkillData.SkillId.IsNone())
	{
		SkillData.SkillId = InSkillId;
	}
	CooldownRemaining = 0.0f;
	bActive = false;
	bInitialized = InOwner != nullptr && !SkillId.IsNone();
}

bool UActionSkillObject::CanActivate() const
{
	if (!bInitialized || bActive || CooldownRemaining > 0.0f || !OwnerCharacter.IsValid())
	{
		return false;
	}

	const AActionCharacterBase* Owner = OwnerCharacter.Get();
	if (Owner == nullptr || Owner->IsDead())
	{
		return false;
	}

	if (!SkillData.bAllowInAir)
	{
		const UCharacterMovementComponent* Movement = Owner->GetCharacterMovement();
		if (Movement != nullptr && Movement->IsFalling())
		{
			return false;
		}
	}

	return true;
}

void UActionSkillObject::Activate(AActor* InTarget)
{
	if (!CanActivate())
	{
		return;
	}

	bActive = true;
	CurrentTarget = InTarget;
}

void UActionSkillObject::Deactivate(EActionSkillStopReason Reason)
{
	if (!bActive)
	{
		return;
	}

	bActive = false;
	CurrentTarget.Reset();
	LastStopReason = Reason;

	// 有些技能被受击打断时不应该进入冷却。这个规则放在配置行里，
	// 方便之后按技能单独调整。
	if (Reason != EActionSkillStopReason::HitInterrupt || SkillData.bStartCooldownOnHitInterrupt)
	{
		CooldownRemaining = FMath::Max(0.0f, SkillData.Cooldown);
	}
}

void UActionSkillObject::TickCooldown(float DeltaTime)
{
	if (CooldownRemaining > 0.0f)
	{
		CooldownRemaining = FMath::Max(0.0f, CooldownRemaining - FMath::Max(0.0f, DeltaTime));
	}
}

bool UActionSkillObject::CanBeCancelledBy(EActionSkillCancelFlag IncomingType) const
{
	const int32 IncomingMask = static_cast<int32>(IncomingType);
	return IncomingMask != 0 && (SkillData.AllowCancelBy & IncomingMask) != 0;
}
