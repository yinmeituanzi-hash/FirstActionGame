#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/HitReact/HitReactTypes.h"
#include "HitReactComponent.generated.h"

class AActionCharacterBase;
class UAnimInstance;
class UAnimMontage;
class UDataTable;

/**
 * UHitReactComponent
 *
 * 受击动画反应组件。挂在 AActionCharacterBase（玩家和怪物共用）。
 *
 * ─────────────────────────────────────────────────────────────────────
 * 设计为 ActorComponent 而不是 UActionFeatureBase 子类的原因：
 *   - ActionFeature 体系是"玩家主动动作"（攻击/闪避/跳跃），强依赖 PlayerCharacter
 *     的输入缓冲、相机、锁定逻辑，且基类 OwnerChar 类型是 AActionPlayerCharacter*。
 *   - HitReact 是"被动响应"——所有角色都需要、不需要输入、不需要相机、不需要互斥。
 *   - 与 HitFeedback / HitPhysics 命名风格保持一致（都是 *Component）。
 * ─────────────────────────────────────────────────────────────────────
 *
 * 职责：
 *   1. 持有受击蒙太奇 DataTable（FHitMontageRow）
 *   2. 按 FHitContext 选择合适的蒙太奇并播放
 *   3. 维护受击 CD（防止短时间被连续击中导致动画反复重播）
 *   4. 管理 Block.HitReact / State.Action.HitReact Tag 生命周期
 *   5. 受击替换表（Day 3 接入：将某些受击类型转换为另一类）
 *
 * 调用方：HitReceiverComponent.DispatchReact(Ctx)
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UHitReactComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHitReactComponent();

	// ==================== 配置 ====================

	/**
	 * 受击蒙太奇 DataTable，行类型必须是 FHitMontageRow。
	 *
	 * 编辑器配置步骤（Content Browser）：
	 *   1. 右键 → Miscellaneous → DataTable
	 *   2. 选择 Row Structure: HitMontageRow
	 *   3. 命名建议：DT_HitMontages_Player / DT_HitMontages_Monster
	 *   4. 在 BP_ActionPlayerCharacter（或 BP_Monster）的 HitReactFeature 上把此字段指向该表
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact|Data")
	TObjectPtr<UDataTable> HitMontageTable;

	/**
	 * 受击 CD（秒）。
	 *   - 防止"被连续命中导致受击动画反复重启"看起来非常滑稽
	 *   - 对玩家可以设小一些（手感优先，0.05~0.1）
	 *   - 对小怪可以设大一些（避免被刮伤，0.15~0.3）
	 *   - 010 在这里同时维护"轻击/重击/击飞"分别独立的 CD，我们简化为单一 CD
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact|Cooldown", meta = (ClampMin = "0.0"))
	float HitReactCooldown = 0.1f;

	/**
	 * 受击替换表（Day 3 接入）。
	 *   key   = 入参的 ReactType
	 *   value = 实际播放时使用的 ReactType
	 *
	 * 示例用途：
	 *   - 弱点期间所有 LightHit 替换为 HeavyHit
	 *   - 中毒状态下所有受击替换为 Stun
	 *   - 蒙太奇替换不影响伤害结算（伤害已经在 ApplyDamage 里走过了）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact|Rule")
	TMap<EHitReactType, EHitReactType> HitReactReplaceMap;

	/** 是否在播放前转身朝向攻击者（受 FHitContext.bRotateToAttacker 控制）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact|Rule")
	bool bAllowRotateToAttacker = true;

	/** 调试：未找到匹配蒙太奇时是否在屏幕打 Warning（开发期推荐开）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact|Debug")
	bool bWarnOnMontageNotFound = true;

	// ==================== API ====================

	/**
	 * 由 HitReceiver 调用。请求一次受击动画响应。
	 * @return 是否成功开始播放
	 */
	UFUNCTION(BlueprintCallable, Category = "HitReact")
	bool RequestHitReact(const FHitContext& HitCtx);

	/** 主动停止当前受击蒙太奇（用于"受击被技能/复活打断"等场景）。 */
	UFUNCTION(BlueprintCallable, Category = "HitReact")
	void StopHitReact(float BlendOutTime = 0.1f);

	UFUNCTION(BlueprintPure, Category = "HitReact")
	bool IsInHitReact() const { return bIsInReact; }

	UFUNCTION(BlueprintPure, Category = "HitReact")
	bool IsInCooldown() const;

protected:
	virtual void BeginPlay() override;

private:
	/**
	 * 蒙太奇选择主入口。
	 * 顺序：
	 *   1. MontageOverrideRow 非空 → 直接按 RowName 取
	 *   2. ReactType + 计算后的 Direction → 表里找
	 *   3. 仅 ReactType 匹配（任意方向）兜底
	 */
	const FHitMontageRow* SelectMontageRow(const FHitContext& HitCtx, EHitReactType ResolvedType, EHitReactDirection ResolvedDir) const;

	/**
	 * 计算受击方向：把"攻击 → 受击者"水平向量投影到受击者本地坐标系。
	 *   - dot(Forward) > 0 → 来自正面（Front）
	 *   - dot(Forward) < 0 → 来自后方（Back）
	 *   - 取 |Forward| 与 |Right| 的较大者决定主方向
	 */
	EHitReactDirection ComputeHitDirection(const FHitContext& HitCtx) const;

	/** 应用受击替换规则。 */
	EHitReactType ResolveReactType(EHitReactType InType) const;

	/** 转向攻击者（仅 Yaw 旋转，瞬间生效）。 */
	void RotateOwnerToAttacker(AActor* Attacker);

	/** 设置 / 清除受击中的 Tag（Block.HitReact + State.Action.HitReact）。 */
	void EnterReactState();
	void ExitReactState();

	/** 蒙太奇结束回调。 */
	UFUNCTION()
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** 工具：拿 OwnerChar / AnimInstance。 */
	AActionCharacterBase* GetOwnerCharacter() const;
	UAnimInstance* GetOwnerAnimInstance() const;

private:
	/** 当前正在播的受击蒙太奇（用于校验 End 回调归属）。 */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	/** 上一次成功播放受击的世界时间（用于 CD 判断）。 */
	float LastReactTime = -1.0f;

	bool bIsInReact = false;
};
