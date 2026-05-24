#include "Char/ActionMonsterCharacter.h"
#include "AI/ActionMonsterAIController.h"
#include "AI/Alert/AlertComponent.h"
#include "AI/Noise/NoiseListenerComponent.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Combat/ActionCombatLibrary.h"
#include "Combat/ActionCombatNotifies.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Logging/LogMacros.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionMonsterCharacter, Log, All);

AActionMonsterCharacter::AActionMonsterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 目前怪物先沿用基类移动配置。
	// 等后面接入 AI 后，再按怪物类型细分移动速度、转向和攻击距离。

	// Sprint 4-A：默认使用我们的怪物 AIController。
	// PlacedInWorldOrSpawned 表示无论是放在关卡里还是 Spawn 出来的怪都自动 Possess。
	AIControllerClass = AActionMonsterAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	CombatTeam = EActionCombatTeam::Monster;

	AlertComponent = CreateDefaultSubobject<UAlertComponent>(TEXT("AlertComponent"));

	// Sprint 4-B++ Day 5：挂上"耳朵"。BeginPlay 时会自动注册到 UAINoiseSubsystem。
	// 任意继承 AActionMonsterCharacter 的怪默认就具备听觉，不需要每个 BP 手动加。
	NoiseListener = CreateDefaultSubobject<UNoiseListenerComponent>(TEXT("NoiseListener"));
}

void AActionMonsterCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 绑 OnPlayMontageNotifyBegin 一次。等 AnimInstance 真的存在时才绑。
	// 玩家走 UMontageActionFeature 同款机制，怪物没有 Feature 系统所以 Character 直接绑。
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			if (!bMontageNotifyBound)
			{
				AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &AActionMonsterCharacter::OnAnyMontageNotifyBegin);
				bMontageNotifyBound = true;
			}
		}
		else
		{
			UE_LOG(LogActionMonsterCharacter, Warning, TEXT("ActionMonsterCharacter::BeginPlay: AnimInstance not ready, AttackHitCheck Notify won't be routed."));
		}
	}
}

void AActionMonsterCharacter::ApplyDamage(float InDamage)
{
	if (IsDead())
	{
		UE_LOG(LogActionMonsterCharacter, Log, TEXT("ActionMonsterCharacter: Ignore damage because monster is already dead."));
		return;
	}

	const float HPBefore = GetCurrentHP();
	Super::ApplyDamage(InDamage);

	UE_LOG(
		LogActionMonsterCharacter,
		Log,
		TEXT("ActionMonsterCharacter: Took %.1f damage. HP %.1f -> %.1f"),
		InDamage,
		HPBefore,
		GetCurrentHP());

	// 受击动画现在统一由 HitReceiverComponent / HitReactComponent 根据 DataTable 调度。
	// 这里不再播放旧的 HitReactMontage，避免覆盖或掩盖表驱动受击动画。
}

void AActionMonsterCharacter::Die()
{
	if (IsDead())
	{
		return;
	}

	Super::Die();

	UE_LOG(LogActionMonsterCharacter, Log, TEXT("ActionMonsterCharacter: Monster died."));

	// Day 6: 死亡是不可逆状态，BT 走 StopLogic 永久停，不走 Block.AIControl Tag 路径。
	// 区别：受击/Ragdoll 用 Tag 是因为起身后要恢复 BT；死亡不需要"恢复"。
	if (AController* MonsterController = GetController())
	{
		if (AActionMonsterAIController* AIC = Cast<AActionMonsterAIController>(MonsterController))
		{
			if (UBrainComponent* Brain = AIC->BrainComponent)
			{
				Brain->StopLogic(TEXT("Dead"));
			}
		}
	}

	// Day 6: 死亡时清空仇恨列表，避免怪物死后还把"上一次攻击者"留在身上。
	// 等下一步 HatredMap 加上后 ClearHatred 会真正清空 Map；此刻是空操作占位。
	ClearHatred();

	float FinalLifeSpan = DeathLifeSpan;

	// 当前先给一个最小死亡表现：
	// 如果配置了死亡蒙太奇，就先播放；否则仍然按兜底寿命自动销毁。
	if (DeathMontage != nullptr && GetMesh() != nullptr && GetMesh()->GetAnimInstance() != nullptr)
	{
		const float PlayedLength = PlayAnimMontage(DeathMontage);
		if (PlayedLength > 0.0f)
		{
			FinalLifeSpan = FMath::Max(FinalLifeSpan, PlayedLength);
			UE_LOG(LogActionMonsterCharacter, Log, TEXT("ActionMonsterCharacter: Played death montage."));

			// 死亡动画真正播完前，Montage 往往已经开始混出到默认状态机了。
			// 所以这里不是卡“最后一瞬间”，而是提前一点冻结，避免先混回 idle。
			if (UWorld* World = GetWorld())
			{
				const float BlendOutTime = DeathMontage->BlendOut.GetBlendTime();
				const float FreezeLeadTime = FMath::Max(DeathPoseFreezeLeadTime, BlendOutTime);
				const float FreezeDelay = FMath::Max(PlayedLength - FreezeLeadTime, 0.0f);
				World->GetTimerManager().SetTimer(
					FreezeDeathPoseTimerHandle,
					this,
					&AActionMonsterCharacter::FreezeDeathPose,
					FreezeDelay,
					false);
			}
		}
	}

	if (FinalLifeSpan > 0.0f)
	{
		SetLifeSpan(FinalLifeSpan);
	}
}

void AActionMonsterCharacter::StartMonsterAttack()
{
	if (!CanAttack())
	{
		UE_LOG(LogActionMonsterCharacter, Warning, TEXT("ActionMonsterCharacter: StartMonsterAttack blocked because character cannot attack."));
		return;
	}

	// 攻击中或冷却中：直接拒绝。BTTask 会按 IsAttacking + Cooldown 判断后再调，正常情况
	// 走不到这里；保留这层兜底是为了"被外部脚本误调"也不会瞬连乱挥。
	if (IsAttacking())
	{
		UE_LOG(LogActionMonsterCharacter, Verbose, TEXT("ActionMonsterCharacter: StartMonsterAttack ignored — attack already in flight."));
		return;
	}
	if (GetAttackCooldownRemaining() > 0.0f)
	{
		UE_LOG(
			LogActionMonsterCharacter,
			Verbose,
			TEXT("ActionMonsterCharacter: StartMonsterAttack ignored — cooldown remaining %.2fs."),
			GetAttackCooldownRemaining());
		return;
	}

	// 必须有 Montage 才能开打。Montage 是动作游戏怪物攻击的基础需求。
	// 没配的话 AI 会反复尝试调用并失败，BT 会一直退回 MoveToTarget，玩家
	// 会看到怪贴脸不动；这样的明显错误比"无声兜底"更容易暴露配置问题。
	if (MonsterAttackMontage == nullptr)
	{
		UE_LOG(LogActionMonsterCharacter, Warning, TEXT("ActionMonsterCharacter: StartMonsterAttack failed — MonsterAttackMontage is not configured."));
		return;
	}
	if (GetMesh() == nullptr || GetMesh()->GetAnimInstance() == nullptr)
	{
		UE_LOG(LogActionMonsterCharacter, Warning, TEXT("ActionMonsterCharacter: StartMonsterAttack failed — Mesh / AnimInstance not ready."));
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const float PlayedLength = PlayAnimMontage(MonsterAttackMontage);
	if (PlayedLength <= 0.0f)
	{
		UE_LOG(LogActionMonsterCharacter, Warning, TEXT("ActionMonsterCharacter: PlayAnimMontage returned 0, attack aborted."));
		return;
	}

	bAttackInFlight = true;
	LastAttackStartTime = World->GetTimeSeconds();
	HitActorsThisAttack.Reset();  // 新一段攻击：清掉上一段的命中记录
	SetActionState(EActionCharacterState::Attacking);

	// Montage 整体播完后清状态。注意 BlendOut 阶段也算在内，所以收尾会比 PlayedLength 略晚。
	if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
	{
		FOnMontageEnded EndedDelegate;
		EndedDelegate.BindUObject(this, &AActionMonsterCharacter::OnAttackMontageEnded);
		AnimInst->Montage_SetEndDelegate(EndedDelegate, MonsterAttackMontage);
	}

	UE_LOG(
		LogActionMonsterCharacter,
		Log,
		TEXT("ActionMonsterCharacter: Started attack montage. Length=%.2fs (waiting for AttackHitCheck Notify)"),
		PlayedLength);
}

void AActionMonsterCharacter::OnAnyMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& /*Payload*/)
{
	// 只处理"我们正在播攻击 Montage 期间"的 AttackHitCheck Notify。
	// 其他 Montage（受击 / 死亡 / 起身）即使带同名 Notify 也不会触发命中。
	if (!bAttackInFlight || NotifyName != ActionCombatNotifies::AttackHitCheck)
	{
		return;
	}
	HandleAttackHitCheckNotify();
}

void AActionMonsterCharacter::HandleAttackHitCheckNotify()
{
	if (IsDead())
	{
		return;
	}

	// HitContext 模板：填入所有"攻击者一侧决定"的字段。
	// Library 会复制此模板用于每个被命中目标，并自动填 Attacker / HitLocation / HitDirection。
	FHitContext Template;
	Template.ReactType = MonsterAttackReactType;
	Template.DamageAmount = GetAttackPower();
	Template.FeedbackScale = MonsterAttackFeedbackScale;
	Template.bRotateToAttacker = bMonsterAttackRotateVictimToAttacker;
	Template.HitFlyXYStrength = MonsterAttackHitFlyXYStrength;
	Template.HitFlyZStrength = MonsterAttackHitFlyZStrength;
	Template.bUseRagdoll = bMonsterAttackUseRagdoll;

	FSphereAttackHitParams Params;
	Params.Attacker = this;
	Params.Center = GetActorLocation() + GetActorForwardVector() * HitCheckForwardOffset;
	Params.Radius = HitCheckRadius;
	// TargetClassFilter 用 AActionCharacterBase 而不是只玩家：未来加同伴 / 中立怪都自动覆盖。
	Params.TargetClassFilter = AActionCharacterBase::StaticClass();
	Params.HitContextTemplate = Template;
	Params.HitLocationBackstep = MonsterAttackHitLocationBackstep;
	Params.bDrawDebugSphere = bDrawDebugHitSphere;
	Params.DebugDrawDuration = 1.0f;

	const int32 HitCount = UActionCombatLibrary::PerformSphereAttackHit(this, Params, HitActorsThisAttack);
	UE_LOG(
		LogActionMonsterCharacter,
		Verbose,
		TEXT("ActionMonsterCharacter: AttackHitCheck Notify hit %d target(s)."),
		HitCount);
}

void AActionMonsterCharacter::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != MonsterAttackMontage)
	{
		return;
	}
	UE_LOG(
		LogActionMonsterCharacter,
		Verbose,
		TEXT("ActionMonsterCharacter: Attack montage ended. Interrupted=%s"),
		bInterrupted ? TEXT("true") : TEXT("false"));
	FinishMonsterAttack();
}

void AActionMonsterCharacter::FinishMonsterAttack()
{
	bAttackInFlight = false;
	HitActorsThisAttack.Reset();

	// 攻击结束后回 Idle，前提是没有切到其他状态（受击/死亡）。
	if (CurrentActionState == EActionCharacterState::Attacking)
	{
		SetActionState(EActionCharacterState::Idle);
	}
}

// ---------- AI 接口 ----------

bool AActionMonsterCharacter::IsAttacking() const
{
	return bAttackInFlight;
}

float AActionMonsterCharacter::GetAttackCooldownRemaining() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr || LastAttackStartTime < 0.0f)
	{
		return 0.0f;
	}
	const float Elapsed = World->GetTimeSeconds() - LastAttackStartTime;
	return FMath::Max(0.0f, AttackCooldown - Elapsed);
}

float AActionMonsterCharacter::GetDistance2DTo(const AActor* Other) const
{
	if (Other == nullptr)
	{
		return TNumericLimits<float>::Max();
	}
	return FVector::Dist2D(GetActorLocation(), Other->GetActorLocation());
}

bool AActionMonsterCharacter::IsTargetInAttackRange(const AActor* Target) const
{
	if (Target == nullptr)
	{
		return false;
	}
	return GetDistance2DTo(Target) <= MonsterAttackRange;
}

// ============================================================================
// IActionLockableInterface
// ============================================================================

bool AActionMonsterCharacter::CanBeLockedOn_Implementation() const
{
	return !IsDead();
}

FVector AActionMonsterCharacter::GetLockOnTargetLocation_Implementation() const
{
	if (LockOnSocketName != NAME_None && GetMesh() != nullptr && GetMesh()->DoesSocketExist(LockOnSocketName))
	{
		return GetMesh()->GetSocketLocation(LockOnSocketName);
	}
	// 默认锁角色根位置；建议在 BP 里配 LockOnSocketName 指到胸口/头部 socket，体感更好。
	return GetActorLocation();
}

void AActionMonsterCharacter::OnLockedOn_Implementation()
{
	bIsBeingLockedOn = true;
	UE_LOG(LogActionMonsterCharacter, Log, TEXT("ActionMonsterCharacter: Locked on by player."));
}

void AActionMonsterCharacter::OnLockedOff_Implementation()
{
	bIsBeingLockedOn = false;
	UE_LOG(LogActionMonsterCharacter, Log, TEXT("ActionMonsterCharacter: Locked off."));
}

// ---------- Sprint 4-C+ Day 6: 简化仇恨列表 ----------

void AActionMonsterCharacter::AddHatred(AActor* Source, float Value)
{
	if (Source == nullptr || Source == this || Value <= 0.0f)
	{
		return;
	}

	// 死亡的攻击者也别累。比如玩家死后 DOT 还在跳血，怪没必要把仇恨记给一个尸体。
	if (const AActionCharacterBase* SourceChar = Cast<AActionCharacterBase>(Source))
	{
		if (SourceChar->IsDead())
		{
			return;
		}
	}

	TWeakObjectPtr<AActor> Key(Source);
	float& Current = HatredMap.FindOrAdd(Key);
	Current += Value;

	UE_LOG(
		LogActionMonsterCharacter,
		Verbose,
		TEXT("ActionMonsterCharacter: AddHatred(%s, %.1f) → total %.1f. Owner=%s"),
		*GetNameSafe(Source),
		Value,
		Current,
		*GetNameSafe(this));
}

AActor* AActionMonsterCharacter::GetHighestHatredTarget() const
{
	AActor* Best = nullptr;
	float BestValue = 0.0f;

	// 顺手清理无效项。const 接口里要清理 → const_cast 一下 HatredMap，是合理的"缓存维护"用法。
	TMap<TWeakObjectPtr<AActor>, float>& Map = const_cast<TMap<TWeakObjectPtr<AActor>, float>&>(HatredMap);
	for (auto It = Map.CreateIterator(); It; ++It)
	{
		AActor* Candidate = It->Key.Get();
		if (Candidate == nullptr || !IsValid(Candidate))
		{
			It.RemoveCurrent();
			continue;
		}
		if (const AActionCharacterBase* CharCandidate = Cast<AActionCharacterBase>(Candidate))
		{
			if (CharCandidate->IsDead())
			{
				It.RemoveCurrent();
				continue;
			}
		}
		if (It->Value > BestValue)
		{
			BestValue = It->Value;
			Best = Candidate;
		}
	}
	return Best;
}

int32 AActionMonsterCharacter::GetHatredEntryCount() const
{
	return HatredMap.Num();
}

void AActionMonsterCharacter::ClearHatred()
{
	HatredMap.Reset();
}

void AActionMonsterCharacter::GetHatredTopEntries(int32 N, TArray<FHatredEntry>& OutEntries) const
{
	OutEntries.Reset();
	if (N <= 0)
	{
		return;
	}

	// 拷贝出来排序，避免在 const 上下文修改源 Map 的迭代顺序。
	TArray<FHatredEntry> All;
	All.Reserve(HatredMap.Num());
	for (const TPair<TWeakObjectPtr<AActor>, float>& Pair : HatredMap)
	{
		if (!Pair.Key.IsValid())
		{
			continue;
		}
		FHatredEntry Entry;
		Entry.Target = Pair.Key;
		Entry.Value = Pair.Value;
		All.Add(Entry);
	}
	All.Sort([](const FHatredEntry& A, const FHatredEntry& B) { return A.Value > B.Value; });

	const int32 Count = FMath::Min(N, All.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		OutEntries.Add(All[i]);
	}
}

void AActionMonsterCharacter::FreezeDeathPose()
{
	if (GetMesh() == nullptr)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(0.0f);
	}

	// 同时暂停动画、冻结播放速率，并停掉 Mesh Tick。
	// 这样比只设 bPauseAnims 更稳，不容易被 AnimBP 拉回到待机。
	GetMesh()->bPauseAnims = true;
	GetMesh()->GlobalAnimRateScale = 0.0f;
	GetMesh()->SetComponentTickEnabled(false);

	UE_LOG(LogActionMonsterCharacter, Log, TEXT("ActionMonsterCharacter: Death pose frozen before montage blend-out."));
}
