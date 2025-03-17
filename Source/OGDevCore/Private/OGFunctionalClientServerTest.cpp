/// Copyright Occam's Gamekit contributors 2025

#include "OGFunctionalClientServerTest.h"

#include "OGAsyncUtils.h"
#include "GameFramework/GameModeBase.h"
#include "OGFunctionalTestRPCBridge.h"
#include "OGFutureUtilities.h"
#include "Editor/UnrealEdEngine.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

// Sets default values
AOGFunctionalClientServerTest::AOGFunctionalClientServerTest()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	bNetLoadOnClient = true;
}

void AOGFunctionalClientServerTest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, IsServerCheckpointAcknowledged, Params);
}

void AOGFunctionalClientServerTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bIsRunning)
		return;
	
	TimeForClientConnection -= DeltaSeconds;
	if (TimeForClientConnection < 0 && !bRequestedNewClients)
	{
		//const UUnrealEdEngine* EdEngine = Cast<UUnrealEdEngine>(GEngine);
		//const bool bSkipFirstClient = EdEngine && GetNetMode() == NM_DedicatedServer;
		//NumClientsToSpawn = NumClients - (bSkipFirstClient ? 1 : 0);
		NumClientsToSpawn = NumClients - ServerRPCBridges.Num();
		bRequestedNewClients = true;
	}

	//Spawn one client per tick because the request isn't processed immediately
	if (NumClientsToSpawn > 0)
	{
		StartNewClient();
		--NumClientsToSpawn;
	}
}

void AOGFunctionalClientServerTest::OnRep_ServerCheckpoint(bool OldCheckpoint)
{
	ensure(!HasAuthority());
	if (IsServerCheckpointAcknowledged)
	{
		WhenServerReachesCheckpoint = TOGPromise<void>();
		WhenServerAcknowledgesCheckpoint->Fulfill();
	}
	else
	{
		WhenServerAcknowledgesCheckpoint = TOGPromise<void>();
		WhenServerReachesCheckpoint->Fulfill();
	}
}

void AOGFunctionalClientServerTest::Checkpoint(FLatentActionInfo LatentInfo, EOGCheckpointReturn& OutReturn)
{
	if (HasAuthority())
	{
		OutReturn = EOGCheckpointReturn::Server;
		WhenAllClientsReachCheckpoint(false)->WeakThen(this, [this]()
		{
			SetServerCheckpoint(false);
			return WhenAllClientsReachCheckpoint(true);
		})->WeakThen(this, [this, LatentInfo]()mutable
		{
			SetServerCheckpoint(true);
			FOGAsyncUtils::ExecuteLatentAction(LatentInfo);
		});
	}
	else
	{
		OutReturn = EOGCheckpointReturn::Client;
		if (!ensureAlways(ClientRPCBridge))
			return;
		
		ClientRPCBridge->RPC_ClientReachedCheckpoint(false);
		WhenServerReachesCheckpoint->WeakThen(this, [this]() ->TOGPromise<void>&
		{
			ClientRPCBridge->RPC_ClientReachedCheckpoint(true);
			return WhenServerAcknowledgesCheckpoint;
		})->WeakThen(this, [this, LatentInfo]() mutable
		{
			FOGAsyncUtils::ExecuteLatentAction(LatentInfo);
		});
	}
}

void AOGFunctionalClientServerTest::SetServerCheckpoint(const bool bIsAcknowledge)
{
	IsServerCheckpointAcknowledged = bIsAcknowledge;
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, IsServerCheckpointAcknowledged, this);
}

void AOGFunctionalClientServerTest::RPC_StartTestOnAll_Implementation()
{
	if (!HasAuthority())
	{
		ReceiveStartTest();
	}
}

void AOGFunctionalClientServerTest::OnClientConnected(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	//No need for a bridge to a local controller
	if (NewPlayer->IsLocalController())
		return;
	ensure(HasAuthority());
	CreateRPCBridgeForClient(NewPlayer);
}

void AOGFunctionalClientServerTest::OnClientDisconnected(AGameModeBase* GameMode, AController* DisconnectedPlayer)
{
	//No need for a bridge to a local controller
	if (DisconnectedPlayer->IsLocalController())
		return;
	
	ensure(HasAuthority());
	
	if (ServerRPCBridges.Contains(DisconnectedPlayer))
	{
		AOGFunctionalTestRPCBridge* Bridge = ServerRPCBridges.FindAndRemoveChecked(DisconnectedPlayer);
		Bridge->Destroy();
	}
}

void AOGFunctionalClientServerTest::CreateRPCBridgeForClient(APlayerController* ClientController)
{
	if (!ensure(HasAuthority() && ClientController && !ServerRPCBridges.Contains(ClientController)))
		return;
    
	// Spawn parameters
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = ClientController;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
	// Spawn the RPC bridge actor
	AOGFunctionalTestRPCBridge* NewBridge = GetWorld()->SpawnActor<AOGFunctionalTestRPCBridge>(
		RPCBridgeClass ? RPCBridgeClass->StaticClass() : AOGFunctionalTestRPCBridge::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams
	);
    
	if (NewBridge)
	{
		// Initialize the bridge with the client controller
		NewBridge->ParentFunctionalTest = this;
        
		// Add to our map
		ServerRPCBridges.Add(ClientController, NewBridge);
	}

	if (ServerRPCBridges.Num() == NumClients)
	{
		WhenAllClientBridgesEstablished->Fulfill();
	}
}

TOGFuture<void> AOGFunctionalClientServerTest::WhenAllClientsReachCheckpoint(bool bIsAcknowledge)
{
	ensure(HasAuthority());
	TArray<FOGFuture> AllClientFutures;
	for (auto& [Controller, Bridge] : ServerRPCBridges)
	{
		AllClientFutures.Add(Bridge->GetCheckpointFuture(bIsAcknowledge));
	}
	return UOGFutureUtilities::FutureAll(this, AllClientFutures);
}

AOGFunctionalTestRPCBridge* AOGFunctionalClientServerTest::GetRPCBridgeForClient(APlayerController* ClientController)
{
	if (!ensure(HasAuthority()))
		return nullptr;

	AOGFunctionalTestRPCBridge** Bridge = ServerRPCBridges.Find(ClientController);
	if (!ensure(Bridge))
		return nullptr;
	return *Bridge;
}

void AOGFunctionalClientServerTest::PrepareTest()
{
	if (GetNetMode() == NM_Standalone)
	{
		FinishTest(EFunctionalTestResult::Invalid, TEXT("Cannot run a client-server test in NetMode Standalone"));
	}
	
	Super::PrepareTest();
	
	//If we're rerunning the test, use existing clients
	if (WhenAllClientBridgesEstablished->IsFulfilled())
	{
		return;
	}
	
	RegisterConnectionEvents();
	WhenAllClientBridgesEstablished->WeakThen(this, [this]()
	{
		return WhenAllClientsReachCheckpoint(true);
	})->WeakThen(this, [this]()
	{
		bAreClientConnectionsReady = true;
	});
}

void AOGFunctionalClientServerTest::StartTest()
{
	Super::StartTest();
	RPC_StartTestOnAll();
}

void AOGFunctionalClientServerTest::LogStep(ELogVerbosity::Type Verbosity, const FString& Message)
{
	if (HasAuthority())
	{
		Super::LogStep(Verbosity, Message);
	}
	else
	{
		ClientRPCBridge->RPC_ForwardLogStep(Verbosity, Message);
	}
}

bool AOGFunctionalClientServerTest::IsReady_Implementation()
{
	return bAreClientConnectionsReady && Super::IsReady_Implementation();
}

void AOGFunctionalClientServerTest::LinkClientRPCBridge(AOGFunctionalTestRPCBridge* Bridge)
{
	ClientRPCBridge = Bridge;
	ClientRPCBridge->RPC_ClientReachedCheckpoint(true);
}

void AOGFunctionalClientServerTest::RegisterConnectionEvents()
{
	if(!ensure(HasAuthority())) [[unlikely]] 
		return;

	ServerRPCBridges.Empty();
	

	FGameModeEvents::OnGameModePostLoginEvent().AddUObject(this, &ThisClass::OnClientConnected);
	FGameModeEvents::OnGameModeLogoutEvent().AddUObject(this, &ThisClass::OnClientDisconnected);

	// Spawn RPC bridge for any existing connections
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && !PC->IsLocalController()) // We only care about remote clients
		{
			// Check if this client already has a bridge
			if (!ServerRPCBridges.Contains(PC) || !IsValid(ServerRPCBridges.FindChecked(PC)))
			{
				CreateRPCBridgeForClient(PC);
			}
		}
	}
}

void AOGFunctionalClientServerTest::StartNewClient() const
{
	ensure(HasAuthority());
	UUnrealEdEngine* EdEngine = Cast<UUnrealEdEngine>(GEngine);
	EdEngine->RequestLateJoin();
	//TODO: Handle starting clients when we don't have EdEngine (presumably this will happen when running tests autonomously)
}


