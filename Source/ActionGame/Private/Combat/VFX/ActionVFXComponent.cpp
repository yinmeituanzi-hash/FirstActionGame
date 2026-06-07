#include "Combat/VFX/ActionVFXComponent.h"

#include "Combat/VFX/ActionVFXSubsystem.h"
#include "Engine/DataTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionVFXComponent, Log, All);

// 组件级 VFX 入口。业务代码只传 VFXId，由组件查表，再交给 World Subsystem 真正生成。
UActionVFXComponent::UActionVFXComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UActionVFXComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UActionVFXSubsystem* VFXSubsystem = UActionVFXSubsystem::Get(this))
	{
		for (const FActionVFXHandle& Handle : OwnedHandles)
		{
			VFXSubsystem->StopVFX(Handle, true);
		}
	}
	OwnedHandles.Reset();

	Super::EndPlay(EndPlayReason);
}

FActionVFXHandle UActionVFXComponent::PlayVFX(FName VFXId, const FActionVFXContext& Context)
{
	FActionVFXHandle InvalidHandle;
	const FActionVFXRow* Row = FindVFXRow(VFXId);
	if (Row == nullptr)
	{
		UE_LOG(LogActionVFXComponent, Verbose, TEXT("ActionVFXComponent[%s]: VFX row not found. VFXId=%s"), *GetNameSafe(GetOwner()), *VFXId.ToString());
		return InvalidHandle;
	}

	UActionVFXSubsystem* VFXSubsystem = UActionVFXSubsystem::Get(this);
	if (VFXSubsystem == nullptr)
	{
		return InvalidHandle;
	}

	FActionVFXPlayRequest Request;
	Request.VFXRow = *Row;
	if (Request.VFXRow.VFXId.IsNone())
	{
		Request.VFXRow.VFXId = VFXId;
	}
	Request.Context = Context;
	if (Request.Context.SourceActor == nullptr)
	{
		// 角色局部特效大多使用 Owner 作为来源；投射物或转发的受击特效可以由调用方覆盖 SourceActor。
		Request.Context.SourceActor = GetOwner();
	}

	const FActionVFXHandle Handle = VFXSubsystem->PlayVFX(Request);
	if (Handle.IsValid())
	{
		OwnedHandles.AddUnique(Handle);
	}
	return Handle;
}

void UActionVFXComponent::StopVFX(FActionVFXHandle Handle, bool bImmediate)
{
	if (UActionVFXSubsystem* VFXSubsystem = UActionVFXSubsystem::Get(this))
	{
		VFXSubsystem->StopVFX(Handle, bImmediate);
	}
	RemoveOwnedHandle(Handle);
}

void UActionVFXComponent::StopVFXByGroup(FName GroupTag, bool bImmediate)
{
	if (GroupTag.IsNone())
	{
		return;
	}

	if (UActionVFXSubsystem* VFXSubsystem = UActionVFXSubsystem::Get(this))
	{
		VFXSubsystem->StopVFXByOwner(GetOwner(), GroupTag, bImmediate);
	}

	SyncOwnedHandlesFromSubsystem();
}

void UActionVFXComponent::StopSkillLifetimeVFX(FName SkillId)
{
	if (UActionVFXSubsystem* VFXSubsystem = UActionVFXSubsystem::Get(this))
	{
		VFXSubsystem->StopVFXBySkill(GetOwner(), SkillId);
	}

	SyncOwnedHandlesFromSubsystem();
}

const FActionVFXRow* UActionVFXComponent::FindVFXRow(FName VFXId) const
{
	if (VFXDataTable == nullptr || VFXId.IsNone())
	{
		return nullptr;
	}

	if (VFXDataTable->GetRowStruct() != FActionVFXRow::StaticStruct())
	{
		UE_LOG(
			LogActionVFXComponent,
			Warning,
			TEXT("ActionVFXComponent[%s]: VFXDataTable row type mismatch. Expected FActionVFXRow."),
			*GetNameSafe(GetOwner()));
		return nullptr;
	}

	static const FString ContextString(TEXT("ActionVFXComponent.FindVFXRow"));
	return VFXDataTable->FindRow<FActionVFXRow>(VFXId, ContextString, false);
}

void UActionVFXComponent::RemoveOwnedHandle(FActionVFXHandle Handle)
{
	OwnedHandles.RemoveAll([Handle](const FActionVFXHandle& Existing)
	{
		return Existing == Handle;
	});
}

void UActionVFXComponent::SyncOwnedHandlesFromSubsystem()
{
	UActionVFXSubsystem* VFXSubsystem = UActionVFXSubsystem::Get(this);
	if (VFXSubsystem == nullptr)
	{
		OwnedHandles.Reset();
		return;
	}

	TArray<FActionVFXRecord> ActiveRecords;
	VFXSubsystem->GetActiveVFXRecords(ActiveRecords);
	// OwnedHandles 只是本地便捷缓存。按 Group / Skill 停止后，需要和 Subsystem 的权威记录对齐。
	OwnedHandles.RemoveAll([&ActiveRecords](const FActionVFXHandle& Existing)
	{
		return !ActiveRecords.ContainsByPredicate([Existing](const FActionVFXRecord& Record)
		{
			return Record.Handle == Existing;
		});
	});
}
