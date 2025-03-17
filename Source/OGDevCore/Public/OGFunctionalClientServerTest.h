/// Copyright Occam's Gamekit contributors 2025

#pragma once

#include "CoreMinimal.h"
#include "FunctionalTest.h"
#include "OGFuture.h"
#include "OGFunctionalClientServerTest.generated.h"

class AOGFunctionalTestRPCBridge;

UENUM(BlueprintType)
enum class EOGCheckpointReturn : uint8
{
	Server,
	Client
};

UCLASS()
class OGDEVCORE_API AOGFunctionalClientServerTest : public AFunctionalTest
{
	friend AOGFunctionalTestRPCBridge;
	
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AOGFunctionalClientServerTest();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void Tick(float DeltaSeconds) override;
	
	UFUNCTION(BlueprintCallable, meta=(Latent, LatentInfo="LatentInfo", ExpandEnumAsExecs="OutReturn"))
	void Checkpoint(FLatentActionInfo LatentInfo, EOGCheckpointReturn& OutReturn);

	void SetServerCheckpoint(const bool bIsAcknowledge);

	UFUNCTION()
	void OnRep_ServerCheckpoint(bool OldCheckpoint);
	
	UFUNCTION(NetMulticast, Reliable)
	void RPC_StartTestOnAll();

	// Called when a new client connects to the game
	UFUNCTION()
	void OnClientConnected(AGameModeBase* GameMode, APlayerController* NewPlayer);
    
	// Called when a client disconnects from the game
	UFUNCTION()
	void OnClientDisconnected(AGameModeBase* GameMode, AController* DisconnectedPlayer);
    
	// Creates a new RPC bridge for the specified client
	UFUNCTION()
	void CreateRPCBridgeForClient(APlayerController* ClientController);

	TOGFuture<void> WhenAllClientsReachCheckpoint(bool bIsAcknowledge);
	
	// Get the RPC bridge associated with a specific client
	UFUNCTION(BlueprintCallable, Category = "Functional Testing")
	AOGFunctionalTestRPCBridge* GetRPCBridgeForClient(APlayerController* ClientController);
	
	UFUNCTION()
	virtual void PrepareTest() override;
	UFUNCTION()
	virtual void StartTest() override;

	//Hide non-virtual AFunctionalTest::LogStep as a quick way to make asserts work over network.
	void LogStep(ELogVerbosity::Type Verbosity, const FString& Message);

	virtual bool IsReady_Implementation() override;

	void LinkClientRPCBridge(AOGFunctionalTestRPCBridge* Bridge);

private:
	// Register to receive notifications about player connections/disconnections
	void RegisterConnectionEvents();

	void StartNewClient() const;
public:
	UPROPERTY(EditDefaultsOnly)
	int NumClients = 1;

protected:
	// Class to use when spawning the RPC bridge actors
	UPROPERTY(EditDefaultsOnly, Category = "Functional Testing")
	TSubclassOf<AOGFunctionalTestRPCBridge> RPCBridgeClass = nullptr;
	
	//Server-only - all bridges used to receive messages from the clients
	UPROPERTY()
	TMap<AController*, AOGFunctionalTestRPCBridge*> ServerRPCBridges;

	//Client-only - specific bridge used by this client
	UPROPERTY(BlueprintReadOnly)
	AOGFunctionalTestRPCBridge* ClientRPCBridge = nullptr;

	//Server communicates checkpoints via rep notify to reduce the chance that there are values still to be replicated by the time the checkpoint is finished.
	UPROPERTY(ReplicatedUsing=OnRep_ServerCheckpoint)
	bool IsServerCheckpointAcknowledged = true;

private:
	TOGPromise<void> WhenServerReachesCheckpoint;
	TOGPromise<void> WhenServerAcknowledgesCheckpoint;
	TOGPromise<void> WhenAllClientBridgesEstablished;

	bool bAreClientConnectionsReady = false;

	bool bRequestedNewClients = false;
	int NumClientsToSpawn = 0;
	//Give a small amount of time for existing clients to connect before requesting new clients
	float TimeForClientConnection = 0.1f;
};
