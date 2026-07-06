#pragma once

#include "CoreMinimal.h"
#include "AI/Noise/AINoiseTypes.h"
#include "Animation/AnimMontage.h"
#include "Combat/Skills/SkillCreatureTypes.h"
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
 *
 * SkillCreature 层的类型集（枚举 / FSkillCreatureRow / Request / HitResult）
 * 集中放在 Combat/Skills/SkillCreatureTypes.h，这里只承载 Skill / Node / Effect
 * 三条主 Row。
 */

UENUM(BlueprintType)
enum class ESkillType : uint8
{
	NormalAttack UMETA(DisplayName = "Normal Attack"),
	HeavyAttack UMETA(DisplayName = "Heavy Attack"),
	Skill UMETA(DisplayName = "Skill"),
	Ultimate UMETA(DisplayName = "Ultimate"),
	Passive UMETA(DisplayName = "Passive")
};

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ESkillCancelFlag : uint8
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
ENUM_CLASS_FLAGS(ESkillCancelFlag);

UENUM(BlueprintType)
enum class ESkillStopReason : uint8
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
enum class ESkillEffectTiming : uint8
{
	Enter UMETA(DisplayName = "Enter"),
	Leave UMETA(DisplayName = "Leave"),
	Notify UMETA(DisplayName = "Notify")
};

UENUM(BlueprintType)
enum class ESkillEffectType : uint8
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
enum class ESkillTargetFilter : uint8
{
	Sphere UMETA(DisplayName = "Sphere"),
	Cone UMETA(DisplayName = "Cone"),
	Capsule UMETA(DisplayName = "Capsule"),
	Box UMETA(DisplayName = "Box"),
	SweepBox UMETA(DisplayName = "Sweep Box"),
	Self UMETA(DisplayName = "Self"),
	TargetActor UMETA(DisplayName = "Target Actor")
};

USTRUCT(BlueprintType)
struct ACTIONGAME_API FSkillRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill")
	FName SkillId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill")
	ESkillType SkillType = ESkillType::Skill;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill")
	FName BeginNodeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill", meta = (ClampMin = "0.0"))
	float Cooldown = 0.0f;

	/**
	 * AI 释放距离配置。
	 *
	 * 这是“行为树是否应该释放该技能”的决策距离，不是命中判定范围。
	 * 命中体大小仍放在 SkillEffect / SkillCreature 中，类似 010 中 UseSkill 条件和 Effect 范围分层。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|AI")
	bool bUseReleaseRange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|AI", meta = (EditCondition = "bUseReleaseRange", ClampMin = "0.0"))
	float MinReleaseRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|AI", meta = (EditCondition = "bUseReleaseRange", ClampMin = "0.0"))
	float MaxReleaseRange = 0.0f;

	/** AI 走位时更想停留的距离。0 表示使用 MaxReleaseRange。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|AI", meta = (EditCondition = "bUseReleaseRange", ClampMin = "0.0"))
	float PreferredReleaseRange = 0.0f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill", meta = (Bitmask, BitmaskEnum = "/Script/ActionGame.ESkillCancelFlag"))
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
struct ACTIONGAME_API FSkillNodeRow : public FTableRowBase
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
struct ACTIONGAME_API FSkillEffectRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	FName EffectId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	ESkillEffectTiming ExecuteTiming = ESkillEffectTiming::Notify;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	FName NotifyName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	ESkillEffectType EffectType = ESkillEffectType::Damage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect")
	ESkillTargetFilter TargetFilter = ESkillTargetFilter::Sphere;

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

	// ===== SpawnCreature Effect 专用字段 =====

	/** 引用的 SkillCreature Row Id。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect|SpawnCreature")
	FName SpawnCreatureId = NAME_None;

	/** 出生位置解析模式，Row 的 SpawnSocket 为兜底。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect|SpawnCreature")
	ECreatureSpawnTargetMode SpawnMode = ECreatureSpawnTargetMode::SelfOffset;

	/** SpawnMode = SourceSocket/WeaponSocket 时的 socket 名，为空则使用 Row 上的 SpawnSocket。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect|SpawnCreature")
	FName SpawnSocket = NAME_None;

	/** 相对偏移（Local：X 前, Y 右, Z 上），叠加在 Row.BornLocationOffset 之上。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect|SpawnCreature")
	FVector SpawnOffset = FVector::ZeroVector;

	/** 初始朝向解析模式。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect|SpawnCreature")
	ECreatureDirectionMode DirectionMode = ECreatureDirectionMode::SourceForward;

	/** 本次生成几发（散弹 / 多发预留）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect|SpawnCreature", meta = (ClampMin = "1"))
	int32 SpawnCount = 1;

	/** 散射半角（度），围绕主方向左右均匀分布。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Effect|SpawnCreature", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float SpawnSpreadAngleDeg = 0.0f;

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
