#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "HitReactTypes.generated.h"

class UAnimMontage;
class AActor;

/**
 * 受击大类。
 *
 * 设计意图：
 *   - 用最少的枚举值表达"受击表现的根本差别"，避免分类爆炸。
 *   - 具体角色受多大力度可以由 DamageAmount + 阈值映射决定（轻击/重击切换）。
 *
 * 与 010 的对比：
 *   - 010 用 EHitType（LightHit/LightHitRanged/HeavyHit/HitFly/Stun/GrabHit）分得更细。
 *   - 我们暂时不做远程和投技，所以合并成 4 类。Stun 暂保留枚举位，逻辑后续接入。
 */
UENUM(BlueprintType)
enum class EHitReactType : uint8
{
	/** 轻击：普攻命中、轻量打击。播放短促受击动画，不打断硬直。 */
	LightHit UMETA(DisplayName = "Light Hit"),

	/** 重击：蓄力攻击、Boss 重击。受击动画更明显，可能伴随短硬直。 */
	HeavyHit UMETA(DisplayName = "Heavy Hit"),

	/** 击飞：被打飞，物理冲量 + 可选 Ragdoll。 */
	HitFly   UMETA(DisplayName = "Hit Fly"),

	/** 晕眩：进入特殊循环动画，等待外部解除。 */
	Stun     UMETA(DisplayName = "Stun"),
};

/**
 * 受击方向（相对受击者自身朝向）。
 *
 * 计算方式：
 *   把"攻击者→受击者"的水平向量投影到受击者本地坐标系，
 *   然后看落在 Front / Back / Left / Right 哪个 90° 扇区。
 *
 * 注：Front 是"被从前方打"，所以受击者会向后退；用动画时挑"向后倒"的素材。
 */
UENUM(BlueprintType)
enum class EHitReactDirection : uint8
{
	Front UMETA(DisplayName = "Front (被前方打)"),
	Back  UMETA(DisplayName = "Back  (被后方打)"),
	Left  UMETA(DisplayName = "Left  (被左方打)"),
	Right UMETA(DisplayName = "Right (被右方打)"),
};

/**
 * 受击上下文：所有受击响应所需的输入信息。
 *
 * 设计意图：
 *   - 用一个值类型贯穿"攻击源 → HitReceiver → Feedback/React/Physics 三层"。
 *   - 任何想加新字段的人只需要扩展这里，不必改各层接口签名。
 *
 * 后续扩展点：
 *   - 元素属性（火/水/雷）可以加在这里 → React 表加列 → 选元素特化蒙太奇。
 *   - 暴击标记 → Feedback 层做强化震屏与粒子。
 *   - 命中部位（头/胸/腿）→ React 表挑部位特化动画。
 */
USTRUCT(BlueprintType)
struct ACTIONGAME_API FHitContext
{
	GENERATED_BODY()

	FHitContext() = default;

	/** 攻击者，可空（环境伤害情况下）。用于 HitStop / 转向 / 仇恨。 */
	UPROPERTY(BlueprintReadWrite, Category = "HitContext")
	TObjectPtr<AActor> Attacker = nullptr;

	/** 命中世界位置（建议是剑刃接触点或胶囊体中心高度）。用于粒子/音效定位。 */
	UPROPERTY(BlueprintReadWrite, Category = "HitContext")
	FVector HitLocation = FVector::ZeroVector;

	/**
	 * 命中方向（攻击施加的方向，世界坐标）。
	 * 一般取 Attacker→Victim 的水平归一化向量。
	 * 受击方向由这个向量与受击者朝向反算得到。
	 */
	UPROPERTY(BlueprintReadWrite, Category = "HitContext")
	FVector HitDirection = FVector::ForwardVector;

	/** 受击大类。决定走哪一套表现链路。 */
	UPROPERTY(BlueprintReadWrite, Category = "HitContext")
	EHitReactType ReactType = EHitReactType::LightHit;

	/** 本次造成的伤害值。可用于反馈强度缩放、轻/重切换阈值。 */
	UPROPERTY(BlueprintReadWrite, Category = "HitContext")
	float DamageAmount = 0.0f;

	/**
	 * 反馈强度系数 [0, 1]。
	 * 1.0 = 满强度（震屏/HitStop 取默认）；
	 * 0.5 = 半强度；攻击者通常按"伤害/角色基础攻击力"计算后传入。
	 */
	UPROPERTY(BlueprintReadWrite, Category = "HitContext")
	float FeedbackScale = 1.0f;

	/** 是否在播放受击动画前把角色 Yaw 转向攻击者。需要"挨打面对镜头"时打开。 */
	UPROPERTY(BlueprintReadWrite, Category = "HitContext")
	bool bRotateToAttacker = false;

	/**
	 * 强制指定的蒙太奇 RowName。
	 * 若不为空，HitReactFeature 直接走这一行而不走 ReactType+Direction 匹配。
	 * 用于 Boss 特殊技或剧情触发的固定受击。
	 */
	UPROPERTY(BlueprintReadWrite, Category = "HitContext")
	FName MontageOverrideRow = NAME_None;

	/**
	 * 击飞物理参数（仅当 ReactType==HitFly 才使用）。
	 * 这部分参数交给 HitPhysicsComponent 解读。
	 */
	UPROPERTY(BlueprintReadWrite, Category = "HitContext|Physics")
	float HitFlyXYStrength = 800.0f;

	UPROPERTY(BlueprintReadWrite, Category = "HitContext|Physics")
	float HitFlyZStrength = 400.0f;

	/** 击飞时是否进入 Ragdoll 物理。false = 蒙太奇击飞。 */
	UPROPERTY(BlueprintReadWrite, Category = "HitContext|Physics")
	bool bUseRagdoll = false;
};

/**
 * 受击蒙太奇配置行。HitReactFeature 的 DataTable 行类型。
 *
 * 表格使用方式：
 *   - 在编辑器里新建 Miscellaneous → DataTable，行结构选 FHitMontageRow。
 *   - 每个角色（玩家/小怪/Boss）通常配一张表，也可共享。
 *   - HitReactFeature.HitMontageTable 字段引用对应表。
 *
 * 匹配策略（HitReactFeature 内部）：
 *   1. 若 FHitContext.MontageOverrideRow 非空 → 直接按该 RowName 取行。
 *   2. 否则遍历表，按"ReactType + Direction 同时匹配"返回第一条命中。
 *   3. 找不到时降级到 ReactType 匹配（任意方向），再找不到记 Warning 不播。
 */
USTRUCT(BlueprintType)
struct ACTIONGAME_API FHitMontageRow : public FTableRowBase
{
	GENERATED_BODY()

	FHitMontageRow() = default;

	/** 该行对应的受击大类。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitMontage")
	EHitReactType ReactType = EHitReactType::LightHit;

	/** 该行对应的受击方向。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitMontage")
	EHitReactDirection Direction = EHitReactDirection::Front;

	/** 受击蒙太奇资源。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitMontage")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	/** 蒙太奇播放速率倍率。1.0 = 原速。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitMontage", meta = (ClampMin = "0.1"))
	float PlayRate = 1.0f;

	/** 起始 Section 名（蒙太奇内含多段时使用）。NAME_None = 从开头播。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitMontage")
	FName StartSection = NAME_None;
};
