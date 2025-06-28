#include <engine/shared/config.h>

#include <game/client/gameclient.h>

#include <base/system.h>
#include <game/generated/protocol.h>
#include <game/localization.h>

#include "base/log.h"
#include "mod.h"

class CMod::CIden
{
private:
	enum class EType
	{
		ID,
		ADDR,
		NAME,
		ERROR,
	};
	EType m_Type;
	int m_ClientId;
	std::string m_Content;

public:
	[[nodiscard]] std::string Printable() const
	{
		switch(m_Type)
		{
		case EType::ID:
			return "#" + m_Content;
		case EType::ADDR:
			return std::to_string(m_ClientId);
		case EType::NAME:
			return "'" + std::to_string(m_ClientId) + ": " + m_Content + "'";
		case EType::ERROR:
			dbg_assert(false, "Tried to get Printable of error Iden");
		default:
			dbg_break();
		}
	}
	[[nodiscard]] std::string RCon() const
	{
		switch(m_Type)
		{
		case EType::ID:
			return std::to_string(m_ClientId);
		case EType::ADDR:
			return m_Content;
		case EType::NAME:
			return std::to_string(m_ClientId);
		case EType::ERROR:
			dbg_assert(false, "Tried to get RCon of error Iden");
		default:
			dbg_break();
		}
	}
	[[nodiscard]] const char *Error() const
	{
		return m_Type == EType::ERROR ? m_Content.c_str() : nullptr;
	}
	CIden() = delete;
	enum class EParseMode
	{
		NAME,
		ID_OR_ADDR,
		ID,
	};
	CIden(const CMod *pThis, const char *pStr, EParseMode Mode)
	{
		CGameClient &This = *pThis->GameClient();
		if(Mode == EParseMode::NAME)
		{
			for(const auto &Player : This.m_aClients)
				if(str_comp(pStr, Player.m_aName) == 0)
				{
					m_Type = EType::NAME;
					m_ClientId = Player.ClientId();
					m_Content = Player.m_aName;
					return;
				}
			for(const auto &Player : This.m_aClients)
				if(str_comp_nocase(pStr, Player.m_aName) == 0)
				{
					m_Type = EType::NAME;
					m_ClientId = Player.ClientId();
					m_Content = Player.m_aName;
					return;
				}
			for(const auto &Player : This.m_aClients)
				if(str_utf8_comp_confusable(pStr, Player.m_aName) == 0)
				{
					m_Type = EType::NAME;
					m_ClientId = Player.ClientId();
					m_Content = Player.m_aName;
					return;
				}
			m_Type = EType::ERROR;
			m_Content = "'" + std::string(pStr) + "' was not found";
			return;
		}
		int Id;
		if(str_toint(pStr, &Id))
		{
			if(Id < 0 || Id > (int)std::size(This.m_aClients))
			{
				m_Type = EType::ERROR;
				m_Content = "Id " + std::to_string(Id) + " is not in range 0 to " + std::to_string(std::size(This.m_aClients));
				return;
			}
			const auto &Player = This.m_aClients[Id];
			if(!Player.m_Active)
			{
				m_Type = EType::ERROR;
				m_Content = "Id " + std::to_string(Id) + " is not connected";
				return;
			}
			m_Type = EType::NAME;
			m_Content = Player.m_aName;
			m_ClientId = Id;
			return;
		}
		if(Mode == EParseMode::ID_OR_ADDR)
		{
			NETADDR Addr;
			if(net_addr_from_str(&Addr, pStr) == 0)
			{
				char aAddr[128];
				net_addr_str(&Addr, aAddr, sizeof(aAddr), false);
				if(net_addr_is_local(&Addr))
				{
					m_Type = EType::ERROR;
					m_Content = "'" + std::string(aAddr) + "' is a local address";
					return;
				}
				m_Type = EType::ADDR;
				m_Content = std::string(aAddr);
			}
			m_Type = EType::ERROR;
			m_Content = "'" + std::string(pStr) + "' is not a valid address or id";
		}
		else
		{
			m_Type = EType::ERROR;
			m_Content = "'" + std::string(pStr) + "' is not a valid id";
		}
	}
};

static int UnitLengthSeconds(char Unit)
{
	switch(Unit)
	{
	case 's':
	case 'S': return 1;
	case 'm':
	case 'M': return 60;
	case 'h':
	case 'H': return 60 * 60;
	case 'd':
	case 'D': return 60 * 60 * 24;
	default: return -1;
	}
}

int CMod::TimeFromStr(const char *pStr, char OutUnit)
{
	double Time = -1;
	char InUnit = OutUnit;
	std::sscanf(pStr, "%lf%c", &Time, &InUnit);
	if(Time < 0)
		return -1;
	int InUnitLength = UnitLengthSeconds(InUnit);
	if(InUnitLength < 0)
		return -1;
	int OutUnitLength = UnitLengthSeconds(OutUnit);
	if(OutUnitLength < 0)
		return -1;
	return std::round(Time * (float)InUnitLength / (float)OutUnitLength);
}

void CMod::Kill(const CMod::CIden &Iden, bool Silent)
{
	if(Iden.Error())
	{
		GameClient()->Echo(Iden.Error());
		return;
	}
	char aBuf[256];
	const std::string IdenRCon = Iden.RCon();
	const char *pIdenRCon = IdenRCon.c_str();
	if(Silent)
		str_format(aBuf, sizeof(aBuf), "set_team %s -1; set_team %s 0", pIdenRCon, pIdenRCon);
	else
		str_format(aBuf, sizeof(aBuf), "kill_pl %s", pIdenRCon);
	Client()->Rcon(aBuf);
	str_format(aBuf, sizeof(aBuf), "Killed %s", Iden.Printable().c_str());
	GameClient()->Echo(aBuf);
}

void CMod::Kick(const CMod::CIden &Iden, const char *pReason)
{
	if(Iden.Error())
	{
		GameClient()->Echo(Iden.Error());
		return;
	}
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "kick %s %s", Iden.RCon().c_str(), pReason);
	Client()->Rcon(aBuf);
	if(pReason[0] == '\0')
		str_format(aBuf, sizeof(aBuf), "Kicked %s", Iden.Printable().c_str());
	else
		str_format(aBuf, sizeof(aBuf), "Kicked %s (%s)", Iden.Printable().c_str(), pReason);
	GameClient()->Echo(aBuf);
}

void CMod::Ban(const CMod::CIden &Iden, const char *pTime, const char *pReason)
{
	if(Iden.Error())
	{
		GameClient()->Echo(Iden.Error());
		return;
	}
	char aBuf[256];
	const int Minutes = TimeFromStr(pTime, 'm');
	str_format(aBuf, sizeof(aBuf), "ban %s %d %s", Iden.RCon().c_str(), Minutes, pReason);
	Client()->Rcon(aBuf);
	if(pReason[0] == '\0')
		str_format(aBuf, sizeof(aBuf), "Banned %s for %d minutes", Iden.Printable().c_str(), Minutes);
	else
		str_format(aBuf, sizeof(aBuf), "Banned %s for %d minutes (%s)", Iden.Printable().c_str(), Minutes, pReason);
	GameClient()->Echo(aBuf);
}

void CMod::Mute(const CMod::CIden &Iden, const char *pTime, const char *pReason)
{
	if(Iden.Error())
	{
		GameClient()->Echo(Iden.Error());
		return;
	}
	char aBuf[256];
	const int Seconds = TimeFromStr(pTime, 'm');
	str_format(aBuf, sizeof(aBuf), "muteid %s %d %s", Iden.RCon().c_str(), Seconds, pReason);
	Client()->Rcon(aBuf);
	if(pReason[0] == '\0')
		str_format(aBuf, sizeof(aBuf), "Muted %s for %d seconds", Iden.Printable().c_str(), Seconds);
	else
		str_format(aBuf, sizeof(aBuf), "Muted %s for %d seconds (%s)", Iden.Printable().c_str(), Seconds, pReason);
	GameClient()->Echo(aBuf);
}

void CMod::OnConsoleInit()
{
	auto FRegisterModCommand = [&](const char *pName, const char *pParams, const char *pHelp, void (*FCallback)(IConsole::IResult *, CMod *)) {
		Console()->Register(pName, pParams, CFGFLAG_CLIENT, (CConsole::FCommandCallback)FCallback, this, pHelp);
	};

	FRegisterModCommand("mod_rcon_ban", "s[id|ip] s[time (minutes)] ?r[reason]", "RCon ban someone", [](IConsole::IResult *pResult, CMod *pThis) {
		pThis->Ban(CIden(pThis, pResult->GetString(0), CIden::EParseMode::ID_OR_ADDR), pResult->GetString(1), pResult->GetString(2));
	});
	FRegisterModCommand("mod_rcon_ban_name", "s[name] s[time (minutes)] ?r[reason]", "RCon ban someone by name", [](IConsole::IResult *pResult, CMod *pThis) {
		pThis->Ban(CIden(pThis, pResult->GetString(0), CIden::EParseMode::NAME), pResult->GetString(1), pResult->GetString(2));
	});

	FRegisterModCommand("mod_rcon_kick", "s[id|ip] ?r[reason]", "RCon kick someone", [](IConsole::IResult *pResult, CMod *pThis) {
		pThis->Kick(CIden(pThis, pResult->GetString(0), CIden::EParseMode::ID), pResult->GetString(2));
	});
	FRegisterModCommand("mod_rcon_kick_name", "s[name] ?r[reason]", "RCon kick someone by name", [](IConsole::IResult *pResult, CMod *pThis) {
		pThis->Kick(CIden(pThis, pResult->GetString(0), CIden::EParseMode::NAME), pResult->GetString(2));
	});

	FRegisterModCommand("mod_rcon_mute", "s[id] s[time (minutes)] ?r[reason]", "RCon mute someone", [](IConsole::IResult *pResult, CMod *pThis) {
		pThis->Mute(CIden(pThis, pResult->GetString(0), CIden::EParseMode::ID), pResult->GetString(1), pResult->GetString(2));
	});
	FRegisterModCommand("mod_rcon_mute_name", "s[name] s[time (minutes)] ?r[reason]", "RCon mute someone by name", [](IConsole::IResult *pResult, CMod *pThis) {
		pThis->Mute(CIden(pThis, pResult->GetString(0), CIden::EParseMode::NAME), pResult->GetString(1), pResult->GetString(2));
	});

	FRegisterModCommand("mod_rcon_kill", "s[id/ip] ?s[2] ?s[3] ?s[4] ?s[5] ?s[6] ?s[7] ?s[8]", "RCon kill people", [](IConsole::IResult *pResult, CMod *pThis) {
		for(int i = 0; i < 8; ++i)
			if(pResult->GetString(i)[0] != '\0')
				pThis->Kill(CIden(pThis, pResult->GetString(i), CIden::EParseMode::ID), true);
	});
	FRegisterModCommand("mod_rcon_kill_name", "s[name] ?s[2] ?s[3] ?s[4] ?s[5] ?s[6] ?s[7] ?s[8]", "RCon kill people by name", [](IConsole::IResult *pResult, CMod *pThis) {
		for(int i = 0; i < 8; ++i)
			if(pResult->GetString(i)[0] != '\0')
				pThis->Kill(CIden(pThis, pResult->GetString(i), CIden::EParseMode::NAME), true);
	});

	Console()->Chain("+fire", [](IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData) {
		pfnCallback(pResult, pCallbackUserData);
		if(pResult->GetInteger(0) == 1)
			((CMod *)pUserData)->OnFire();
	}, this);
}

void CMod::OnRender()
{
	// If haven't reshot someone in last 5 seconds, cancel shot
	if(Client()->State() != IClient::STATE_ONLINE && m_LastShotClientId != -1 && time() - m_LastShotTime > (int64_t)5e9)
	{
		m_LastShotClientId = -1;
		GameClient()->Echo(TCLocalize("Cancelling shot"));
	}

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	GameClient()->RenderTools()->MapScreenToGroup(GameClient()->m_Camera.m_Center.x, GameClient()->m_Camera.m_Center.y, GameClient()->Layers()->GameGroup(), GameClient()->m_Camera.m_Zoom);

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	Graphics()->TextureClear();

	if(g_Config.m_ClShowPlayerHitBoxes > 0)
	{
		auto FRenderHitbox = [&](vec2 Position, float Alpha) {
			if(Alpha <= 0.0f)
				return;
			const float RadiusInner = 16.0f;
			const float RadiusOuter = 30.0f;
			Graphics()->QuadsBegin();
			Graphics()->SetColor(ColorRGBA(0.0f, 1.0f, 0.0f, 0.2f * Alpha));
			Graphics()->DrawCircle(Position.x, Position.y, RadiusInner, 20);
			Graphics()->DrawCircle(Position.x, Position.y, RadiusOuter, 20);
			Graphics()->QuadsEnd();
			IEngineGraphics::CLineItem aLines[] = {
				{Position.x, Position.y - RadiusOuter, Position.x, Position.y + RadiusOuter},
				{Position.x - RadiusOuter, Position.y, Position.x + RadiusOuter, Position.y},
			};
			Graphics()->LinesBegin();
			Graphics()->SetColor(ColorRGBA(1.0f, 0.0f, 0.0f, 0.8f * Alpha));
			Graphics()->LinesDraw(aLines, std::size(aLines));
			Graphics()->LinesEnd();
		};

		for(const auto &Player : GameClient()->m_aClients)
		{
			const int ClientId = Player.ClientId();
			const auto &Char = GameClient()->m_Snap.m_aCharacters[ClientId];
			if(!Char.m_Active || !Player.m_Active)
				continue;
			if(Player.m_Team < 0)
				continue;

			if(!(in_range(Player.m_RenderPos.x, ScreenX0, ScreenX1) && in_range(Player.m_RenderPos.y, ScreenY0, ScreenY1)))
				continue;

			float Alpha = 1.0f;
			if(GameClient()->IsOtherTeam(ClientId))
				Alpha *= (float)g_Config.m_ClShowOthersAlpha / 100.0f;

			FRenderHitbox(Player.m_RenderPos, Alpha);

			if(g_Config.m_ClShowPlayerHitBoxes > 1)
			{
				// From CPlayers::RenderPlayer
				vec2 ShadowPosition = mix(
					vec2(GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev.m_X, GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev.m_Y),
					vec2(GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_X, GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_Y),
					Client()->IntraGameTick(g_Config.m_ClDummy));
				FRenderHitbox(ShadowPosition, Alpha * 0.75f);
			}
		}
	}
}

void CMod::OnStateChange(int OldState, int NewState)
{
	m_LastShotClientId = -1;
}

void CMod::OnFire()
{
	auto FGetBestClient = [&]() -> const CGameClient::CClientData * {
		if(Client()->State() != IClient::STATE_ONLINE)
			return nullptr;
		if(g_Config.m_ClModWeapon == -1)
			return nullptr;
		if(!Client()->RconAuthed())
			return nullptr;
		if(GameClient()->m_aLocalIds[g_Config.m_ClDummy] < 0)
			return nullptr;
		const auto &Player = GameClient()->m_aClients[GameClient()->m_aLocalIds[g_Config.m_ClDummy]];
		if(!Player.m_Active)
			return nullptr;
		if(Player.m_Team == TEAM_SPECTATORS)
			return nullptr;
		if(Player.m_RenderPrev.m_Weapon != g_Config.m_ClModWeapon)
			return nullptr;
		const vec2 Pos = Player.m_RenderPos;
		const vec2 Angle = normalize(GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy]);
		// Find person who we have shot
		const CGameClient::CClientData *pBestClient = nullptr;
		float BestClientScore = -INFINITY;
		for(const CGameClient::CClientData &Other : GameClient()->m_aClients)
		{
			if(!Other.m_Active || Player.ClientId() == Other.ClientId() || GameClient()->IsOtherTeam(Other.ClientId()))
				continue;
			const float PosDelta = distance(Other.m_RenderPos, Pos);
			const float MaxRange = (g_Config.m_ClModWeapon == 0 ? 100.0f : 750.0f);
			log_debug("Mod", "%d: pos delta %f/%f\n", Other.ClientId(), PosDelta, MaxRange);
			if(PosDelta > MaxRange)
				continue;
			const float AngleDelta = dot(normalize(Other.m_RenderPos - Pos), Angle);
			log_debug("Mod", "%d: angle delta %f\n", Other.ClientId(), AngleDelta);
			if(AngleDelta < 0.7f)
				continue;
			const float Score = (1.0f - AngleDelta) * 5.0f + (MaxRange - PosDelta) * 0.01f;
			if(Score > BestClientScore)
			{
				BestClientScore = Score;
				pBestClient = &Other;
			}
		}
		return pBestClient;
	};

	const CGameClient::CClientData *pBestClient = FGetBestClient();
	char aBuf[256];
	if(!pBestClient)
	{
		if(m_LastShotClientId != -1)
			GameClient()->Echo(TCLocalize("Cancelling shot"));
		return;
	}
	if(pBestClient->ClientId() != m_LastShotClientId)
	{
		str_format(aBuf, sizeof(aBuf), TCLocalize("Shot %d: %s, shoot again to run command"), pBestClient->ClientId(), pBestClient->m_aName);
		GameClient()->Echo(aBuf);
		m_LastShotClientId = pBestClient->ClientId();
		m_LastShotTime = time();
		return;
	}
	str_format(aBuf, sizeof(aBuf), TCLocalize("Shot %d: %s, running command"), pBestClient->ClientId(), pBestClient->m_aName);
	GameClient()->Echo(aBuf);
	m_LastShotClientId = -1;
	class CResultModFire : public CConsole::IResult
	{
	public:
		const char *m_pBuf;
		CResultModFire(const char *pBuf) : IResult(0), m_pBuf(pBuf) {}
		int NumArguments() const
		{
			return 1;
		}
		const char *GetString(unsigned Index) const override
		{
			if(Index == 0)
				return m_pBuf;
			return "";
		}
		int GetInteger(unsigned Index) const override { return 0; };
		float GetFloat(unsigned Index) const override { return 0.0f; };
		std::optional<ColorHSLA> GetColor(unsigned Index, float DarkestLighting) const override { return std::nullopt; };
		void RemoveArgument(unsigned Index) override {};
		int GetVictim() const override { return -1; };
	};
	str_format(aBuf, sizeof(aBuf), "%d", pBestClient->ClientId());
	CResultModFire ResultModFire(aBuf);
	GameClient()->m_Conditional.m_pResult = &ResultModFire;
	Console()->ExecuteLine(g_Config.m_ClModWeaponCommand);
	GameClient()->m_Conditional.m_pResult = nullptr;
}
