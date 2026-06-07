#pragma once

#include "CoreMinimal.h"
#include "Combat/VFX/ActionVFXTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ActionVFXSubsystem.generated.h"

class USceneComponent;

/**
 * World 级特效注册与播放服务。
 *
 * 技能效果、受击效果、技能生成物都不直接 Spawn Niagara，而是统一走这里。
 * 后续对象池、屏幕外裁剪、重要度降级、Debug 查询也都可以集中加在这个 Subsystem。
 */
UCLASS()
class ACTIONGAME_API UActionVFXSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UActionVFXSubsystem* Get(const UObject* WorldContextObject);

	FActionVFXHandle PlayVFX(const FActionVFXPlayRequest& Request);
	void StopVFX(const FActionVFXHandle& Handle, bool bImmediate = false);
	void StopVFXByOwner(AActor* Owner, FName GroupTag = NAME_None, bool bImmediate = false);
	void StopVFXBySkill(AActor* Owner, FName SkillId);
	void StopAllVFX(bool bImmediate = true);

	UFUNCTION(BlueprintPure, Category = "Action|VFX")
	int32 GetActiveVFXCount() const;

	void GetActiveVFXRecords(TArray<FActionVFXRecord>& OutRecords) const;

private:
	int32 NextHandleId = 1;

	/** 权威存活特效注册表。各 Actor Component 只保存本地便捷句柄。 */
	TMap<int32, FActionVFXRecord> ActiveRecords;

	FActionVFXHandle RegisterRecord(const FActionVFXPlayRequest& Request, class UNiagaraComponent* NiagaraComponent);
	void UnregisterRecord(int32 HandleId);
	void PurgeInvalidRecords() const;

	FTransform ResolveSpawnTransform(const FActionVFXPlayRequest& Request, USceneComponent*& OutAttachComponent) const;
	USceneComponent* ResolveSceneComponent(AActor* Actor, FName SocketName) const;
};
