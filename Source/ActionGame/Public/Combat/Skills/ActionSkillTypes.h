#pragma once

#include "CoreMinimal.h"
#include "AI/Noise/AINoiseTypes.h"
#include "Animation/AnimMontage.h"
#include "Combat/Skills/ActionSkillCreature.h"
#include "Combat/HitReact/HitReactTypes.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "ActionSkillTypes.generated.h"

/**
 * 技能系统的数据定义文件。
 *
 * 这些结构体主要用于 DataTable 配置。冷却、当前节点、已命中目标、
 * 已生成特效句柄等运行时状态属于 SkillObject / SkillComponent，
 * 不应该写回这些配置行。
 */

UENUM(BlueprintType)
enum class EActionSkillType : uint8
{
	NormalAttack UMETA(DisplayName = "Normal Attack"),
	HeavyAttack UMETA(DisplayName = "Heavy Attack"),
	Skill UMETA(DisplayName = "Skill"),
	Ultimate UMETA(DisplayName = "Ultimate"),
	Passive UMETA(DisplayName = "Passive")
};

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EActionSkillCancelFlag : uint8
{
	None = 0 UMETA(Hidden),
	NormalAttack = 1 << 0 UMETA(DisplayName = "Normal Attack"),
	HeavyAttack = 1 << 1 UMETA(DisplayName = "Heavy Attack"),
	Dodge = 1 << 2 UMETA(DisplayName = "Dodge"),
	Skill = 1 << 3 UMETA(DisplayName = "Skill"),
	Jump = 1 << 4 UMETA(DisplayName = "Jump"),
	Ultimate = 1 << 5 UMETA(DisplayName = "Ultimate"),
	Move = 1 << 6 UMETA(DisplayName = "Move")
};
ENUM_CLASS_FLAGS(EActionSkillCancelFlag);

UENUM(BlueprintType)
enum class EActionSkillStopReason : uint8
{
	Normal UMETA(DisplayName = "Normal"),
	SkillCancel UMETA(DisplayName = "Skill Cancel"),
	DodgeCancel UMETA(DisplayName = "Dodge Cancel"),
	JumpCancel UMETA(DisplayName = "Jump Cancel"),
	MoveCancel UMETA(DisplayName = "Move Cancel"),
	HitInterrupt UMETA(DisplayName = "Hit Interrupt"),
	Death UMETA(DisplayName = "Death"),
	Forced UMETA(DisplayName = "Forced")
};

UENUM(BlueprintType)
enum class EActionSkillEffectTiming : uint8
{
	Enter UMETA(DisplayName = "Enter"),
	Leave UMETA(DisplayName = "Leave"),
	Notify UMETA(DisplayName = "Notify")
};

UENUM(BlueprintType)
enum class EActionSkillEffectType : uint8
{
	Damage UMETA(DisplayName = "Damage"),
	SpawnCreature UMETA(DisplayName = "Spawn Creature"),
	PlayVFX UMETA(DisplayName = "Play VFX"),
	StopVFX UMETA(DisplayName = "Stop VFX"),
	ReportNoise UMETA(DisplayName = "Report Noise"),
	ApplyTagForDuration UMETA(DisplayName = "Apply Tag For Duration"),
	Stun UMETA(DisplayName = "Stun"),
	RestoreSP UMETA(DisplayName = "Restore SP")
};

UENUM(BlueprintType)
enum class EActionSkillTargetFilter : uint8
{
	Sphere UMETA(DisplayName = "Sphere"),
	Cone UMETA(DisplayName = "Cone"),
	Capsule UMETA(DisplayName = "Capsule"),
	Box UMETA(DisplayName = "Box"),
	SweepBox UMETA(DisplayName = "Sweep Box"),
	Self UMETA(DisplayName = "Self"),
	TargetActor UMETA(DisplayName = "Target Actor")
};

UENUM(BlueprintType)
enum class EActionCreatureMoveMode : uint8
{
	Straight UMETA(DisplayName = "Straight"),
	Homing UMETA(DisplayName = "Homing"),
	Gravity UMETA(DisplayName = "Gravity"),
	Static UMETA(DisplayName = "Static")
};

USTRUCT(BlueprintType)
struct ACTIONGAME_API FActionSkillRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill")
	FName SkillId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill")
	EActionSkillType SkillType = EActionSkillType::Skill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill")
	FName BeginNodeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill", meta = (ClampMin = "0.0"))
	float Cooldown = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill", meta = (ClampMin = "1"))
	int32 MaxUseCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill")
	bool bAllowInAir = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill")
	bool bBlockMove = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill")
	bool bBlockDodge = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill")
	bool bBlockAttack = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill", meta = (Bitmask, BitmaskEnum = "/Script/ActionGame.EActionSkillCancelFlag"))
	int32 AllowCancelBy = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill")
	bool bReportCombatNoise = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill", meta = (ClampMin = "0.0"))
	float NoiseLoudness = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill")
	bool bStartCooldownOnHitInterrupt = true;
};

/** 技能内部的一个可播放动作节点，通常对应一个 Montage 或 Montage Section。 */
USTRUCT(BlueprintType)
struct ACTIONGAME_API FActionSkillNodeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Node")
	FName NodeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Node")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Node", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Node")
	FName StartSection = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Node")
	FName AnimSlot = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Node")
	FName NextNodeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Node")
	FName BranchNodeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Node")
	TArray<FName> EffectIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Node")
	bool bUseRootMotion = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Node", meta = (ClampMin = "0.0"))
	float RootMotionScale = 1.0f;

	/**
	 * 当怪物与目标距离 <= 胶囊半径 * RootMotionRadius 时，自动禁用 Root Motion 防止穿模。
	 * 0 表示不做距离裁剪。仅对非玩家角色生效。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Node", meta = (ClampMin = "0.0"))
	float RootMotionRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Node", meta = (ClampMin = "0"))
	int32 CostSP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Node", meta = (ClampMin = "0.0"))
	float MontageBlendOutTime = 0.1f;
};

/** 数据驱动的技能效果，可在进入节点、离开节点或指定 AnimNotify 时执行。 */
USTRUCT(BlueprintType)
struct ACTIONGAME_API FActionSkillEffectRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	FName EffectId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	EActionSkillEffectTiming ExecuteTiming = EActionSkillEffectTiming::Notify;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	FName NotifyName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	EActionSkillEffectType EffectType = EActionSkillEffectType::Damage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	EActionSkillTargetFilter TargetFilter = EActionSkillTargetFilter::Sphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect", meta = (ClampMin = "0.0"))
	float Radius = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	float ForwardOffset = 120.0f;

	/** Box / SweepBox 使用的半尺寸。X=前后长度半径，Y=左右宽度半径，Z=高度半径。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	FVector BoxExtent = FVector(160.0f, 80.0f, 80.0f);

	/** Capsule 使用的半高。Radius 字段作为 Capsule 半径。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect", meta = (ClampMin = "0.0"))
	float CapsuleHalfHeight = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect", meta = (ClampMin = "0.0"))
	float HitLocationBackstep = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ConeHalfAngle = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect", meta = (ClampMin = "0.0"))
	float DamageScale = 1.0f;

	/**
	 * 是否在当前 SkillNode 内共享命中去重。
	 *
	 * false：默认行为。每次 Effect 执行都可以再次命中同一目标，适合多段斩、多次 Hit Notify。
	 * true：同一个节点里只命中同一目标一次，适合一个长窗口里可能多次触发检测但只想结算一次的技能。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	bool bDeduplicateHitsWithinNode = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	EHitReactType ReactType = EHitReactType::LightHit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect", meta = (ClampMin = "0.0"))
	float FeedbackScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	bool bUseRagdoll = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect", meta = (ClampMin = "0.0"))
	float HitFlyXY = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect", meta = (ClampMin = "0.0"))
	float HitFlyZ = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect", meta = (ClampMin = "0.0"))
	float HatredBonus = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect", meta = (ClampMin = "0.0"))
	float NoiseLoudness = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	EAINoiseCategory NoiseCategory = EAINoiseCategory::Combat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	FName SpawnCreatureId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	FName VFXId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	FName VFXStopGroup = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	FGameplayTag TagToApply;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect", meta = (ClampMin = "0.0"))
	float TagDuration = 0.0f;

	/** RestoreSP 效果类型使用：每次命中恢复的 SP 量。实际总回复 = SPRestoreAmount * 本次命中数。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect", meta = (ClampMin = "0"))
	int32 SPRestoreAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	bool bDrawDebug = false;
};

/** 技能生成物配置，后续用于投射物、陷阱、范围场等。 */
USTRUCT(BlueprintType)
struct ACTIONGAME_API FActionSkillCreatureRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature")
	FName CreatureId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature")
	TSubclassOf<AActionSkillCreature> ActorClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature", meta = (ClampMin = "0.0"))
	float LifeTime = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature", meta = (ClampMin = "0.0"))
	float InitialSpeed = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature", meta = (ClampMin = "0.0"))
	float CollisionRadius = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature")
	EActionCreatureMoveMode MoveMode = EActionCreatureMoveMode::Straight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature", meta = (ClampMin = "0.0"))
	float HomingSpeed = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature", meta = (ClampMin = "0.0"))
	float HomingDelay = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature")
	float GravityScale = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature")
	FName HitEffectId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature")
	FName DestroyEffectId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature")
	bool bDestroyOnHit = true;
};
