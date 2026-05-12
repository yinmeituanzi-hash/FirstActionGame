#include "Combat/HitReact/HitReactComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Char/ActionCharacterBase.h"
#include "Common/ActionGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogHitReactComp, Log, All);


UHitReactComponent::UHitReactComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHitReactComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UHitReactComponent::RequestHitReact(const FHitContext& HitCtx)
{
	AActionCharacterBase* Owner = GetOwnerCharacter();
	if (Owner == nullptr || Owner->IsDead())
	{
		return false;
	}

	if (IsInCooldown())
	{
		UE_LOG(LogHitReactComp, Verbose, TEXT("HitReactComp[%s]: skipped (cooldown)."), *GetNameSafe(Owner));
		return false;
	}

	// 1. 应用受击替换，例如弱点状态、中毒状态等场景下转换 ReactType。
	const EHitReactType ResolvedType = ResolveReactType(HitCtx.ReactType);

	// 2. 计算 4 方向：被前方、后方、左方或右方打。
	const EHitReactDirection ResolvedDir = ComputeHitDirection(HitCtx);

	// 3. 选择蒙太奇配置行，找不到完整匹配时会走兜底。
	const FHitMontageRow* Row = SelectMontageRow(HitCtx, ResolvedType, ResolvedDir);
	if (Row == nullptr || Row->Montage == nullptr)
	{
		if (bWarnOnMontageNotFound)
		{
			UE_LOG(LogHitReactComp, Warning, TEXT("HitReactComp[%s]: no montage for Type=%d Dir=%d (Override=%s)"),
				*GetNameSafe(Owner),
				static_cast<int32>(ResolvedType),
				static_cast<int32>(ResolvedDir),
				*HitCtx.MontageOverrideRow.ToString());
		}
		return false;
	}

	// 4. 可选：转身朝向攻击者，让受击表现更稳定地面对镜头。
	if (bAllowRotateToAttacker && HitCtx.bRotateToAttacker && HitCtx.Attacker != nullptr)
	{
		RotateOwnerToAttacker(HitCtx.Attacker);
	}

	// 5. 播放受击蒙太奇。
	UAnimInstance* AnimInstance = GetOwnerAnimInstance();
	if (AnimInstance == nullptr)
	{
		return false;
	}

	// 如果有正在播放的旧受击蒙太奇，先停掉。CD 已经过滤了过快重播，这里处理被技能强制打断的边界。
	if (ActiveMontage != nullptr && AnimInstance->Montage_IsPlaying(ActiveMontage))
	{
		AnimInstance->Montage_Stop(0.05f, ActiveMontage);
	}

	const float Duration = AnimInstance->Montage_Play(Row->Montage, Row->PlayRate);
	if (Duration <= 0.0f)
	{
		UE_LOG(LogHitReactComp, Warning, TEXT("HitReactComp[%s]: Montage_Play failed for %s."), *GetNameSafe(Owner), *GetNameSafe(Row->Montage));
		return false;
	}

	// 跳到指定 Section。普通受击保持 NAME_None，即从头播放。
	if (Row->StartSection != NAME_None)
	{
		AnimInstance->Montage_JumpToSection(Row->StartSection, Row->Montage);
	}

	ActiveMontage = Row->Montage;
	LastReactTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0f;

	// 绑定 End 委托。每次重新绑定，避免上一次未触发的委托残留。
	FOnMontageEnded EndedDelegate;
	EndedDelegate.BindUObject(this, &UHitReactComponent::HandleMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndedDelegate, Row->Montage);

	EnterReactState();

	UE_LOG(LogHitReactComp, Log, TEXT("HitReactComp[%s]: play %s (Type=%d Dir=%d Duration=%.2f)"),
		*GetNameSafe(Owner),
		*GetNameSafe(Row->Montage),
		static_cast<int32>(ResolvedType),
		static_cast<int32>(ResolvedDir),
		Duration);

	return true;
}

void UHitReactComponent::StopHitReact(float BlendOutTime)
{
	UAnimInstance* AnimInstance = GetOwnerAnimInstance();
	if (AnimInstance != nullptr && ActiveMontage != nullptr && AnimInstance->Montage_IsPlaying(ActiveMontage))
	{
		AnimInstance->Montage_Stop(BlendOutTime, ActiveMontage);
	}
	ActiveMontage = nullptr;
	ExitReactState();
}

bool UHitReactComponent::IsInCooldown() const
{
	if (HitReactCooldown <= 0.0f || LastReactTime < 0.0f)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}
	return (World->GetTimeSeconds() - LastReactTime) < HitReactCooldown;
}

const FHitMontageRow* UHitReactComponent::SelectMontageRow(const FHitContext& HitCtx, EHitReactType ResolvedType, EHitReactDirection ResolvedDir) const
{
	if (HitMontageTable == nullptr)
	{
		return nullptr;
	}

	// 1. RowName 强制覆盖：用于 Boss 特殊技、剧情触发的固定受击。
	if (HitCtx.MontageOverrideRow != NAME_None)
	{
		const FString Context = TEXT("HitReact MontageOverrideRow");
		if (const FHitMontageRow* OverrideRow = HitMontageTable->FindRow<FHitMontageRow>(HitCtx.MontageOverrideRow, Context))
		{
			return OverrideRow;
		}
		// 找不到强制行也不静默，告知有问题但继续走兜底。
		UE_LOG(LogHitReactComp, Warning, TEXT("HitReact: MontageOverrideRow '%s' not found in table %s."),
			*HitCtx.MontageOverrideRow.ToString(),
			*GetNameSafe(HitMontageTable));
	}

	// 2. 完整匹配：ReactType + Direction。
	const FHitMontageRow* FallbackByType = nullptr;
	TArray<FName> RowNames = HitMontageTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		const FHitMontageRow* Row = HitMontageTable->FindRow<FHitMontageRow>(RowName, TEXT("HitReact Match"));
		if (Row == nullptr || Row->Montage == nullptr)
		{
			continue;
		}

		if (Row->ReactType != ResolvedType)
		{
			continue;
		}

		if (Row->Direction == ResolvedDir)
		{
			return Row;
		}

		// 暂存第一条 ReactType 命中的行，用于兜底。
		if (FallbackByType == nullptr)
		{
			FallbackByType = Row;
		}
	}

	// 3. 兜底：仅 ReactType 匹配，方向任意。
	return FallbackByType;
}

EHitReactDirection UHitReactComponent::ComputeHitDirection(const FHitContext& HitCtx) const
{
	const AActionCharacterBase* Owner = GetOwnerCharacter();
	if (Owner == nullptr)
	{
		return EHitReactDirection::Front;
	}

	// HitDirection 表示攻击施加方向，也就是攻击者到受击者的水平向量。
	// 把它投影到受击者本地坐标系，看主分量落在哪个方向。
	FVector LocalHit = HitCtx.HitDirection.GetSafeNormal2D();
	if (LocalHit.IsNearlyZero())
	{
		// HitDirection 没填时退化为 Front，这是最常见情况，可以避免随机方向。
		return EHitReactDirection::Front;
	}

	const FVector OwnerForward = Owner->GetActorForwardVector().GetSafeNormal2D();
	const FVector OwnerRight = Owner->GetActorRightVector().GetSafeNormal2D();

	const float DotForward = FVector::DotProduct(LocalHit, OwnerForward);
	const float DotRight = FVector::DotProduct(LocalHit, OwnerRight);

	// 注意语义：
	//   DotForward > 0  表示攻击方向与受击者朝向相同，受击者被从后方推，受击方向是 Back。
	//   DotForward < 0  表示攻击方向与受击者朝向相反，受击者被从前方推，受击方向是 Front。
	//   DotRight   > 0  表示攻击方向偏右，受击者被从左方推，受击方向是 Left。
	//   DotRight   < 0  表示攻击方向偏左，受击方向是 Right。
	if (FMath::Abs(DotForward) >= FMath::Abs(DotRight))
	{
		return DotForward < 0.0f ? EHitReactDirection::Front : EHitReactDirection::Back;
	}
	else
	{
		return DotRight > 0.0f ? EHitReactDirection::Left : EHitReactDirection::Right;
	}
}

EHitReactType UHitReactComponent::ResolveReactType(EHitReactType InType) const
{
	if (const EHitReactType* Replaced = HitReactReplaceMap.Find(InType))
	{
		return *Replaced;
	}
	return InType;
}

void UHitReactComponent::RotateOwnerToAttacker(AActor* Attacker)
{
	AActionCharacterBase* Owner = GetOwnerCharacter();
	if (Owner == nullptr || Attacker == nullptr)
	{
		return;
	}

	const FVector ToAttacker = (Attacker->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
	if (ToAttacker.IsNearlyZero())
	{
		return;
	}

	FRotator NewRot = Owner->GetActorRotation();
	NewRot.Yaw = ToAttacker.Rotation().Yaw;
	Owner->SetActorRotation(NewRot);
}

void UHitReactComponent::EnterReactState()
{
	bIsInReact = true;

	AActionCharacterBase* Owner = GetOwnerCharacter();
	if (Owner == nullptr)
	{
		return;
	}

	// Block.HitReact：受击中禁止其他主动 Feature 启动，例如攻击、闪避、跳跃。
	// 010 在这里还会区分 Block.Skill、Block.Move 等更细的 Tag，我们简化为统一的 Block.HitReact。
	Owner->AddActionTagExternal(ActionGameplayTags::Block_HitReact);
	Owner->AddActionTagExternal(ActionGameplayTags::Block_Attack);
	Owner->AddActionTagExternal(ActionGameplayTags::Block_Dodge);
	Owner->AddActionTagExternal(ActionGameplayTags::Block_Move);
}

void UHitReactComponent::ExitReactState()
{
	bIsInReact = false;

	AActionCharacterBase* Owner = GetOwnerCharacter();
	if (Owner == nullptr)
	{
		return;
	}

	Owner->RemoveActionTagExternal(ActionGameplayTags::Block_HitReact);
	Owner->RemoveActionTagExternal(ActionGameplayTags::Block_Attack);
	Owner->RemoveActionTagExternal(ActionGameplayTags::Block_Dodge);
	Owner->RemoveActionTagExternal(ActionGameplayTags::Block_Move);
}

void UHitReactComponent::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// 仅处理由本组件启动的蒙太奇结束，避免与攻击、闪避蒙太奇串扰。
	if (Montage != ActiveMontage)
	{
		return;
	}

	UE_LOG(LogHitReactComp, Verbose, TEXT("HitReactComp[%s]: montage ended (interrupted=%s)."),
		*GetNameSafe(GetOwner()),
		bInterrupted ? TEXT("true") : TEXT("false"));

	ActiveMontage = nullptr;
	ExitReactState();
}

AActionCharacterBase* UHitReactComponent::GetOwnerCharacter() const
{
	return Cast<AActionCharacterBase>(GetOwner());
}

UAnimInstance* UHitReactComponent::GetOwnerAnimInstance() const
{
	const AActionCharacterBase* Owner = GetOwnerCharacter();
	if (Owner == nullptr)
	{
		return nullptr;
	}
	USkeletalMeshComponent* Mesh = Owner->GetMesh();
	return Mesh != nullptr ? Mesh->GetAnimInstance() : nullptr;
}
