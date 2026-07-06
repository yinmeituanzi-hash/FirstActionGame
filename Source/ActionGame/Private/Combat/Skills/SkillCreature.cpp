#include "Combat/Skills/SkillCreature.h"

#include "Char/ActionCharacterBase.h"
#include "Combat/Skills/ActionSkillComponent.h"
#include "Components/SphereComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSkillCreature, Log, All);

// -----------------------------------------------------------------------------
// Day8.1 架构骨架：所有关键 API 已就位，函数体在 Step 3 填充。
// 之所以先把签名 + 组件 + 状态字段一次性写全，是为了：
//   1. Subsystem / Logic / EffectLibrary 可以立刻按照这个 API 编写
//   2. Row 字段的语义与运行时状态字段一一对应，无遗漏
//   3. Step 3 实现时只改函数体，不改头文件，减少级联重编与心智负担
// -----------------------------------------------------------------------------

ASkillCreature::ASkillCreature()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(30.0f);
	CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	CollisionSphere->SetGenerateOverlapEvents(true);
	CollisionSphere->SetCanEverAffectNavigation(false);
	RootComponent = CollisionSphere;
}

void ASkillCreature::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionSphere != nullptr)
	{
		CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ASkillCreature::OnSphereBeginOverlap);
	}
}

void ASkillCreature::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CollisionSphere != nullptr)
	{
		CollisionSphere->OnComponentBeginOverlap.RemoveDynamic(this, &ASkillCreature::OnSphereBeginOverlap);
	}
	Super::EndPlay(EndPlayReason);
}

void ASkillCreature::ActivateFromRequest(const FSkillCreatureSpawnRequest& InRequest, const FSkillCreatureRow& InRow)
{
	CreatureId = InRequest.CreatureId;
	SourceSkillId = InRequest.SourceSkillId;
	SourceCharacter = InRequest.SourceCharacter;
	SourceSkillComponent = InRequest.SourceSkillComponent;
	Target = InRequest.Target;

	RuntimeRow = MakeShared<FSkillCreatureRow>(InRow);

	RemainingLifeTime = InRow.LifeTime;
	RemainingBreakCount = InRow.BreakCount;
	RemainingBoundCount = InRow.BoundCount;
	AliveTime = 0.0f;
	bCollisionEnabled = (InRow.CollisionDelayTime <= 0.0f);
	bMarkedForDestroy = false;
	HitActors.Reset();

	// TODO(Day8.1 Step 3):
	//   - 调用 USkillCreatureLogic::ResolveSpawnTransform() 设置 Actor Transform
	//   - 调用 USkillCreatureLogic::ResolveInitialVelocity() 设置 CurrentVelocity
	//   - 根据 Row.CollisionShape 切换 Sphere/Box/Capsule（Day8.2 起用得到）
	//   - CollisionSphere->SetSphereRadius(InRow.CollisionRadius)
	//   - CollisionSphere->SetCollisionProfileName(InRow.CollisionProfile)
	//   - 若 bAttachOwner，AttachToActor(SourceCharacter, ...)
	//   - LoopVFXId 非空时通过 VFXSubsystem 播放并挂到 Root（DelayPlayFX 生效）

	SetActorTickEnabled(true);

	UE_LOG(
		LogSkillCreature,
		Verbose,
		TEXT("SkillCreature[%s]: Activate CreatureId=%s Source=%s Skill=%s"),
		*GetNameSafe(this),
		*CreatureId.ToString(),
		*GetNameSafe(SourceCharacter.Get()),
		*SourceSkillId.ToString());
}

void ASkillCreature::DestroyCreature(ECreatureDestroyReason Reason)
{
	if (bMarkedForDestroy)
	{
		return;
	}
	bMarkedForDestroy = true;

	// TODO(Day8.1 Step 3):
	//   - 执行 RuntimeRow->DestroyEffectIds（回到 USkillEffectLibrary::ExecuteEffect）
	//   - 兼容单值 RuntimeRow->DestroyEffectId
	//   - 停止 LoopVFX
	//   - 通知 USkillCreatureSubsystem 反注册
	//   - DelayDestroyTime > 0 时启动 Timer 再 Destroy
	//   - 否则直接 Destroy()

	UE_LOG(
		LogSkillCreature,
		Verbose,
		TEXT("SkillCreature[%s]: DestroyCreature Reason=%d"),
		*GetNameSafe(this),
		static_cast<int32>(Reason));

	Destroy();
}

void ASkillCreature::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bMarkedForDestroy || !RuntimeRow.IsValid())
	{
		return;
	}

	AliveTime += DeltaSeconds;

	// 延迟开启碰撞
	if (!bCollisionEnabled && AliveTime >= RuntimeRow->CollisionDelayTime)
	{
		bCollisionEnabled = true;
	}

	const FVector OldLocation = GetActorLocation();
	TickMovement(DeltaSeconds);
	const FVector NewLocation = GetActorLocation();

	if (bCollisionEnabled)
	{
		SweepForHits(OldLocation, NewLocation);
	}

	RemainingLifeTime -= DeltaSeconds;
	if (RemainingLifeTime <= 0.0f)
	{
		DestroyCreature(ECreatureDestroyReason::LifeTimeExpired);
	}
}

void ASkillCreature::TickMovement(float /*DeltaSeconds*/)
{
	// TODO(Day8.1 Step 3):
	//   switch (RuntimeRow->MoveMode)
	//   {
	//   case ECreatureMoveMode::Straight: 按 CurrentVelocity 匀速平移
	//   case ECreatureMoveMode::Homing:   延迟后按 HomingSpeed 修正朝目标
	//   case ECreatureMoveMode::Gravity:  ParabolaDelay 后按 GravityScale*GravityFactor 累加 Z 速度
	//   case ECreatureMoveMode::Static:   不移动
	//   }
	//   若 !bFixedMoveRotation，SetActorRotation(CurrentVelocity.Rotation())
}

void ASkillCreature::SweepForHits(const FVector& /*OldLocation*/, const FVector& /*NewLocation*/)
{
	// TODO(Day8.1 Step 3):
	//   World->SweepMultiByChannel(Old→New, CollisionRadius) 兜底命中
	//   命中的 Actor 转 HandleHit()
	//   注意 CollisionProfile / 阵营过滤仍走 HandleHit 内部逻辑
}

void ASkillCreature::HandleHit(AActor* OtherActor, const FHitResult& /*OptionalHit*/)
{
	if (bMarkedForDestroy || OtherActor == nullptr || !bCollisionEnabled || !RuntimeRow.IsValid())
	{
		return;
	}
	if (OtherActor == this || OtherActor == SourceCharacter.Get())
	{
		return;
	}

	const TWeakObjectPtr<AActor> WeakOther(OtherActor);
	if (HitActors.Contains(WeakOther))
	{
		return;
	}

	const ECreatureHitPolicy Policy = ClassifyHit(OtherActor);
	HitActors.Add(WeakOther);

	ExecuteHitEffects(Policy, OtherActor);

	// TODO(Day8.1 Step 3):
	//   - BreakCount 允许时不销毁，只递减
	//   - bDontDestroyExceptLife = true 时忽略销毁
	//   - Policy = HitEnemy + bDestroyOnHit 时 DestroyCreature(HitDestroyed)
	if (RuntimeRow->bDestroyOnHit && !RuntimeRow->bDontDestroyExceptLife && Policy == ECreatureHitPolicy::HitEnemy)
	{
		if (RemainingBreakCount > 0)
		{
			--RemainingBreakCount;
		}
		else
		{
			DestroyCreature(ECreatureDestroyReason::HitDestroyed);
		}
	}
}

ECreatureHitPolicy ASkillCreature::ClassifyHit(AActor* OtherActor) const
{
	// Day8.1 只支持 Enemy 判定，Friend/Scene 留给 Day8.2。
	if (AActionCharacterBase* Source = SourceCharacter.Get())
	{
		if (AActionCharacterBase* Victim = Cast<AActionCharacterBase>(OtherActor))
		{
			if (Source->CanDamageTarget(Victim))
			{
				return ECreatureHitPolicy::HitEnemy;
			}
			return ECreatureHitPolicy::HitFriend;
		}
	}
	return ECreatureHitPolicy::HitScene;
}

void ASkillCreature::ExecuteHitEffects(ECreatureHitPolicy /*Policy*/, AActor* /*OtherActor*/)
{
	// TODO(Day8.1 Step 3):
	//   - 根据 Policy 选择 RuntimeRow->HitEnemyEffectIds / HitFriendEffectIds / HitSceneEffectIds
	//   - Day8.1 Demo 兼容：数组为空时 fallback 到 RuntimeRow->HitEffectId
	//   - 构造 FSkillEffectContext（每次命中一个新的 Context，避免跨命中共享）
	//   - 遍历 EffectIds，从 SourceSkillComponent 的 SkillEffectDataTable 查 Row
	//   - 调用 USkillEffectLibrary::ExecuteEffect(this, SkillComp, ..., Context)
	//   - 若 bDamageSourceIsCreature，把 Source 换成 Creature 自身（后续接口层再做）
}

void ASkillCreature::OnSphereBeginOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& SweepResult)
{
	HandleHit(OtherActor, SweepResult);
}
