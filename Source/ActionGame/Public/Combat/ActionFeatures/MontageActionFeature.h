#pragma once

#include "CoreMinimal.h"
#include "Combat/ActionFeatures/ActionFeatureBase.h"
#include "MontageActionFeature.generated.h"

class UAnimMontage;
class UAnimInstance;

/**
 * UMontageActionFeature
 *
 * 所有"由蒙太奇驱动"的动作 Feature 的中间基类，例如：闪避、普攻、技能。
 *
 * 主要职责：
 *   - 统一蒙太奇播放、停止、End 回调、Notify 回调的连接与解绑。
 *   - 为子类提供一个最简单的"播放某个蒙太奇"的入口 PlayMontageInternal。
 *   - 子类只需要决定"什么时候播什么蒙太奇"以及"收到 Notify 怎么处理"。
 *
 * 与 010 的 UMontageActionFeature 对比：
 *   - 010 用蒙太奇路径字符串 + LoadObject 动态加载，依赖角色目录约定。
 *     这里改为子类直接持有 UAnimMontage* 资源指针，配合 UPROPERTY 编辑器配置，更直观。
 *   - 010 用 UCharPlayMontageCallbackProxy 代理回调；这里用 AnimInstance 自带委托，更轻量。
 */
UCLASS(Abstract)
class ACTIONGAME_API UMontageActionFeature : public UActionFeatureBase
{
	GENERATED_BODY()

public:
	UMontageActionFeature();

	/** 蒙太奇被打断时使用的 BlendOut 时间。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MontageFeature", meta = (ClampMin = "0.0"))
	float DefaultMontageBlendOutTime = 0.1f;

	virtual void Stop(bool bInterrupted) override;

protected:
	/**
	 * 通用蒙太奇播放入口。
	 * 内部会：
	 *   1. 如果当前正在播某个蒙太奇且不是同一个，先 BlendOut。
	 *   2. 调用 AnimInstance->Montage_Play。
	 *   3. 绑定 OnMontageEnded 与 OnPlayMontageNotifyBegin。
	 *
	 * @param Montage    要播放的蒙太奇资源。
	 * @param PlayRate   播放速率，默认 1.0。
	 * @return 播放成功返回蒙太奇时长，失败返回 0。
	 */
	float PlayMontageInternal(UAnimMontage* Montage, float PlayRate = 1.0f);

	/** 主动停止当前蒙太奇（如果是 ActiveMontage）。 */
	void StopActiveMontage(float BlendOutTime = -1.0f);

	/** 当前由本 Feature 启动的蒙太奇（用于校验回调归属）。 */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

private:
	UFUNCTION()
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

	void ClearMontageDelegates(UAnimInstance* AnimInstance, UAnimMontage* MontageForEndDelegate = nullptr);
};
