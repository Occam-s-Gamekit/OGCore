/// Copyright Occam's Gamekit contributors 2025

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OGFuture.h"
#include "OGFunctionalTestRPCBridge.generated.h"

class AOGFunctionalClientServerTest;

UCLASS()
class OGDEVCORE_API AOGFunctionalTestRPCBridge : public AActor
{
	friend AOGFunctionalClientServerTest;
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AOGFunctionalTestRPCBridge();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(Server, Reliable)
	void RPC_ClientReachedCheckpoint(const bool bIsAcknowledge);

	TOGFuture<void> GetCheckpointFuture(const bool bIsAcknowledge) {return bIsAcknowledge ? WhenClientAcknowledgesCheckpoint : WhenClientReachesCheckpoint; }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable)
	void RPC_ForwardLogStep(uint8 Verbosity, const FString& Message);

	UPROPERTY(Replicated)
	AOGFunctionalClientServerTest* ParentFunctionalTest = nullptr;
	
	TOGPromise<void> WhenClientReachesCheckpoint;
	TOGPromise<void> WhenClientAcknowledgesCheckpoint;
};
