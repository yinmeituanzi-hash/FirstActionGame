#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "SkillCreatureTypes.generated.h"

class AActor;
class ASkillCreature;
class AActionCharacterBase;
class UActionSkillComponent;

/**
 * SkillCreature 类型集：Creature 系统在配置层与运行时层用到的所有共用类型。
 *
 * - 枚举（Spawn/Direction/CollisionShape/HitPolicy/DestroyReason/MoveMode）
 * - Row：FSkillCreatureRow（DT_SkillCreatures 的行结构）
 * - 请求 / 结果结构：FSkillCreatureSpawnRequest / FSkillCreatureHitResult
 *
 * 集中放在同一个头文件是为了：
 * - ActionSkillTypes.h 不必再感知 SkillCreature（Skill/Node/Effect Row 层独立）
 * - SkillCreatureSubsystem/Logic/Creature 都只 include 本头文件即可覆盖全部依赖
 * - 未来加"Row 到运行时"的字段映射时，改动集中在一处
 */

/** SkillCreature 出生位置的解析模式。 */
UENUM(BlueprintType)
enum class ECreatureSpawnTargetMode : uint8
{
	/** 直接使用 Effect 位置（角色前方偏移，兼容 Day8.1 的最小实现）。 */
	SelfOffset UMETA(DisplayName = "Self Offset"),
	/** 角色 Mesh 上的 socket，例如武器口、手部。 */
	SourceSocket UMETA(DisplayName = "Source Socket"),
	/** 当前武器 Mesh 上的 socket。 */
	WeaponSocket UMETA(DisplayName = "Weapon Socket"),
	/** 目标角色位置（追踪弹或以敌人为原点的场地效果）。 */
	TargetActor UMETA(DisplayName = "Target Actor"),
	/** SkillEffect 显式传入的世界坐标（例如点击地面选点技能）。 */
	WorldLocation UMETA(DisplayName = "World Location")
};

/** SkillCreature 出生朝向 / 初始速度方向的解析模式。 */
UENUM(BlueprintType)
enum class ECreatureDirectionMode : uint8
{
	/** 角色 Actor 的 Forward。 */
	SourceForward UMETA(DisplayName = "Source Forward"),
	/** 从出生点指向当前 Target 的方向。 */
	TowardsTarget UMETA(DisplayName = "Towards Target"),
	/** SkillEffect 显式传入的世界方向。 */
	WorldDirection UMETA(DisplayName = "World Direction")
};

/** SkillCreature 碰撞形状。第一版只启用 Sphere，其它值保留供 Day8.2 使用。 */
UENUM(BlueprintType)
enum class ECreatureCollisionShape : uint8
{
	Sphere UMETA(DisplayName = "Sphere"),
	Box UMETA(DisplayName = "Box"),
	Capsule UMETA(DisplayName = "Capsule")
};

/** SkillCreature 命中不同阵营时的 Effect 归组，Day8.1 只启用 HitEnemy。 */
UENUM(BlueprintType)
enum class ECreatureHitPolicy : uint8
{
	HitEnemy UMETA(DisplayName = "Hit Enemy"),
	HitFriend UMETA(DisplayName = "Hit Friend"),
	HitScene UMETA(DisplayName = "Hit Scene")
};

/** SkillCreature 结束原因。 */
UENUM(BlueprintType)
enum class ECreatureDestroyReason : uint8
{
	/** 生命周期正常结束。 */
	LifeTimeExpired UMETA(DisplayName = "LifeTime Expired"),
	/** 命中并按配置销毁。 */
	HitDestroyed UMETA(DisplayName = "Hit Destroyed"),
	/** 场景反弹次数用尽。 */
	BoundExhausted UMETA(DisplayName = "Bound Exhausted"),
	/** 发射者死亡或显式回收。 */
	Forced UMETA(DisplayName = "Forced")
};

/**
 * SkillEffect → Subsystem 的一次生成请求。
 *
 * 这里刻意不包含 Row 引用本身：Row 是配置层数据，Subsystem 内部再去查表；
 * 请求只描述"这一次生成的现场参数"（谁发的、朝向哪里、几发、散射角）。
 */
USTRUCT(BlueprintType)
struct ACTIONGAME_API FSkillCreatureSpawnRequest
{
	GENERATED_BODY()

	/** 配置行 Id，指向 DT_SkillCreatures。 */
	UPROPERTY(BlueprintReadWrite, Category = "Action|Skill|Creature")
	FName CreatureId = NAME_None;

	/** 触发本次生成的技能 Id，用于 HitContext 与统计归属。 */
	UPROPERTY(BlueprintReadWrite, Category = "Action|Skill|Creature")
	FName SourceSkillId = NAME_None;

	/** 发射者角色。Creature 命中执行 Effect 时的默认 Source。 */
	UPROPERTY(BlueprintReadWrite, Transient, Category = "Action|Skill|Creature")
	TWeakObjectPtr<AActionCharacterBase> SourceCharacter;

	/** 发射者的 SkillComponent（可选，用于回调路径）。 */
	UPROPERTY(BlueprintReadWrite, Transient, Category = "Action|Skill|Creature")
	TWeakObjectPtr<UActionSkillComponent> SourceSkillComponent;

	/** 当前锁定 / 传入的目标（追踪弹、TowardsTarget 使用）。 */
	UPROPERTY(BlueprintReadWrite, Transient, Category = "Action|Skill|Creature")
	TWeakObjectPtr<AActor> Target;

	/** 出生位置解析模式。 */
	UPROPERTY(BlueprintReadWrite, Category = "Action|Skill|Creature")
	ECreatureSpawnTargetMode SpawnMode = ECreatureSpawnTargetMode::SelfOffset;

	/** 出生朝向解析模式。 */
	UPROPERTY(BlueprintReadWrite, Category = "Action|Skill|Creature")
	ECreatureDirectionMode DirectionMode = ECreatureDirectionMode::SourceForward;

	/** SpawnMode = SourceSocket / WeaponSocket 时使用的 socket 名。 */
	UPROPERTY(BlueprintReadWrite, Category = "Action|Skill|Creature")
	FName SpawnSocket = NAME_None;

	/** 在出生点基础上叠加的相对偏移（Local 空间：X 前, Y 右, Z 上）。 */
	UPROPERTY(BlueprintReadWrite, Category = "Action|Skill|Creature")
	FVector SpawnOffset = FVector::ZeroVector;

	/** SpawnMode = WorldLocation 时的显式世界位置。 */
	UPROPERTY(BlueprintReadWrite, Category = "Action|Skill|Creature")
	FVector ExplicitWorldLocation = FVector::ZeroVector;

	/** DirectionMode = WorldDirection 时的显式方向（单位向量）。 */
	UPROPERTY(BlueprintReadWrite, Category = "Action|Skill|Creature")
	FVector ExplicitWorldDirection = FVector::ForwardVector;

	/** 本次共生成几发（散弹 / 多发预留，Day8.1 默认为 1）。 */
	UPROPERTY(BlueprintReadWrite, Category = "Action|Skill|Creature", meta = (ClampMin = "1"))
	int32 SpawnCount = 1;

	/** 多发时的散射半角（度），围绕主方向左右均匀分布。 */
	UPROPERTY(BlueprintReadWrite, Category = "Action|Skill|Creature", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float SpreadAngleDeg = 0.0f;
};

/** SkillCreature 一次命中的解算结果，供 Subsystem/Debug 统计使用。 */
USTRUCT(BlueprintType)
struct ACTIONGAME_API FSkillCreatureHitResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Action|Skill|Creature")
	TWeakObjectPtr<AActor> HitActor;

	UPROPERTY(BlueprintReadOnly, Category = "Action|Skill|Creature")
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Action|Skill|Creature")
	FVector HitNormal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Action|Skill|Creature")
	ECreatureHitPolicy Policy = ECreatureHitPolicy::HitEnemy;
};

/** SkillCreature 的位移模式。 */
UENUM(BlueprintType)
enum class ECreatureMoveMode : uint8
{
	Straight UMETA(DisplayName = "Straight"),
	Homing UMETA(DisplayName = "Homing"),
	Gravity UMETA(DisplayName = "Gravity"),
	Static UMETA(DisplayName = "Static")
};

/**
 * 技能生成物配置。Day8.1 只启用一部分字段，其余为 010 SkillCreatureData 对齐预留，
 * 目的是让 Row 结构稳定不变，后续里程碑（Day8.2~8.5）只需要打开对应功能开关即可。
 *
 * 字段分组（对应 SkillCreature_010_Research_Day8.md 第 10.1 节）：
 *   身份 / 池化：CreatureId、Tags、ActorClass、bEnterPool、bUseBulletCreature、bDestroyWithCreator
 *   出生 / Attach：SpawnSocket、BornLocationOffset、bAttachOwner、bFixedBornRotation、bFixedMoveRotation
 *   碰撞：CollisionProfile、CollisionShape、CollisionRadius、CollisionBoxExtent、CollisionCapsuleHalfHeight
 *         SceneCollisionRadius、CollisionDelayTime
 *   移动：MoveMode、InitialSpeed、HomingSpeed、HomingDelay
 *         GravityScale、GravityFactor、bGravityAdapt、ParabolaDelay、TimeToHit
 *   命中：HitEnemyEffectIds、HitFriendEffectIds、HitSceneEffectIds、HitOthersEffectIds
 *         BreakCount、bDontDestroyExceptLife、bDestroyOnHit
 *   反弹：BoundCount、BoundAttenuation
 *   销毁 / VFX / 延迟：LifeTime、DestroyEffectIds、DelayEffect、DelayDestroyTime、DelayPlayFX
 *         LoopVFXId、DamageSourceIsCreature
 */
USTRUCT(BlueprintType)
struct ACTIONGAME_API FSkillCreatureRow : public FTableRowBase
{
	GENERATED_BODY()

	// ===== 身份 / 池化 =====

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Id")
	FName CreatureId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Id")
	TSubclassOf<ASkillCreature> ActorClass = nullptr;

	/** 分类标签，供外部系统过滤/查询（例如 "Bullet.Fireball"、"AoE.Field"）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Id")
	FGameplayTagContainer Tags;

	/** 是否走对象池。Day8.1 池骨架已就位但 Acquire 一律回退 SpawnActor。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Id")
	bool bEnterPool = false;

	/** 是否走 BulletCreature 通道（Day8.4 才启用）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Id")
	bool bUseBulletCreature = false;

	/** 发射者死亡时是否随之销毁。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Id")
	bool bDestroyWithCreator = false;

	// ===== 出生 / Attach =====

	/** Row 上的默认 socket（Effect 未指定时使用）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Spawn")
	FName SpawnSocket = NAME_None;

	/** socket / Actor 之上的默认相对偏移（Local：X 前, Y 右, Z 上）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Spawn")
	FVector BornLocationOffset = FVector::ZeroVector;

	/** 是否 attach 到发射者。Orbit / 环绕类必开。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Spawn")
	bool bAttachOwner = false;

	/** 出生时是否固定朝向（否则由 Request.DirectionMode 决定）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Spawn")
	bool bFixedBornRotation = false;

	/** 移动过程中 Actor 是否随速度旋转（火球 Mesh 是否随飞行方向转）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Spawn")
	bool bFixedMoveRotation = false;

	// ===== 碰撞 =====

	/** UE Collision Profile 名。写进 Row，避免硬编码通道。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Collision")
	FName CollisionProfile = FName(TEXT("OverlapAllDynamic"));

	/** Day8.1 只启用 Sphere；Box/Capsule 保留供 Day8.2。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Collision")
	ECreatureCollisionShape CollisionShape = ECreatureCollisionShape::Sphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Collision", meta = (ClampMin = "0.0"))
	float CollisionRadius = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Collision")
	FVector CollisionBoxExtent = FVector(30.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Collision", meta = (ClampMin = "0.0"))
	float CollisionCapsuleHalfHeight = 30.0f;

	/**
	 * 场景碰撞半径（可独立于角色碰撞配置）。
	 * 0 表示与 CollisionRadius 相同。允许"对角色薄片、对墙壁球体"这种组合。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Collision", meta = (ClampMin = "0.0"))
	float SceneCollisionRadius = 0.0f;

	/** 生成后延迟多少秒才开启碰撞（避免刚出生就打到自己）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Collision", meta = (ClampMin = "0.0"))
	float CollisionDelayTime = 0.0f;

	// ===== 移动 / 追踪 / 重力 =====

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Movement")
	ECreatureMoveMode MoveMode = ECreatureMoveMode::Straight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Movement", meta = (ClampMin = "0.0"))
	float InitialSpeed = 1200.0f;

	/** Homing 修正速率（度/秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Movement", meta = (ClampMin = "0.0"))
	float HomingSpeed = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Movement", meta = (ClampMin = "0.0"))
	float HomingDelay = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Movement")
	float GravityScale = 0.0f;

	/** 额外重力系数（预留场景/天气修正）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Movement")
	float GravityFactor = 1.0f;

	/** 是否自动求重力使抛物线正好落到目标（Day8.2 起可启用）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Movement")
	bool bGravityAdapt = false;

	/** 抛物线开始受重力前的直飞时间（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Movement", meta = (ClampMin = "0.0"))
	float ParabolaDelay = 0.0f;

	/** 期望命中时间（抛物线求速时使用），0 = 不使用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Movement", meta = (ClampMin = "0.0"))
	float TimeToHit = 0.0f;

	// ===== 命中 =====

	/** 打到敌人时执行的 Effect 列表（对应 010 HitEnemy）。Day8.1 只启用这个数组。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Hit")
	TArray<FName> HitEnemyEffectIds;

	/** 打到友军时执行的 Effect 列表（Day8.2）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Hit")
	TArray<FName> HitFriendEffectIds;

	/** 打到场景时执行的 Effect 列表（Day8.2）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Hit")
	TArray<FName> HitSceneEffectIds;

	/** 打到其他特殊物体（破坏体、道具）时的 Effect 列表。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Hit")
	TArray<FName> HitOthersEffectIds;

	/** 允许穿透的次数（0 = 只命中一次）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Hit", meta = (ClampMin = "0"))
	int32 BreakCount = 0;

	/** 命中后是否销毁（bDestroyOnHit=false + BreakCount 组合可以做穿透弹）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Hit")
	bool bDestroyOnHit = true;

	/** 只允许生命周期结束时销毁（忽略命中销毁请求）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Hit")
	bool bDontDestroyExceptLife = false;

	// ===== 反弹 =====

	/** 允许场景反弹次数（0 = 不反弹）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Bound", meta = (ClampMin = "0"))
	int32 BoundCount = 0;

	/** 每次反弹后的速度衰减系数（1 = 无衰减）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Bound", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BoundAttenuation = 0.6f;

	// ===== 销毁 / 生命周期 =====

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Life", meta = (ClampMin = "0.0"))
	float LifeTime = 3.0f;

	/** 销毁时统一触发的 Effect 列表（爆炸、大范围伤害、场景震动）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Life")
	TArray<FName> DestroyEffectIds;

	/** 单个 DestroyEffect 兼容字段——当 DestroyEffectIds 空时使用。Day8.1 编辑器 Demo 用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Life")
	FName DestroyEffectId = NAME_None;

	/** Day8.1 编辑器 Demo 用的单个 HitEffect 兼容字段——当 HitEnemyEffectIds 空时使用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Life")
	FName HitEffectId = NAME_None;

	/** 生成后延迟多久开始执行 Effects（例如先蓄力再打）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Life", meta = (ClampMin = "0.0"))
	float DelayEffect = 0.0f;

	/** 命中后延迟多久销毁（爆炸残留 / VFX 完整播放）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Life", meta = (ClampMin = "0.0"))
	float DelayDestroyTime = 0.0f;

	// ===== VFX / 表现 =====

	/** LoopVFX 名（挂载在 Creature Root 上，销毁时统一停止）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|VFX")
	FName LoopVFXId = NAME_None;

	/** 延迟多久开始播 LoopVFX（0 = 立即）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|VFX", meta = (ClampMin = "0.0"))
	float DelayPlayFX = 0.0f;

	// ===== 伤害归属 =====

	/**
	 * Effect 执行时的 Source 归属：
	 *   false（默认）= 发射者角色（伤害/仇恨/成就正确归到玩家或怪物身上）
	 *   true         = Creature 自己（少数召唤物代打场景）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|Skill|Creature|Damage")
	bool bDamageSourceIsCreature = false;
};
