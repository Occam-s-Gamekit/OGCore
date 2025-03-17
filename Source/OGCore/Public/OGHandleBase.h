/// Copyright Occam's Gamekit contributors 2025

#pragma once

#include "CoreMinimal.h"
#include "OGHandleBase.generated.h"

typedef uint32 OGHandleIdType;

/**
 * Base class for Generic Locally Unique handles
 * To use, create a subclass of FOGHandleBase and implement a default constructor and constructor with a OGHandleIdType param.
 * EX:
 * FOGHandle() : FOGHandleBase() {}
 * FOGHandle(OGHandleIdType InHandle) : FOGHandleBase(InHandle) {}
 *
 * Each subclass will have a unique internal counter.
 *
 * Note: GenerateHandle must never be called within an inline function or from multiple DLLs/modules
 */
USTRUCT(BlueprintType)
struct OGCORE_API FOGHandleBase
{
	GENERATED_BODY()

public:

	FOGHandleBase() {}
	FOGHandleBase(OGHandleIdType InHandle) : Handle(InHandle) {}
	FOGHandleBase(const FOGHandleBase& Other) : Handle(Other.Handle) {}
	FOGHandleBase(const FOGHandleBase&& Other) noexcept : Handle(Other.Handle) {}

	template<typename T UE_REQUIRES(std::is_base_of_v<FOGHandleBase, T>)>
	FORCENOINLINE static T GenerateHandle()
	{
		static OGHandleIdType NextHandleID = 1;

		//Explicitly skip 0, as 0 is reserved for invalid handle.
		if (NextHandleID == 0) [[unlikely]]
		{
			++NextHandleID;
		}
		return T(NextHandleID++);
	}

	template<typename T UE_REQUIRES(std::is_base_of_v<FOGHandleBase, T>)>
	static const T& EmptyHandle()
	{
		static const T EmptyHandle;
		return EmptyHandle;
	}

	bool IsValid() const
	{
		return Handle != 0;
	}

	void operator=(const FOGHandleBase& Other)
	{
		Handle = Other.Handle;
	}

	void operator=(const FOGHandleBase&& Other)
	{
		Handle = Other.Handle;
	}

	bool operator==(const FOGHandleBase& Other) const
	{
		return Handle == Other.Handle
			&& StaticStruct() == Other.StaticStruct();
	}

	bool operator!=(const FOGHandleBase& Other) const
	{
		return !FOGHandleBase::operator==(Other);
	}

	friend OGHandleIdType GetTypeHash(const FOGHandleBase& InHandle)
	{
		return InHandle.Handle;
	}

	friend FArchive& operator <<(FArchive& Ar, FOGHandleBase& InHandle)
	{
		Ar << InHandle.Handle;
		return Ar;
	}

	FString ToString() const
	{
		return FString::Format(TEXT("{0}({1})"),{
			StaticStruct()->GetStructCPPName(),
			IsValid() ? FString::FromInt(Handle) : TEXT("INVALID")});
	}

private:
	OGHandleIdType Handle = 0;
};
