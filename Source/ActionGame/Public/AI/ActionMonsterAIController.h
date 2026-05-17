#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ActionMonsterAIController.generated.h"

class UBehaviorTree;
class UBlackboardComponent;
class UBehaviorTreeComponent;

/**
 * 怪物 AI 控制器（Sprint 4-A 起点）。
 *
 * 职责：
 *  1. Possess 怪物时自动启动 BehaviorTree。
 *  2. 在 BTAsset 上配置好 Blackboard，OnPossess 自动 UseBlackboard。
 *  3. 提供 ControlRotation 平滑插值（动作游戏怪转头不应瞬切，010 同款）。
 *
 * 设计取舍：
 *  - 不做 010 的 InfoMove / MonSteering / CrossLevel / NavAgentProps 切换等。
 *    单机小项目用 UE 原生 NavMesh + AIController.MoveTo 已经够。
 *  - 不在此处放任何"怪物业务逻辑"。所有业务（攻击、感知、状态机）都在 BTService /
 *    BTTask 和 Character 端。AIController 只做"启动 / 控制 / 销毁"三件事。
 *  - BTAsset 字段在编辑器里指向 BT_Monster_Basic（蓝图资源）。
 */
UCLASS()
class ACTIONGAME_API AActionMonsterAIController : public AAIController
{
	GENERATED_BODY()

public:
	AActionMonsterAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 关联的 BehaviorTree 资源。在 BP_ActionMonsterAIController 默认值里指定 BT_Monster_Basic。 */
	UPROPERTY(EditDefaultsOnly, Category = "Action|AI")
	TObjectPtr<UBehaviorTree> BTAsset = nullptr;

	/** 当前正在运行的 BTComponent（运行时由 RunBehaviorTree 创建并赋值给 BrainComponent）。 */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Action|AI")
	TObjectPtr<UBehaviorTreeComponent> BehaviorComp = nullptr;

	/** 当前 Blackboard 组件（来自基类 Blackboard，转过来方便外部读取）。 */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Action|AI")
	TObjectPtr<UBlackboardComponent> BlackboardComp = nullptr;

	/** 是否启用控制朝向插值。开启后 ControlRotation 不会瞬切，让怪物转头自然。 */
	UPROPERTY(EditDefaultsOnly, Category = "Action|AI|Rotation")
	bool bSmoothControlRotation = true;

	/**
	 * ControlRotation 插值速度（度/秒）。
	 * 对应"想看向新目标，但要花 N 秒看过去"的速度。
	 * 360 = 1 秒转一圈。值越大越接近瞬切。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Action|AI|Rotation", meta = (ClampMin = "0.0"))
	float SmoothControlRotationSpeed = 240.0f;

	/** 取被控制的角色（Cast 一次的便捷封装）。 */
	UFUNCTION(BlueprintCallable, Category = "Action|AI")
	class AActionMonsterCharacter* GetMonsterCharacter() const;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 覆写控制朝向更新：当 bSmoothControlRotation = true 时不直接 SetControlRotation，
	 * 而是按 SmoothControlRotationSpeed 插值。
	 *
	 * 注意：UE 的 AAIController::UpdateControlRotation 默认会把整段 SetFocus 计算好的目标
	 * 朝向直接赋值。这里我们改成插值，否则怪物视角抖动很明显。
	 */
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;

private:
	/** 平滑插值的目标朝向缓存（每帧 UpdateControlRotation 内更新）。 */
	FRotator DesiredControlRotation = FRotator::ZeroRotator;
};
