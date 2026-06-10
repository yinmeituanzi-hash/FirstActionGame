#pragma once

#include "CoreMinimal.h"
#include "AI/Significance/AISignificanceTypes.h"
#include "Engine/DataTable.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "ActionVFXTypes.generated.h"

/**
 * Action VFX 的配置数据与运行时记录。
 *
 * 业务代码不应该到处直接 Spawn Niagara，而是统一通过 UActionVFXSubsystem。
 * 这样后续对象池、屏幕外裁剪、预算降级和 Debug 查询都能收敛到一个入口。
 */

UENUM(BlueprintType)
enum class EActionVFXSpawnSpace : uint8
{
	SourceSocket UMETA(DisplayName = "Source Socket"),
	TargetSocket UMETA(DisplayName = "Target Socket"),
	HitLocation UMETA(DisplayName = "Hit Location"),
	WorldLocation UMETA(DisplayName = "World Location"),
	SkillCreature UMETA(DisplayName = "Skill Creature")
};

UENUM(BlueprintType)
enum class EActionVFXLifetimePolicy : uint8
{
	AutoDestroy UMETA(DisplayName = "Auto Destroy"),
	FixedDuration UMETA(DisplayName = "Fixed Duration"),
	PersistentUntilStopped UMETA(DisplayName = "Persistent Until Stopped"),
	FollowSkillLifetime UMETA(DisplayName = "Follow Skill Lifetime")
};

USTRUCT(BlueprintType)
struct ACTIONGAME_API FActionVFXHandle
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|VFX")
	int32 Id = INDEX_NONE;

	bool IsValid() const { return Id != INDEX_NONE; }
	void Reset() { Id = INDEX_NONE; }

	friend bool operator==(const FActionVFXHandle& A, const FActionVFXHandle& B)
	{
		return A.Id == B.Id;
	}
};

/** 设计侧配置的一条特效行，也是 DT_ActionVFX 的单个效果契约。 */
USTRUCT(BlueprintType)
struct ACTIONGAME_API FActionVFXRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|VFX")
	FName VFXId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|VFX")
	TObjectPtr<UNiagaraSystem> NiagaraSystem = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|VFX")
	EActionVFXSpawnSpace SpawnSpace = EActionVFXSpawnSpace::SourceSocket;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|VFX")
	FName SocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|VFX")
	FTransform OffsetTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|VFX")
	bool bAttachToTarget = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|VFX")
	EActionVFXLifetimePolicy LifetimePolicy = EActionVFXLifetimePolicy::AutoDestroy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|VFX", meta = (ClampMin = "0.0"))
	float Duration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|VFX")
	FVector Scale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|VFX")
	FName GroupTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|VFX")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|VFX")
	EAISignificanceLevel MinSignificanceLevel = EAISignificanceLevel::Dormant;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|VFX")
	bool bAllowOffscreen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|VFX")
	bool bStopOnSkillEnd = false;
};

/** 播放特效时由技能、效果或受击逻辑提供的运行时上下文。 */
USTRUCT(BlueprintType)
struct ACTIONGAME_API FActionVFXContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Action|VFX")
	TObjectPtr<AActor> SourceActor = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Action|VFX")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Action|VFX")
	TObjectPtr<AActor> SkillCreature = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Action|VFX")
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Action|VFX")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Action|VFX")
	FRotator WorldRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, Category = "Action|VFX")
	FName SkillId = NAME_None;
};

/** UActionVFXSubsystem 消费的完整播放请求。 */
USTRUCT(BlueprintType)
struct ACTIONGAME_API FActionVFXPlayRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Action|VFX")
	FActionVFXRow VFXRow;

	UPROPERTY(BlueprintReadWrite, Category = "Action|VFX")
	FActionVFXContext Context;
};

/** 已注册的存活特效实例记录，用于停止、查询和 Debug。 */
USTRUCT(BlueprintType)
struct ACTIONGAME_API FActionVFXRecord
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|VFX")
	FActionVFXHandle Handle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|VFX")
	FName VFXId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|VFX")
	FName SkillId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|VFX")
	FName GroupTag = NAME_None;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> OwnerActor;

	UPROPERTY(Transient)
	TWeakObjectPtr<UNiagaraComponent> NiagaraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|VFX")
	EActionVFXLifetimePolicy LifetimePolicy = EActionVFXLifetimePolicy::AutoDestroy;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|VFX")
	bool bStopOnSkillEnd = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|VFX")
	float StartTime = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|VFX")
	float Duration = 0.0f;

	FTimerHandle DurationTimerHandle;
};
