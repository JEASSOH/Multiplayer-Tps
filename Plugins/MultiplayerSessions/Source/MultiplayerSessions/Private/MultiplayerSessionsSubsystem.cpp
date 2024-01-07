// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"

UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem():
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete)),
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete)),
	DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete)),
	StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete))

{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		SessionInterface = Subsystem->GetSessionInterface();
	}
}
void UMultiplayerSessionsSubsystem::CreateSession(int32 NumPublicConnections, FString MatchType)
{
	// 세션 인터페이스가 유효한지 확인합니다.
	if (!SessionInterface.IsValid())
	{
		return;
	}

	// 이미 세션이 존재하면 세션을 제거하고 새로 생성할 준비
	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession != nullptr)
	{
		bCreateSessionOnDestroy = true;
		LastNumPublicConnections = NumPublicConnections;
		LastMatchType = MatchType;

		DestroySession();
	}

	// 세션 생성 완료 시의 콜백을 바인딩하기 위해 DelegateHandle을 저장
	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	// 세션 설정 객체를 생성하고 원하는 세션 설정을 구성
	LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
	LastSessionSettings->bIsLANMatch = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
	LastSessionSettings->NumPublicConnections = NumPublicConnections;
	LastSessionSettings->bAllowJoinInProgress = true;
	LastSessionSettings->bAllowJoinViaPresence = true;
	LastSessionSettings->bShouldAdvertise = true;
	LastSessionSettings->bUsesPresence = true;
	LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->BuildUniqueId = 1;
	LastSessionSettings->bUseLobbiesIfAvailable = true;


	// 로컬 플레이어를 통해 세션을 생성
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();

	if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);

		// 세션 생성 완료를 알리기 위해 커스텀 델리게이트를 브로드캐스트
		MultiplayerOnCreateSessionComplete.Broadcast(false);
	}
}

//void UMultiplayerSessionsSubsystem::CreateSession(int32 NumPublicConnections, FString MatchType)
//{
//	if (!SessionInterface.IsValid())
//	{	
//		return;
//	}
//
//	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
//	if (ExistingSession != nullptr)
//	{
//		bCreateSessionOnDestroy = true;
//		LastNumPublicConnections = NumPublicConnections;
//		LastMatchType = MatchType;
//
//		DestroySession();
//	}
//
//	// Store the delegate in a FDelegateHandle so we can later remove it from the delegate list
//	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);
//
//	LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
//	LastSessionSettings->bIsLANMatch = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
//	LastSessionSettings->NumPublicConnections = NumPublicConnections;
//	LastSessionSettings->bAllowJoinInProgress = true;
//	LastSessionSettings->bAllowJoinViaPresence = true;
//	LastSessionSettings->bShouldAdvertise = true;
//	LastSessionSettings->bUsesPresence = true;
//	LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
//	LastSessionSettings->BuildUniqueId = 1;
//	LastSessionSettings->bUseLobbiesIfAvailable = true;
//
//	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
//	if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings))
//	{
//		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
//
//		// Broadcast our own custom delegate
//		MultiplayerOnCreateSessionComplete.Broadcast(false);
//	}
//}

void UMultiplayerSessionsSubsystem::FindSessions(int32 MaxSearchResults)
{
	if (!SessionInterface.IsValid())
	{
		return;
	}

	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);
	// 세션 검색 설정 객체를 생성하고 원하는 검색 설정을 구성
	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastSessionSearch->MaxSearchResults = MaxSearchResults;
	LastSessionSearch->bIsLanQuery = IOnlineSubsystem::Get()->GetSubsystemName() == "NULL" ? true : false;
	LastSessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
	
	// 로컬 플레이어를 통해 세션을 검색
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);

		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}
}

void UMultiplayerSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);
	// 로컬 플레이어를 통해 세션에 참여
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);

		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
	}
}

void UMultiplayerSessionsSubsystem::DestroySession()
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnDestroySessionComplete.Broadcast(false);
		return;
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);

	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		MultiplayerOnDestroySessionComplete.Broadcast(false);
	}
}

void UMultiplayerSessionsSubsystem::StartSession()
{
}

void UMultiplayerSessionsSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}

	MultiplayerOnCreateSessionComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}

	if (LastSessionSearch->SearchResults.Num() <= 0)
	{
		MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}

	MultiplayerOnFindSessionsComplete.Broadcast(LastSessionSearch->SearchResults, bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}

	MultiplayerOnJoinSessionComplete.Broadcast(Result);
}

void UMultiplayerSessionsSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}
	if (bWasSuccessful && bCreateSessionOnDestroy)
	{
		bCreateSessionOnDestroy = false;
		CreateSession(LastNumPublicConnections, LastMatchType);
	}
	MultiplayerOnDestroySessionComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
}
