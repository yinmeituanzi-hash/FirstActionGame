#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/HitReact/HitReactTypes.h"
#include "HitReceiverComponent.generated.h"

class UHitFeedbackComponent;
class UHitPhysicsComponent;
class UHitReactFeature;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHitReceivedSignature, const FHitContext&, HitContext);

/**
 * 受击体系的统一入口和调度器。
 *
 * 三层职责：
 * - HitFeedback：纯表现层，HitStop / 震屏 / 粒子 / 音效，挂在攻击者侧。
 * - HitReact：动画反应层，根据 ReactType + Direction 选择受击 Montage，挂在受击者侧。
 * - HitPhysics：物理位移层，负责击飞冲量 / Ragdoll / 起身，挂在受击者侧。
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UHitReceiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHitReceiverComponent();

	/** 处理一次受击事件。攻击者、Buff、环境伤害都通过这里进入受击表现链。 */
	UFUNCTION(BlueprintCallable, Category = "HitReceiver")
	void ReceiveHit(const FHitContext& HitCtx);

	/** 受击事件广播。AI、UI、成就系统可以监听这里，不污染受击调度本身。 */
	UPROPERTY(BlueprintAssignable, Category = "HitReceiver")
	FOnHitReceivedSignature OnHitReceived;

	/**
	 * 是否暂停所有受击响应。true 时 ReceiveHit 直接返回。
	 * 注意：这和霸体不同。霸体只屏蔽 React / Physics，伤害和 Feedback 仍生效。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReceiver")
	bool bSuppressAllHits = false;

protected:
	virtual void BeginPlay() override;

private:
	/** 第一层：表现反馈。资源在攻击者的 HitFeedbackComponent 上。 */
	void DispatchFeedback(const FHitContext& HitCtx);

	/** 第二层：受击动画。资源在受击者自己的 HitReactComponent 上。 */
	void DispatchReact(const FHitContext& HitCtx);

	/** 第三层：物理冲量 / Ragdoll。资源在受击者自己的 HitPhysicsComponent 上。 */
	void DispatchPhysics(const FHitContext& HitCtx);

	/** 受击者是否带霸体 Tag。霸体期间跳过 React / Physics，但保留 Feedback / Damage。 */
	bool IsOwnerInSuperArmor() const;

	/** 受击者是否处于 Ragdoll / 起身强控制状态。此期间跳过 React / Physics 重复施加。 */
	bool IsOwnerInRagdoll() const;

	/** 受击者是否已经死亡。死亡后大多数受击响应应被截断。 */
	bool IsOwnerDead() const;
};
