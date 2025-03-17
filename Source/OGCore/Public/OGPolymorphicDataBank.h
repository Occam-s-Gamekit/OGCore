/// Copyright Occam's Gamekit contributors 2025

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OGPolymorphicDataBank.generated.h"

struct OGCORE_API FOGPolymorphicStructCache
{
	void InitTypeCache(const UScriptStruct* PolymorphicGroupType)
	{
		if (!CachedStructTypes.IsEmpty()) [[likely]]
			return;
		
		// Find all script structs of this type and add them to the list
		// (not sure of a better way to do this but it should only happen once at startup)
		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			if (It->IsChildOf(PolymorphicGroupType))
			{
				CachedStructTypes.Add(TWeakObjectPtr<UScriptStruct>(*It));
			}
		}
	
		CachedStructTypes.Sort([](const TWeakObjectPtr<UScriptStruct>& A, const TWeakObjectPtr<UScriptStruct>& B) { return A->GetName().ToLower() > B->GetName().ToLower(); });
	}
	
	uint16 GetIndexForType(const UScriptStruct* Type)
	{
		const int Index = Algo::BinarySearchBy(CachedStructTypes, Type->GetName().ToLower(),
			[](const TWeakObjectPtr<UScriptStruct>& A){return A->GetName().ToLower();},
			[](const FString& Rhs, const FString& Lhs){return Rhs > Lhs;});
		ensureAlways(Index != INDEX_NONE);
		return Index;
	}
	
	UScriptStruct* GetTypeForIndex(const uint16& Index)
	{
		ensureAlways(CachedStructTypes.Num() > Index);
		return CachedStructTypes[Index].Get();
	}

private:
	TArray<TWeakObjectPtr<UScriptStruct>> CachedStructTypes;
};

/** Custom INetDeltaBaseState used by DataBank Serialization */
class FOGPolymorphicDataBankDeltaState : public INetDeltaBaseState
{
public:

	FOGPolymorphicDataBankDeltaState()
	: ContainerReplicationKey(INDEX_NONE)
	{}

	virtual bool IsStateEqual(INetDeltaBaseState* OtherState) override
	{
		FOGPolymorphicDataBankDeltaState * Other = static_cast<FOGPolymorphicDataBankDeltaState*>(OtherState);
		for (auto It = IDToRepKeyMap.CreateIterator(); It; ++It)
		{
			const auto Ptr = Other->IDToRepKeyMap.Find(It.Key());
			if (!Ptr || *Ptr != It.Value())
			{
				return false;
			}
		}
		return true;
	}

	virtual void CountBytes(FArchive& Ar) const override
	{
		IDToRepKeyMap.CountBytes(Ar);
	}

	/** Maps an element's StructId to ReplicationKey. */
	TMap<uint16, uint16> IDToRepKeyMap;

	uint16 ContainerReplicationKey;
};

/** Struct for holding guid references */
struct FOGPolymorphicDataBankSerializerGuidReferences
{
	/** List of guids that were unmapped so we can quickly check */
	TSet<FNetworkGUID> UnmappedGUIDs;

	/** List of guids that were mapped so we can move them to unmapped when necessary (i.e. actor channel closes) */
	TSet<FNetworkGUID> MappedDynamicGUIDs;

	/** Buffer of data to re-serialize when the guids are mapped */
	TArray<uint8> Buffer;

	/** Number of bits in the buffer */
	int32 NumBufferBits;
};

/**
 * Base type for structs that will be put into polymorphic data structures
 * Create a new struct that goes into the data structure
 */
USTRUCT(NotBlueprintType)
struct OGCORE_API FOGPolymorphicStructBase
{
	friend struct FOGPolymorphicDataBankBase;
	GENERATED_BODY()

	void SetReplicationKey(const uint16 NewKey)
	{
		ReplicationKey = NewKey;
	}
	
protected:
	UPROPERTY(NotReplicated)
	uint16 ReplicationKey = 0;
};

/**
 * Base type for a collection of polymorphic structs stored in a map using the struct type itself as a key
 * This is intended primarily as a way to give projects an easy method to add a layer of game specific parameters
 * to the internal structs created for general purpose plugins.
 * For example, a TriggerContext struct containing information about a gameplay event could be provided different structs
 * based on the type of event.
 * The tradeoff for being able to access the data by struct type is that there can only ever be one instance
 * of each struct can be in the collection.
 *
 * Use:
 * Create a new struct, e.g. MyDataBank, that extends this one, and one that extends FOGPolymorphicStructBase, e.g. MyDataStructBase
 * Create additional structs extending MyDataStructBase - these are the specific data structures MyDataBank will be able to hold
 * The structs that derive from MyDataStructBase must EACH use the BlueprintType USTRUCT trait. If they don't each have it, they won't
 * have Make and Break blueprint nodes, which are necessary for using a DataBank in blueprint.
 *
 * Your implementation of MyDataBank must implement these two methods
 * GetInnerStruct must return the UScriptStruct of the root type of the structs this will hold.
 * GetStructCache must return a single FOGPolymorphicStructCache that is maintained by the module for this class.
 *
 * If MyDataBank is going to replicate, you must apply the type trait WithNetSerializer or WithNetDeltaSerializer (Or both)
 * The actual implementations for delta and non-delta serialization are already done, you only need the trait to inform the engine.
 * If the data bank is going to be replicated via RPC, you must use WithNetSerializer - RPCs will never use delta serialization
 * If the data bank is going to replicate by value, either trait will work but WithNetDeltaSerializer is generally preferred
 * it reduces bandwidth and has better handling for object references that cannot be resolved by the client at the time of replication.
 *
 * You may apply both type traits, it will just trigger an engine warning if you do so. As far as I can tell it will correctly
 * use delta serialization for value replication and non-delta serialization RPCs without any issues, but the warning itself can interfere
 * with automated tests.
 *
 * Iris replication is not yet supported
 */
USTRUCT(BlueprintType)
struct OGCORE_API FOGPolymorphicDataBankBase
{
	GENERATED_BODY()

	friend class UOGPolymorphicDataFunctionLibrary;
	
	FOGPolymorphicDataBankBase() {}
	virtual ~FOGPolymorphicDataBankBase() {}

	template <typename Derived UE_REQUIRES(std::is_base_of_v<FOGPolymorphicStructBase, Derived>)>
	bool Contains() const
	{
		const uint16 Key = GetKey(Derived::StaticStruct());
		return DataMap.Contains(Key);
	}

	template <typename Derived UE_REQUIRES(std::is_base_of_v<FOGPolymorphicStructBase, Derived>)>
	Derived& AddUnique()
	{
		return static_cast<Derived&>(AddUnique_Internal(GetKey(Derived::StaticStruct()),Derived::StaticStruct()));
	}

	template <typename Derived UE_REQUIRES(std::is_base_of_v<FOGPolymorphicStructBase, Derived>)>
	void Remove()
	{
		const uint16 Key = GetKey(Derived::StaticStruct());
		Remove_Internal(Key, Derived::StaticStruct());
	}
	
	template <typename Derived UE_REQUIRES(std::is_base_of_v<FOGPolymorphicStructBase, Derived>)>
	void SetByCopy(const Derived& Source)
	{
		const uint16 Key = GetKey(Derived::StaticStruct());
		if (Derived* Existing = static_cast<Derived*>(Get_Internal(Key)))
		{
			*Existing = Source;
			//Override replication key in source
			MarkDirty(*Existing);
			return;
		}
		TSharedRef<Derived> NewEntry = MakeShared<Derived>(Source);
		MarkDirty(NewEntry.Get());
		DataMap.Add(Key, NewEntry);
#if WITH_EDITOR
		AvailableDataTypes.Add(Derived::StaticStruct()->GetStructCPPName());
#endif
	}

	template <typename Derived UE_REQUIRES(std::is_base_of_v<FOGPolymorphicStructBase, Derived>)>
	Derived& GetSafe()
	{
		const uint16 Key = GetKey(Derived::StaticStruct());
		if (Derived* Existing = static_cast<Derived*>(Get_Internal(Key)))
		{
			return *Existing;
		}
		return static_cast<Derived&>(AddUnique_Internal(Key, Derived::StaticStruct()));
	}
	
	template <typename Derived UE_REQUIRES(std::is_base_of_v<FOGPolymorphicStructBase, Derived>)>
	const Derived& GetConstChecked() const
	{
		const uint16 Key = GetKey(Derived::StaticStruct());
		const Derived* Existing = static_cast<const Derived*>(GetConst_Internal(Key));
		if (!ensureAlwaysMsgf(Existing, TEXT("Tried getting a ref for a type that's not in the bank"))) [[unlikely]]
			return static_cast<Derived&>(const_cast<FOGPolymorphicDataBankBase*>(this)->AddUnique_Internal(Key, Derived::StaticStruct()));
		return *Existing;
	}

	template <typename Derived UE_REQUIRES(std::is_base_of_v<FOGPolymorphicStructBase, Derived>)>
	Derived& GetChecked()
	{
		const uint16 Key = GetKey(Derived::StaticStruct());
		Derived* Existing = static_cast<Derived*>(Get_Internal(Key));
		if (!ensureAlwaysMsgf(Existing, TEXT("Tried getting a ref for a type that's not in the bank"))) [[unlikely]]
			return static_cast<Derived&>(AddUnique_Internal(Key, Derived::StaticStruct()));
		Derived& ExistingRef = *Existing;
		return ExistingRef;
	}

	void Empty();
	
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool&bOutSuccess);
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams);

private:

	virtual UScriptStruct* GetInnerStruct() const PURE_VIRTUAL(FOGPolymorphicDataBankBase::GetInnerStruct, return nullptr;);
	// Get the specific cache that translates a UScriptStruct to index and back.
	// default implementation gives a cache that covers every struct type that inherits from FOGPolymorphicStructBase.
	// Provide your own cache to improve lookup performance.
	virtual FOGPolymorphicStructCache* GetStructCache() const;
	
	FORCEINLINE uint16 GetKey(const UScriptStruct* ScriptStruct) const
	{
		if (!ensureAlwaysMsgf(ScriptStruct->IsChildOf(GetInnerStruct()), TEXT("Derived type must inherit from InnerStruct"))) [[unlikely]]
			return 0;
		return GetStructCache()->GetIndexForType(ScriptStruct);
	}

	FORCEINLINE void MarkDirty(FOGPolymorphicStructBase& Entry)
	{
		Entry.SetReplicationKey(++LastReplicationKey);
	}
	
	FOGPolymorphicStructBase* Get_Internal(const uint16& Key);

	const FOGPolymorphicStructBase* GetConst_Internal(const uint16& Key) const;
	
	FOGPolymorphicStructBase& AddUnique_Internal(const uint16& Key, const UScriptStruct* ScriptStruct);

	void Remove_Internal(const uint16& Key, const UScriptStruct* ScriptStruct = nullptr);
	
	TMap<uint16, TSharedRef<FOGPolymorphicStructBase>> DataMap;

	/** List of items that need to be re-serialized when the referenced objects are mapped */
	TMap<uint16, FOGPolymorphicDataBankSerializerGuidReferences> GuidReferencesMap;
	
	UPROPERTY()
	uint16 LastReplicationKey = 0;

#if WITH_EDITORONLY_DATA
	//This set is maintained in editor to make it easy to tell what structs are currently in the data bank
	UPROPERTY(Transient)
	TSet<FString> AvailableDataTypes;
#endif
};