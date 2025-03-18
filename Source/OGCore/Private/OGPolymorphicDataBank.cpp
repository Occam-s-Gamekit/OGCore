/// Copyright Occam's Gamekit contributors 2025


#include "OGPolymorphicDataBank.h"

#include "OGCoreModule.h"
#include "Engine/PackageMapClient.h"
#include "Net/RepLayout.h"

void FOGPolymorphicDataBankBase::Empty()
{
	DataMap.Empty();
	GuidReferencesMap.Empty();
	++LastReplicationKey;
#if WITH_EDITOR
	AvailableDataTypes.Empty();
#endif
}

void FOGPolymorphicDataBankBase::AddStructReferencedObjects(FReferenceCollector& Collector)
{
	const FOGPolymorphicStructCache* StructCache = GetStructCache();
	if(!ensure(StructCache))
		return;
	for (auto& [Key, SharedDataRef] : DataMap)
	{
		const UScriptStruct* Struct = StructCache->GetTypeForIndex(Key);
		if (!ensure(Struct))
			continue;
		Collector.AddPropertyReferencesWithStructARO(Struct, &SharedDataRef.Get());
	}
}

bool FOGPolymorphicDataBankBase::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	const FOGPolymorphicStructCache* StructCache = GetStructCache();
	if(!ensure(StructCache)) [[unlikely]]
		return false;
	ensure(DataMap.Num() <= 255);
	uint8 DataNum = DataMap.Num();
	Ar << DataNum;
	
	if (Ar.IsSaving())
	{
		for (auto& [StructKey, SharedDataRef] : DataMap)
		{
			UScriptStruct* Struct = StructCache->GetTypeForIndex(StructKey);
			Ar << StructKey;
			if(!ensure(Struct)) [[unlikely]]
				return false;
			if (Struct->StructFlags & STRUCT_NetSerializeNative)
			{
				Struct->GetCppStructOps()->NetSerialize(Ar, Map, bOutSuccess, &SharedDataRef.Get());
			}
			else
			{
				//modified from FInstancedStruct
				UPackageMapClient* MapClient = Cast<UPackageMapClient>(Map);
				check(::IsValid(MapClient));

				UNetConnection* NetConnection = MapClient->GetConnection();
				check(::IsValid(NetConnection));
				check(::IsValid(NetConnection->GetDriver()));
				
				const TSharedPtr<FRepLayout> RepLayout = NetConnection->GetDriver()->GetStructRepLayout(Struct);
				check(RepLayout.IsValid());

				bool bHasUnmapped = false;
				RepLayout->SerializePropertiesForStruct(Struct, static_cast<FBitArchive&>(Ar), Map, &SharedDataRef.Get(), bHasUnmapped);
				bOutSuccess = true;
			}
		}
		return true;
	}
	else
	{
		//When loading, need to clear out the old values first.
		Empty();
		bool bHasUnmapped = false;
		for (uint8 Idx = 0; Idx < DataNum; ++Idx)
		{
			uint16 StructKey;
			Ar << StructKey;
			UScriptStruct* Struct = StructCache->GetTypeForIndex(StructKey);
			if(!ensure(Struct)) [[unlikely]]
				return false;
			FOGPolymorphicStructBase* NewStructData = &AddUnique_Internal(StructKey, Struct);
			if (Struct->StructFlags & STRUCT_NetSerializeNative)
			{
				Struct->GetCppStructOps()->NetSerialize(Ar, Map, bOutSuccess, NewStructData);
			}
			else
			{
				//modified from FInstancedStruct
				UPackageMapClient* MapClient = Cast<UPackageMapClient>(Map);
				check(::IsValid(MapClient));

				UNetConnection* NetConnection = MapClient->GetConnection();
				check(::IsValid(NetConnection));
				check(::IsValid(NetConnection->GetDriver()));
				
				const TSharedPtr<FRepLayout> RepLayout = NetConnection->GetDriver()->GetStructRepLayout(Struct);
				check(RepLayout.IsValid());

				bool bHasUnmappedScoped = false;
				RepLayout->SerializePropertiesForStruct(Struct, static_cast<FBitArchive&>(Ar), Map, NewStructData, bHasUnmappedScoped);
				bOutSuccess = true;
				bHasUnmapped |= bHasUnmappedScoped;
			}
		}
		return !bHasUnmapped;
	}
}

bool FOGPolymorphicDataBankBase::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
{
	//full serialize the internal structs
	if ( DeltaParams.GatherGuidReferences )
	{
		// Loop over all tracked guids, and return what we have
		for ( const auto& [StructKey, GuidReferences] : GuidReferencesMap )
		{
			DeltaParams.GatherGuidReferences->Append( GuidReferences.UnmappedGUIDs );
			DeltaParams.GatherGuidReferences->Append( GuidReferences.MappedDynamicGUIDs );

			if ( DeltaParams.TrackedGuidMemoryBytes )
			{
				*DeltaParams.TrackedGuidMemoryBytes += GuidReferences.Buffer.Num();
			}
		}
		return true;
	}

	if ( DeltaParams.MoveGuidToUnmapped )
	{
		bool bFound = false;

		const FNetworkGUID GUID = *DeltaParams.MoveGuidToUnmapped;

		// Try to find the guid in the list, and make sure it's on the unmapped lists now
		for ( auto& [StructKey, GuidReferences] : GuidReferencesMap )
		{
			if ( GuidReferences.MappedDynamicGUIDs.Contains( GUID ) )
			{
				GuidReferences.MappedDynamicGUIDs.Remove( GUID );
				GuidReferences.UnmappedGUIDs.Add( GUID );
				bFound = true;
			}
		}
		
		return bFound;
	}

	if ( DeltaParams.bUpdateUnmappedObjects )
	{
		TArray<uint16, TInlineAllocator<8>> ChangedIndices;
		TSet<uint16> KeysToRemove;

		// Loop over each item that has unmapped objects
		for ( auto& [StructKey, GuidReferences] : GuidReferencesMap)
		{			
			if ( ( GuidReferences.UnmappedGUIDs.Num() == 0 && GuidReferences.MappedDynamicGUIDs.Num() == 0 ) || !DataMap.Contains( StructKey ))
			{
				// If for some reason the item is gone (or all guids were removed), we don't need to track guids for this item anymore
				KeysToRemove.Add(StructKey);
				continue;		// We're done with this unmapped item
			}

			// Loop over all the guids, and check to see if any of them are loaded yet
			bool bMappedSomeGUIDs = false;

			for ( auto UnmappedIt = GuidReferences.UnmappedGUIDs.CreateIterator(); UnmappedIt; ++UnmappedIt )
			{
				const FNetworkGUID& GUID = *UnmappedIt;

				if ( DeltaParams.Map->IsGUIDBroken( GUID, false ) )
				{
					// Stop trying to load broken guids
					UnmappedIt.RemoveCurrent();
					continue;
				}

				UObject* Object = DeltaParams.Map->GetObjectFromNetGUID( GUID, false );

				if ( Object != nullptr )
				{
					// This guid loaded!
					if ( GUID.IsDynamic() )
					{
						GuidReferences.MappedDynamicGUIDs.Add( GUID );		// Move back to mapped list
					}
					UnmappedIt.RemoveCurrent();
					bMappedSomeGUIDs = true;
				}
			}

			// Check to see if we loaded any guids. If we did, we can serialize the element again which will load it this time
			if ( bMappedSomeGUIDs )
			{
				DeltaParams.bOutSomeObjectsWereMapped = true;

				if ( !DeltaParams.bCalledPreNetReceive )
				{
					// Call PreNetReceive if we are going to change a value (some game code will need to think this is an actual replicated value)
					DeltaParams.Object->PreNetReceive();
					DeltaParams.bCalledPreNetReceive = true;
				}

				FOGPolymorphicStructBase* ThisElement = &DataMap.FindChecked( StructKey ).Get();

				ChangedIndices.Add(StructKey);

				// Initialize the reader with the stored buffer that we need to read from
				FNetBitReader Reader( DeltaParams.Map, GuidReferences.Buffer.GetData(), GuidReferences.NumBufferBits );

				// Read the property (which should serialize any newly mapped objects as well)
				DeltaParams.Struct = GetStructCache()->GetTypeForIndex(StructKey);
				DeltaParams.Data = ThisElement;
				DeltaParams.Reader = &Reader;
				DeltaParams.NetSerializeCB->NetSerializeStruct(DeltaParams);

				// Let the element know it changed TODO - Once Post Replicated Change is implemented
				//ThisElement->PostReplicatedChange(ArraySerializer);
			}

			// If we have no more guids, we can remove this item for good
			if ( GuidReferences.UnmappedGUIDs.Num() == 0 && GuidReferences.MappedDynamicGUIDs.Num() == 0 )
			{
				KeysToRemove.Add(StructKey);
			}
		}

		for (uint16 Key : KeysToRemove)
		{
			Remove_Internal(Key);
		}
			
		// If we still have unmapped items, then communicate this to the outside
		if ( GuidReferencesMap.Num() > 0 )
		{
			DeltaParams.bOutHasMoreUnmapped = true;
		}

		//TODO: Once Post Replicated Change is implemented for the data bank
		/*if (DeltaParams.bOutSomeObjectsWereMapped)
		{
			ArraySerializer.PostReplicatedChange(ChangedIndices, Items.Num());
			Helper.CallPostReplicatedReceiveOrNot(Items.Num());
		}*/
		
		return true;
	}

	//TODO: Optionally delta serialize internal structs, but only after initial replication has completed

	if (DeltaParams.Writer)
	{
		//-----------------------------
		// Saving
		//-----------------------------	
		check(DeltaParams.Struct);
		
		// Get the old map if its there
		TMap<uint16, uint16> * OldMap = nullptr;
		TSet<uint16> OldKeys;
		int32 BaseReplicationKey = INDEX_NONE;

		// See if the array changed at all. If the ArrayReplicationKey matches we can skip checking individual items
		if (DeltaParams.OldState)
		{
			OldMap = &static_cast<FOGPolymorphicDataBankDeltaState*>(DeltaParams.OldState)->IDToRepKeyMap;
			BaseReplicationKey = static_cast<FOGPolymorphicDataBankDeltaState*>(DeltaParams.OldState)->ContainerReplicationKey;
			
			if (BaseReplicationKey == LastReplicationKey) //If the container is not dirty, we're done
			{
				*DeltaParams.NewState = DeltaParams.OldState->AsShared();
				return false;
			}
			OldMap->GetKeys(OldKeys);
		}

		// Create a new map from the current state of the array		
		FOGPolymorphicDataBankDeltaState * NewState = new FOGPolymorphicDataBankDeltaState();

		check(DeltaParams.NewState);
		*DeltaParams.NewState = MakeShareable( NewState );
		TMap<uint16, uint16>& NewMap = NewState->IDToRepKeyMap;
		NewState->ContainerReplicationKey = LastReplicationKey;

		TSet<uint16> ChangedKeys, AllCurrentKeys;
		for (auto& [Key, DataStruct] : DataMap)
		{
			AllCurrentKeys.Add(Key);
			NewMap.Add(Key, DataStruct->ReplicationKey);
			if (OldKeys.Contains(Key))
			{
				if (DataStruct->ReplicationKey != OldMap->FindChecked(Key))
				{
					ChangedKeys.Add(Key);
				}
			}
			else
			{
				ChangedKeys.Add(Key);
			}
		}

		TSet<uint16> RemovedKeys; 
		RemovedKeys = OldKeys.Difference(AllCurrentKeys);
		
		//----------------------
		// Write it out.
		//----------------------
		FBitWriter& Writer = *DeltaParams.Writer;

		uint8 RemovedCount = RemovedKeys.Num();
		Writer << RemovedCount;
		for (uint16& RemovedKey : RemovedKeys)
		{
			Writer << RemovedKey;
		}

		FOGPolymorphicStructCache* StructCache = GetStructCache();
		uint8 ReplicatedCount = ChangedKeys.Num();
		Writer << ReplicatedCount;
		for (uint16& AddOrChangedKey : ChangedKeys)
		{
			Writer << AddOrChangedKey;
			
			UScriptStruct* Struct = StructCache->GetTypeForIndex(AddOrChangedKey);
			FOGPolymorphicStructBase* DataPtr = &DataMap.Find(AddOrChangedKey)->Get();
			ensure(Struct);
			DeltaParams.Struct = Struct;
			DeltaParams.Data = DataPtr;
			DeltaParams.NetSerializeCB->NetSerializeStruct(DeltaParams);
		}
	}
	else
	{
		//-----------------------------
		// Loading
		//-----------------------------	
		check(DeltaParams.Reader);
		FBitReader& Reader = *DeltaParams.Reader;

		//---------------
		// Read Removed elements
		//---------------
		uint8 RemovedCount;
		Reader << RemovedCount;
		//TODO PreReplicatedRemove callback before any removals take place
		for (int Idx = 0; Idx < RemovedCount; ++Idx)
		{
			uint16 RemovedKey;
			Reader << RemovedKey;
			Remove_Internal(RemovedKey);
			GuidReferencesMap.Remove(RemovedKey);
		}

		//---------------
		// Read Changed/New elements
		//---------------
		FOGPolymorphicStructCache* StructCache = GetStructCache();
		uint8 AddOrChangedCount;
		Reader << AddOrChangedCount;
		TSet<uint16> ChangedKeys, AddedKeys;
		for (int Idx = 0; Idx < AddOrChangedCount; ++Idx)
		{
			uint16 AddedOrChangedKey;
			Reader << AddedOrChangedKey;
			UScriptStruct* Struct = StructCache->GetTypeForIndex(AddedOrChangedKey);
			ensure(Struct);

			FOGPolymorphicStructBase* DataPtr;
			if (DataMap.Contains(AddedOrChangedKey))
			{
				ChangedKeys.Add(AddedOrChangedKey);
				DataPtr = Get_Internal(AddedOrChangedKey);
			}
			else
			{
				AddedKeys.Add(AddedOrChangedKey);
				DataPtr = &AddUnique_Internal(AddedOrChangedKey, Struct);
			}
			
			// Let package map know we want to track and know about any guids that are unmapped during the serialize call
			DeltaParams.Map->ResetTrackedGuids( true );
			// Remember where we started reading from, so that if we have unmapped properties, we can re-deserialize from this data later
			FBitReaderMark Mark( Reader );

			DeltaParams.Struct = Struct;
			DeltaParams.Data = DataPtr;
			DeltaParams.NetSerializeCB->NetSerializeStruct(DeltaParams);

			if ( !Reader.IsError() )
			{
				// Track unmapped guids
                const TSet< FNetworkGUID >& TrackedUnmappedGuids = DeltaParams.Map->GetTrackedUnmappedGuids();
                const TSet< FNetworkGUID >& TrackedMappedDynamicGuids = DeltaParams.Map->GetTrackedDynamicMappedGuids();

				if ( TrackedUnmappedGuids.Num() || TrackedMappedDynamicGuids.Num() )
				{
					FOGPolymorphicDataBankSerializerGuidReferences& GuidReferences = GuidReferencesMap.FindOrAdd( AddedOrChangedKey );

					// If guid lists are different, make note of that, and copy respective list
					if ( !NetworkGuidSetsAreSame( GuidReferences.UnmappedGUIDs, TrackedUnmappedGuids ) )
					{
						// Copy the unmapped guid list to this unmapped item
						GuidReferences.UnmappedGUIDs = TrackedUnmappedGuids;
						DeltaParams.bGuidListsChanged = true;
					}
					
					if ( !NetworkGuidSetsAreSame( GuidReferences.MappedDynamicGUIDs, TrackedMappedDynamicGuids ) )
					{ 
						// Copy the mapped guid list
						GuidReferences.MappedDynamicGUIDs = TrackedMappedDynamicGuids;
						DeltaParams.bGuidListsChanged = true;
					}

					// Copy the buffer into the guid references so we can re-serialize it when the guids change
					GuidReferences.Buffer.Empty();

					// Remember the number of bits in the buffer
					check(Reader.GetPosBits() - Mark.GetPos() <= TNumericLimits<int32>::Max());
					GuidReferences.NumBufferBits = int32(Reader.GetPosBits() - Mark.GetPos());
					
					// Copy the buffer itself
					Mark.Copy( Reader, GuidReferences.Buffer );

					// Hijack this property to communicate that we need to be tracked since we have some unmapped guids
					if ( TrackedUnmappedGuids.Num() )
					{
						DeltaParams.bOutHasMoreUnmapped = true;
					}
				}
				else
				{
					// If we don't have any unmapped objects, make sure we're no longer tracking this item in the unmapped lists
					GuidReferencesMap.Remove( AddedOrChangedKey );
				}
			}

			// Stop tracking unmapped objects
			DeltaParams.Map->ResetTrackedGuids( false );
		}
		//TODO: PostReplicatedChange callback after finished updating data
	}
	return true;
}

FOGPolymorphicStructCache* FOGPolymorphicDataBankBase::GetStructCache() const
{
	return FOGCoreModule::GetUniversalStructCache();
}

FOGPolymorphicStructBase* FOGPolymorphicDataBankBase::Get_Internal(const uint16& Key)
{
	const TSharedRef<FOGPolymorphicStructBase>* Existing = DataMap.Find(Key);
	if (!Existing)
		return nullptr;
	FOGPolymorphicStructBase* ExistingTyped = &Existing->Get();
	if (!ensure(ExistingTyped)) [[unlikely]]
		return nullptr;
	MarkDirty(*ExistingTyped);
	return ExistingTyped;
}

const FOGPolymorphicStructBase* FOGPolymorphicDataBankBase::GetConst_Internal(const uint16& Key) const
{
	const TSharedRef<FOGPolymorphicStructBase>* Existing = DataMap.Find(Key);
	if (!Existing)
		return nullptr;
	const FOGPolymorphicStructBase* ExistingTyped = &Existing->Get();
	if (!ensure(ExistingTyped)) [[unlikely]]
		return nullptr;
	return ExistingTyped;
}

FOGPolymorphicStructBase& FOGPolymorphicDataBankBase::AddUnique_Internal(const uint16& Key,
	const UScriptStruct* ScriptStruct)
{
	if (!ensureMsgf(!DataMap.Contains(Key), TEXT("Tried adding a unique type, but type already exsists"))) [[unlikely]]
		return *Get_Internal(Key);
	//Cannot use the more convenient MakeShared<FOGPolymorphicStructBase> because when dealing with BP we will have access to the script struct but not the type
	FOGPolymorphicStructBase* NewStructPtr = static_cast<FOGPolymorphicStructBase*>(FMemory::MallocZeroed(ScriptStruct->GetCppStructOps()->GetSize(), ScriptStruct->GetCppStructOps()->GetAlignment()));
	if (ScriptStruct->GetCppStructOps()->HasNoopConstructor())
	{
		ScriptStruct->GetCppStructOps()->Construct(NewStructPtr);
	}
	const TSharedRef<FOGPolymorphicStructBase> NewEntry = TSharedRef<FOGPolymorphicStructBase>(NewStructPtr);
	MarkDirty(NewEntry.Get());
	DataMap.Add(Key, NewEntry);

#if WITH_EDITOR
	AvailableDataTypes.Add(ScriptStruct->GetStructCPPName());
#endif
	
	return NewEntry.Get();
}

void FOGPolymorphicDataBankBase::Remove_Internal(const uint16& Key, const UScriptStruct* ScriptStruct)
{
	DataMap.Remove(Key);
#if WITH_EDITOR
	FString NameToRemove;
	if (ScriptStruct)
	{
		NameToRemove = ScriptStruct->GetStructCPPName();
	}
	else
	{
		NameToRemove = GetStructCache()->GetTypeForIndex(Key)->GetStructCPPName();
	}
	AvailableDataTypes.Remove(NameToRemove);
#endif
}
