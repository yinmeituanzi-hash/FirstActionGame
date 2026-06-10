#pragma once

#include "CoreMinimal.h"
#include "Combat/VFX/ActionVFXTypes.h"
#include "Components/ActorComponent.h"
#include "ActionVFXComponent.generated.h"

class UDataTable;

/**
 * Actor 侧特效播放门面。
 *
 * 负责从本 Actor 配置的 DataTable 中按 VFXId 找到特效行，并保存本 Actor
 * 发起过的特效句柄。真正的生成、注册和查询统一交给 UActionVFXSubsystem。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UActionVFXComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UActionVFXComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|VFX|Data")
	TObjectPtr<UDataTable> VFXDataTable = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Action|VFX")
	FActionVFXHandle PlayVFX(FName VFXId, const FActionVFXContext& Context);

	UFUNCTION(BlueprintCallable, Category = "Action|VFX")
	void StopVFX(FActionVFXHandle Handle, bool bImmediate = false);

	UFUNCTION(BlueprintCallable, Category = "Action|VFX")
	void StopVFXByGroup(FName GroupTag, bool bImmediate = false);

	/** 只停止 FollowSkillLifetime 或显式 bStopOnSkillEnd 的特效，不影响瞬时 / 固定时长特效。 */
	UFUNCTION(BlueprintCallable, Category = "Action|VFX")
	void StopSkillLifetimeVFX(FName SkillId);

	UFUNCTION(BlueprintPure, Category = "Action|VFX")
	int32 GetOwnedVFXCount() const { return OwnedHandles.Num(); }

private:
	/** 本 Actor 发起过的特效句柄。真实存活记录仍以 Subsystem 为准。 */
	UPROPERTY(Transient)
	TArray<FActionVFXHandle> OwnedHandles;

	const FActionVFXRow* FindVFXRow(FName VFXId) const;
	void RemoveOwnedHandle(FActionVFXHandle Handle);

	/** 移除 Subsystem 已经停止或自动清理掉的本地句柄。 */
	void SyncOwnedHandlesFromSubsystem();
};
