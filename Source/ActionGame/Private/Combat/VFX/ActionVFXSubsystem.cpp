#include "Combat/VFX/ActionVFXSubsystem.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionVFX, Log, All);

static TAutoConsoleVariable<int32> CVarActionVFXDebug(
	TEXT("VFX.Debug"),
	0,
	TEXT("Enable Action VFX debug logs. 0=off, 1=on."),
	ECVF_Cheat);

// UWorldSubsystem 获取入口。封装成静态函数后，调用点不用反复写空指针和 World 检查。
UActionVFXSubsystem* UActionVFXSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject != nullptr ? WorldContextObject->GetWorld() : nullptr;
	return World != nullptr ? World->GetSubsystem<UActionVFXSubsystem>() : nullptr;
}

FActionVFXHandle UActionVFXSubsystem::PlayVFX(const FActionVFXPlayRequest& Request)
{
	FActionVFXHandle InvalidHandle;
	if (Request.VFXRow.NiagaraSystem == nullptr)
	{
		UE_LOG(LogActionVFX, Verbose, TEXT("ActionVFX: PlayVFX skipped. VFXId=%s has no NiagaraSystem."), *Request.VFXRow.VFXId.ToString());
		return InvalidHandle;
	}

	USceneComponent* AttachComponent = nullptr;
	const FTransform SpawnTransform = ResolveSpawnTransform(Request, AttachComponent);
	const bool bAutoDestroy = Request.VFXRow.LifetimePolicy == EActionVFXLifetimePolicy::AutoDestroy;

	// 挂接特效跟随 Socket / Component；非挂接特效使用解析出的世界 Transform。
	// Transform 可能来自 Socket、命中点或显式传入的世界坐标。
	UNiagaraComponent* NiagaraComponent = nullptr;
	if (Request.VFXRow.bAttachToTarget && AttachComponent != nullptr)
	{
		NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			Request.VFXRow.NiagaraSystem,
			AttachComponent,
			Request.VFXRow.SocketName,
			Request.VFXRow.OffsetTransform.GetLocation(),
			Request.VFXRow.OffsetTransform.Rotator(),
			EAttachLocation::KeepRelativeOffset,
			bAutoDestroy);
	}
	else
	{
		NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			Request.VFXRow.NiagaraSystem,
			SpawnTransform.GetLocation(),
			SpawnTransform.Rotator(),
			Request.VFXRow.Scale,
			bAutoDestroy);
	}

	if (NiagaraComponent == nullptr)
	{
		return InvalidHandle;
	}

	NiagaraComponent->SetWorldScale3D(Request.VFXRow.Scale);
	const FActionVFXHandle Handle = RegisterRecord(Request, NiagaraComponent);
	if (!Handle.IsValid())
	{
		return InvalidHandle;
	}

	if (Request.VFXRow.LifetimePolicy == EActionVFXLifetimePolicy::FixedDuration && Request.VFXRow.Duration > 0.0f)
	{
		if (FActionVFXRecord* Record = ActiveRecords.Find(Handle.Id))
		{
			if (UWorld* World = GetWorld())
			{
				// 使用 WeakLambda，而不是直接绑定 StopVFX。Handle 按值捕获，
				// World 销毁时 Subsystem 消失也不会留下危险回调。
				FTimerDelegate TimerDelegate = FTimerDelegate::CreateWeakLambda(this, [this, Handle]()
				{
					StopVFX(Handle, false);
				});
				World->GetTimerManager().SetTimer(Record->DurationTimerHandle, TimerDelegate, Request.VFXRow.Duration, false);
			}
		}
	}

	if (CVarActionVFXDebug.GetValueOnGameThread() != 0)
	{
		UE_LOG(
			LogActionVFX,
			Log,
			TEXT("ActionVFX: Play Handle=%d VFXId=%s Owner=%s Group=%s"),
			Handle.Id,
			*Request.VFXRow.VFXId.ToString(),
			*GetNameSafe(Request.Context.SourceActor),
			*Request.VFXRow.GroupTag.ToString());
	}

	return Handle;
}

void UActionVFXSubsystem::StopVFX(const FActionVFXHandle& Handle, bool bImmediate)
{
	if (!Handle.IsValid())
	{
		return;
	}

	FActionVFXRecord* Record = ActiveRecords.Find(Handle.Id);
	if (Record == nullptr)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Record->DurationTimerHandle);
	}

	if (UNiagaraComponent* NiagaraComponent = Record->NiagaraComponent.Get())
	{
		// Deactivate 允许 Niagara 自然收尾；DestroyComponent 用于 Owner EndPlay
		// 或 StopAll 这类需要强制清理的场景。
		if (bImmediate)
		{
			NiagaraComponent->DestroyComponent();
		}
		else
		{
			NiagaraComponent->Deactivate();
		}
	}

	if (CVarActionVFXDebug.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogActionVFX, Log, TEXT("ActionVFX: Stop Handle=%d VFXId=%s"), Handle.Id, *Record->VFXId.ToString());
	}

	UnregisterRecord(Handle.Id);
}

void UActionVFXSubsystem::StopVFXByOwner(AActor* Owner, FName GroupTag, bool bImmediate)
{
	if (Owner == nullptr)
	{
		return;
	}

	TArray<FActionVFXHandle> ToStop;
	for (const TPair<int32, FActionVFXRecord>& Pair : ActiveRecords)
	{
		const FActionVFXRecord& Record = Pair.Value;
		if (Record.OwnerActor.Get() == Owner && (GroupTag.IsNone() || Record.GroupTag == GroupTag))
		{
			ToStop.Add(Record.Handle);
		}
	}

	for (const FActionVFXHandle& Handle : ToStop)
	{
		StopVFX(Handle, bImmediate);
	}
}

void UActionVFXSubsystem::StopVFXBySkill(AActor* Owner, FName SkillId)
{
	if (Owner == nullptr || SkillId.IsNone())
	{
		return;
	}

	TArray<FActionVFXHandle> ToStop;
	for (const TPair<int32, FActionVFXRecord>& Pair : ActiveRecords)
	{
		const FActionVFXRecord& Record = Pair.Value;
		const bool bShouldStopWithSkill =
			Record.LifetimePolicy == EActionVFXLifetimePolicy::FollowSkillLifetime ||
			Record.bStopOnSkillEnd;
		if (Record.OwnerActor.Get() == Owner && Record.SkillId == SkillId && bShouldStopWithSkill)
		{
			ToStop.Add(Record.Handle);
		}
	}

	for (const FActionVFXHandle& Handle : ToStop)
	{
		StopVFX(Handle);
	}
}

void UActionVFXSubsystem::StopAllVFX(bool bImmediate)
{
	TArray<FActionVFXHandle> ToStop;
	for (const TPair<int32, FActionVFXRecord>& Pair : ActiveRecords)
	{
		ToStop.Add(Pair.Value.Handle);
	}

	for (const FActionVFXHandle& Handle : ToStop)
	{
		StopVFX(Handle, bImmediate);
	}
}

int32 UActionVFXSubsystem::GetActiveVFXCount() const
{
	PurgeInvalidRecords();
	return ActiveRecords.Num();
}

void UActionVFXSubsystem::GetActiveVFXRecords(TArray<FActionVFXRecord>& OutRecords) const
{
	PurgeInvalidRecords();
	OutRecords.Reset();
	for (const TPair<int32, FActionVFXRecord>& Pair : ActiveRecords)
	{
		OutRecords.Add(Pair.Value);
	}
}

FActionVFXHandle UActionVFXSubsystem::RegisterRecord(const FActionVFXPlayRequest& Request, UNiagaraComponent* NiagaraComponent)
{
	FActionVFXHandle Handle;
	Handle.Id = NextHandleId++;

	FActionVFXRecord Record;
	Record.Handle = Handle;
	Record.VFXId = Request.VFXRow.VFXId;
	Record.SkillId = Request.Context.SkillId;
	Record.GroupTag = Request.VFXRow.GroupTag;
	Record.OwnerActor = Request.Context.SourceActor;
	Record.NiagaraComponent = NiagaraComponent;
	Record.LifetimePolicy = Request.VFXRow.LifetimePolicy;
	Record.bStopOnSkillEnd = Request.VFXRow.bStopOnSkillEnd;
	Record.Duration = Request.VFXRow.Duration;
	Record.StartTime = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f;
	ActiveRecords.Add(Handle.Id, Record);

	return Handle;
}

void UActionVFXSubsystem::UnregisterRecord(int32 HandleId)
{
	ActiveRecords.Remove(HandleId);
}

void UActionVFXSubsystem::PurgeInvalidRecords() const
{
	// 查询接口对外保持 const，但 Niagara 自动销毁后，注册表仍然需要惰性清理失效记录。
	TMap<int32, FActionVFXRecord>& MutableRecords = const_cast<TMap<int32, FActionVFXRecord>&>(ActiveRecords);
	for (auto It = MutableRecords.CreateIterator(); It; ++It)
	{
		if (!It->Value.NiagaraComponent.IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

FTransform UActionVFXSubsystem::ResolveSpawnTransform(const FActionVFXPlayRequest& Request, USceneComponent*& OutAttachComponent) const
{
	OutAttachComponent = nullptr;
	const FActionVFXRow& Row = Request.VFXRow;
	const FActionVFXContext& Context = Request.Context;

	AActor* SpaceActor = nullptr;
	FVector Location = Context.WorldLocation;
	FRotator Rotation = Context.WorldRotation;

	switch (Row.SpawnSpace)
	{
	case EActionVFXSpawnSpace::TargetSocket:
		SpaceActor = Context.TargetActor;
		break;
	case EActionVFXSpawnSpace::SkillCreature:
		SpaceActor = Context.SkillCreature;
		break;
	case EActionVFXSpawnSpace::HitLocation:
		Location = Context.HitLocation;
		break;
	case EActionVFXSpawnSpace::WorldLocation:
		break;
	case EActionVFXSpawnSpace::SourceSocket:
	default:
		SpaceActor = Context.SourceActor;
		break;
	}

	if (SpaceActor != nullptr)
	{
		OutAttachComponent = ResolveSceneComponent(SpaceActor, Row.SocketName);
		if (OutAttachComponent != nullptr)
		{
			// Offset 按所选 Socket / Component 的局部空间配置。
			const FTransform BaseTransform = !Row.SocketName.IsNone()
				? OutAttachComponent->GetSocketTransform(Row.SocketName, RTS_World)
				: OutAttachComponent->GetComponentTransform();
			return Row.OffsetTransform * BaseTransform;
		}

		return Row.OffsetTransform * SpaceActor->GetActorTransform();
	}

	return Row.OffsetTransform * FTransform(Rotation, Location);
}

USceneComponent* UActionVFXSubsystem::ResolveSceneComponent(AActor* Actor, FName SocketName) const
{
	if (Actor == nullptr)
	{
		return nullptr;
	}

	if (USkeletalMeshComponent* Mesh = Actor->FindComponentByClass<USkeletalMeshComponent>())
	{
		if (SocketName.IsNone() || Mesh->DoesSocketExist(SocketName))
		{
			return Mesh;
		}
	}

	return Actor->GetRootComponent();
}
