#pragma once

#include "CoreMinimal.h"
#include "Combat/ActionFeatures/MontageActionFeature.h"
#include "DodgeFeature.generated.h"

class UAnimMontage;

/**
 * UDodgeFeature
 *
 * 闪避动作。
 *
 * 职责：
 *   - 接收 4 个方向的闪避蒙太奇配置（Forward/Backward/Left/Right）。
 *   - 根据当前移动输入选择方向蒙太奇（无输入时回退到 Forward 或通用 Default）。
 *   - 维护闪避充能：MaxCharges、CurrentCharges、ChargeCooldown。
 *   - 监听 DodgeRecoveryStart Notify，进入恢复窗口时移除 Block.Move/Block.Attack。
 *
 * 与原 PlayerCharacter 实现的对应：
 *   - HasAvailableDodgeCharge / ConsumeDodgeCharge → 移到本 Feature 内。
 *   - SelectDodgeMontage / ResolveDodgeDirection → 移到本 Feature 内。
 *   - DodgeRecoveryStart Notify 处理 → 移到 OnNotify 里。
 */
UCLASS(Blueprintable, ClassGroup = (Action))
class ACTIONGAME_API UDodgeFeature : public UMontageActionFeature
{
	GENERATED_BODY()

public:
	UDodgeFeature();

	// ---------- 蒙太奇配置 ----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Animation")
	TObjectPtr<UAnimMontage> DodgeForwardMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Animation")
	TObjectPtr<UAnimMontage> DodgeBackwardMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Animation")
	TObjectPtr<UAnimMontage> DodgeLeftMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Animation")
	TObjectPtr<UAnimMontage> DodgeRightMontage;

	/** 没有方向输入或没配方向蒙太奇时的回退资源。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Animation")
	TObjectPtr<UAnimMontage> DodgeDefaultMontage;

	// ---------- 充能配置 ----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Charges", meta = (ClampMin = "1"))
	int32 MaxCharges = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge|Charges", meta = (ClampMin = "0.0"))
	float ChargeCooldownTime = 2.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dodge|Charges")
	int32 CurrentCharges = 2;

	UFUNCTION(BlueprintPure, Category = "Dodge|Charges")
	bool HasAvailableCharge() const { return CurrentCharges > 0; }

	UFUNCTION(BlueprintPure, Category = "Dodge|Charges")
	int32 GetCurrentCharges() const { return CurrentCharges; }

	// ---------- Lifecycle ----------

	virtual void Initialize(class AActionPlayerCharacter* InOwner) override;
	virtual bool CanExecute() const override;
	virtual void Execute() override;
	virtual void OnNotify(FName NotifyName, const FBranchingPointNotifyPayload& Payload) override;
	virtual void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted) override;

private:
	UAnimMontage* SelectMontageForCurrentInput() const;
	void ConsumeCharge();
	void StartChargeRestoreTimerIfNeeded();
	void RestoreOneCharge();

	FTimerHandle ChargeRestoreTimerHandle;
};
