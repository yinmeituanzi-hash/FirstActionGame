#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HitFeedbackComponent.generated.h"

class AActor;
class ACharacter;
class UCameraShakeBase;
class UNiagaraSystem;
class USoundBase;

/**
 * UHitFeedbackComponent
 *
 * 命中反馈四件套，挂在 ACharacter（通常 PlayerCharacter）上：
 *   1. HitStop  ：临时把攻击者和被击者的 AnimInstance 播放速率降到 0，制造"卡帧"打击感
 *   2. CameraShake：触发玩家相机震屏
 *   3. Particle ：在命中位置生成 Niagara 粒子（如血花、闪光）
 *   4. Sound    ：在命中位置播放 3D 音效
 *
 * 设计理由：
 *   - 这些都是"事件驱动的视听效果"，集中在一个组件比散在各 Feature 里更方便调参
 *   - 字段全部 UPROPERTY 暴露，美术/策划可以在 BP 上直接调，不需要改 C++
 *   - 资源字段为空时降级为安全行为（不播放 / 用 DrawDebug 提示），代码不阻塞
 *
 * 与 010 HitLogicComponent 的对比：
 *   - 010 的 HitLogicComponent 同时管"受击表现"和"硬直/位移"，职责重
 *   - 本组件只管反馈，硬直/位移留给后续 HitReactFeature 实现
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UHitFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHitFeedbackComponent();

	// ==================== HitStop ====================

	/** HitStop 默认时长（秒）。0.05~0.08 是经验值，超过 0.1 会觉得"卡顿"。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitFeedback|HitStop", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float HitStopDuration = 0.06f;

	/** HitStop 时的播放速率，0 = 完全暂停，0.05 = 慢动作。建议 0。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitFeedback|HitStop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HitStopAnimRate = 0.0f;

	/** 是否对攻击者也应用 HitStop。默认 true（双方都卡帧）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitFeedback|HitStop")
	bool bApplyHitStopToAttacker = true;

	// ==================== CameraShake ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitFeedback|CameraShake")
	TSubclassOf<UCameraShakeBase> CameraShakeClass;

	/** CameraShake 的强度倍率。默认 1.0。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitFeedback|CameraShake", meta = (ClampMin = "0.0"))
	float CameraShakeScale = 1.0f;

	// ==================== Particle ====================

	/** 命中粒子（Niagara System）。建议指定一个血花/闪光特效。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitFeedback|Particle")
	TObjectPtr<UNiagaraSystem> HitParticle;

	/**
	 * 没有指定 HitParticle 时是否绘制调试球体作为占位提示。
	 * 开发阶段开 true，方便看到命中位置；正式版关掉。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitFeedback|Particle")
	bool bDrawDebugIfNoParticle = true;

	// ==================== Sound ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitFeedback|Sound")
	TObjectPtr<USoundBase> HitSound;

	// ==================== API ====================

	/**
	 * 触发一次完整的命中反馈。
	 * @param Victim          被击中的角色（用于 HitStop / 粒子位置）
	 * @param HitLocation     命中世界位置（建议用胶囊体中点或剑刃位置）
	 * @param Attacker        攻击者（用于 HitStop；可空）
	 * @param DamageScale     伤害规模 [0, 1]，会按比例放大 CameraShake 和 HitStop 时长。1.0 = 满强度
	 */
	UFUNCTION(BlueprintCallable, Category = "HitFeedback")
	void TriggerHitFeedback(ACharacter* Victim, const FVector& HitLocation, ACharacter* Attacker = nullptr, float DamageScale = 1.0f);

private:
	/** 实际执行 HitStop：把 GlobalAnimRateScale 设为 HitStopAnimRate，Timer 到时还原。 */
	void ApplyHitStop(ACharacter* Char, float Duration);

	/** Timer 到时还原某个 Mesh 的 GlobalAnimRateScale。 */
	UFUNCTION()
	void RestoreAnimRate(TWeakObjectPtr<USkeletalMeshComponent> MeshRef);

	void PlayCameraShakeFor(ACharacter* Attacker, float Scale);
	void SpawnHitParticleAt(const FVector& Location);
	void PlayHitSoundAt(const FVector& Location);
};
