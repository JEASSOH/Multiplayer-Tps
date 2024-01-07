// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyGameMode.h"
#include "GameFramework/GameStateBase.h"

void ALobbyGameMode::PostLogin(APlayerController * NewPlayer)
{
	Super::PostLogin(NewPlayer);

	int32 NumberOfPlayers = GameState.Get()->PlayerArray.Num();//플레이어 인원 설정
	if (NumberOfPlayers == 2)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			bUseSeamlessTravel = true;
			World->ServerTravel(FString("/Game/Maps/BlasterMap?listen"));//맵 저장 후 리슨 서버로 설정
		}
	}
}