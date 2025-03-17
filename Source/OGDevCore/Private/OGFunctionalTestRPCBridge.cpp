/// Copyright Occam's Gamekit contributors 2025


#include "OGFunctionalTestRPCBridge.h"

#include "OGFunctionalClientServerTest.h"
#include "Net/UnrealNetwork.h"


// Sets default values
AOGFunctionalTestRPCBridge::AOGFunctionalTestRPCBridge()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AOGFunctionalTestRPCBridge::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, ParentFunctionalTest);
}

void AOGFunctionalTestRPCBridge::RPC_ClientReachedCheckpoint_Implementation(const bool bIsAcknowledge)
{
	ensure(HasAuthority());
	if (bIsAcknowledge)
	{
		WhenClientReachesCheckpoint = TOGPromise<void>();
		WhenClientAcknowledgesCheckpoint->Fulfill();
	}
	else
	{
		WhenClientAcknowledgesCheckpoint = TOGPromise<void>();
		WhenClientReachesCheckpoint->Fulfill();
	}
}

// Called when the game starts or when spawned
void AOGFunctionalTestRPCBridge::BeginPlay()
{
	Super::BeginPlay();
	if (HasLocalNetOwner())
	{
		ensure(ParentFunctionalTest);
		ParentFunctionalTest->LinkClientRPCBridge(this);
	}
}

void AOGFunctionalTestRPCBridge::RPC_ForwardLogStep_Implementation(uint8 Verbosity, const FString& Message)
{
	ensure(HasAuthority());
	ensure(ParentFunctionalTest);
	ParentFunctionalTest->LogStep(static_cast<ELogVerbosity::Type>(Verbosity), Message);
}


