/*
 * Voxville personal loot. Released under GNU AGPL v3, matching AzerothCore.
 */

#include "Config.h"
#include "Creature.h"
#include "DBCStores.h"
#include "GameObject.h"
#include "Group.h"
#include "Log.h"
#include "LootMgr.h"
#include "Map.h"
#include "MiscScript.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PersonalLootPersistence.h"
#include "PlayerScript.h"
#include "ScriptMgr.h"
#include "TC9Sidecar.h"
#include "WorldScript.h"
#include "WorldSession.h"
#include <mutex>
#include <algorithm>
#include <limits>

#ifdef MOD_PLAYERBOTS
#include "PlayerbotMgr.h"
#endif

namespace
{
    struct Settings
    {
        bool enabled{false};
        bool experimental{false};
        bool enableForBots{false};
        bool bossChests{true};
        bool worldChests{false};
        bool areaLoot{false};
        bool personalGold{false};
        float areaLootRange{30.0f};
    };

    Settings settings;
    std::mutex settingsMutex;

    class PersonalLootWorldScript : public WorldScript
    {
    public:
        PersonalLootWorldScript() : WorldScript("PersonalLootWorldScript",
            {WORLDHOOK_ON_AFTER_CONFIG_LOAD, WORLDHOOK_ON_STARTUP, WORLDHOOK_ON_UPDATE}) { }

        void OnAfterConfigLoad(bool reload) override
        {
            PersonalLootPersistence::ConfigureRecovery(
                sConfigMgr->GetOption<uint32>("PersonalLoot.RecoveryMode", 1),
                sConfigMgr->GetOption<uint32>("PersonalLoot.RecoveryMinQuality", 3),
                sConfigMgr->GetOption<bool>("PersonalLoot.RecoveryGold", true),
                sConfigMgr->GetOption<bool>("PersonalLoot.RecoveryInstancesOnly", false));
            bool journal = sConfigMgr->GetOption<bool>("PersonalLoot.GenerationJournal", false);
            if (!reload)
                Loot::SetGenerationJournalEnabled(journal);
            else if (journal != Loot::GenerationJournalEnabled())
                LOG_ERROR("module.personal_loot", "GenerationJournal requires a server restart; reload ignored.");
            Settings next;
            next.enabled = sConfigMgr->GetOption<bool>("PersonalLoot.Enable", false);
            next.experimental = sConfigMgr->GetOption<bool>("PersonalLoot.AllowExperimental", false);
            next.enableForBots = sConfigMgr->GetOption<bool>("PersonalLoot.EnableForPlayerBots", false);
            next.bossChests = sConfigMgr->GetOption<bool>("PersonalLoot.BossChests", true);
            next.worldChests = sConfigMgr->GetOption<bool>("PersonalLoot.WorldChests", false);
            next.areaLoot = sConfigMgr->GetOption<bool>("PersonalLoot.AreaLoot", false);
            next.personalGold = sConfigMgr->GetOption<bool>("PersonalLoot.PersonalGold", false);
            next.areaLootRange = std::clamp(
                sConfigMgr->GetOption<float>("PersonalLoot.AreaLootRange", 30.0f), 5.0f, 50.0f);

            std::lock_guard<std::mutex> guard(settingsMutex);
            if (reload && Loot::GenerationJournalEnabled())
            {
                if (!next.enabled || !next.experimental || !next.personalGold ||
                    next.worldChests != settings.worldChests || next.bossChests != settings.bossChests)
                    LOG_ERROR("module.personal_loot", "Persistence ownership settings require restart; reload ignored.");
                next.enabled = settings.enabled;
                next.experimental = settings.experimental;
                next.personalGold = settings.personalGold;
                next.bossChests = settings.bossChests;
                next.worldChests = settings.worldChests;
            }
            settings = std::move(next);
            LOG_INFO("module.personal_loot", "Personal loot: enabled={}, experimental={}, bots={}, "
                "bossChests={}, worldChests={}", settings.enabled, settings.experimental, settings.enableForBots,
                settings.bossChests, settings.worldChests);
            if (settings.enabled && !Loot::GenerationJournalEnabled())
                LOG_WARN("module.personal_loot", "Prototype only: unclaimed personal loot is not crash-persistent.");
            LOG_INFO("module.personal_loot", "Personal area loot: enabled={}, range={}, personalGold={}",
                settings.areaLoot, settings.areaLootRange, settings.personalGold);
        }

        void OnStartup() override
        {
            if (Loot::GenerationJournalEnabled())
            {
                if (sToCloud9Sidecar->ClusterModeEnabled() || sToCloud9Sidecar->IsCrossrealm())
                    throw std::runtime_error("Personal loot persistence requires a single-server character database");
                if (!settings.enabled || !settings.experimental || !settings.personalGold ||
                    sConfigMgr->GetOption<uint32>("CharacterDatabase.WorkerThreads", 1) != 1)
                    throw std::runtime_error("Personal loot persistence requires enabled personal gold and one character DB worker");
                PersonalLootPersistence::RecoverStartup();
                LOG_INFO("module.personal_loot", "Durable personal loot and automatic recovery mail are enabled.");
            }
        }

        void OnUpdate(uint32 diff) override
        {
            if (recoveryTimer > diff)
            {
                recoveryTimer -= diff;
                return;
            }
            recoveryTimer = 15000;
            PersonalLootPersistence::RecoverOffline();
        }

    private:
        uint32 recoveryTimer{15000};
    };

    class PersonalLootRecovery : public PlayerScript
    {
    public:
        PersonalLootRecovery() : PlayerScript("PersonalLootRecovery", {PLAYERHOOK_ON_UPDATE, PLAYERHOOK_ON_LOGOUT}) { }
        void OnPlayerUpdate(Player* player, uint32 diff) override { player->UpdateJournalRecovery(diff); }
        void OnPlayerLogout(Player* player) override { PersonalLootPersistence::ForgetPlayer(player); }
    };

    class PersonalAreaLoot : public PlayerScript
    {
    public:
        PersonalAreaLoot() : PlayerScript("PersonalAreaLoot", {PLAYERHOOK_ON_BEFORE_SEND_LOOT}) { }

        void OnPlayerBeforeSendLoot(Player* player, ObjectGuid guid, Loot* loot) override
        {
            Settings current;
            {
                std::lock_guard<std::mutex> guard(settingsMutex);
                current = settings;
            }
            if (!current.enabled || !current.experimental || !current.areaLoot ||
                player->HasPlayerFlag(PLAYER_FLAGS_NO_PLAY_TIME) || loot->loot_type != LOOT_CORPSE)
                return;

            Creature* clicked = player->GetMap()->GetCreature(guid);
            if (!clicked || clicked->IsAlive() || !clicked->loot.HasPersonalLoot() ||
                clicked->loot.GetPersonalLoot(player->GetGUID()) != loot)
                return;

            std::list<Creature*> corpses;
            player->GetDeadCreatureListInGrid(corpses, current.areaLootRange);
            uint32 processed = 0;
            for (Creature* corpse : corpses)
            {
                if (corpse == clicked || corpse->IsAlive() || !corpse->InSamePhase(player) ||
                    !corpse->IsWithinDistInMap(player, current.areaLootRange) ||
                    !corpse->HasDynamicFlag(UNIT_DYNFLAG_LOOTABLE) || !corpse->loot.HasPersonalLoot() ||
                    (corpse->loot.loot_type != LOOT_NONE && corpse->loot.loot_type != LOOT_CORPSE) ||
                    !player->isAllowedToLoot(corpse) ||
                    !sScriptMgr->OnAllowedToLootContainerCheck(player, corpse->GetGUID()))
                    continue;
                if (++processed > 50)
                    break;

                Loot* personal = corpse->loot.GetPersonalLoot(player->GetGUID());
                if (!personal)
                    continue;
                if (!corpse->loot.JournalPersonalLoot(corpse, corpse->GetLootMode()))
                    continue;
                personal = corpse->loot.GetPersonalLoot(player->GetGUID());
                if (!personal)
                    continue;
                personal->loot_type = LOOT_CORPSE;

                // Use the original source for item hooks, achievements and eligibility.
                // Never move rows between containers or regenerate a corpse's results.
                player->SetLootGUID(corpse->GetGUID());
                uint32 slots = personal->GetMaxSlotInLootFor(player);
                for (uint32 slot = 0; slot < slots; ++slot)
                {
                    LootItem* item = personal->LootItemInSlot(slot, player);
                    if (!item || !item->AllowedForPlayer(player, corpse->GetGUID()))
                        continue;
                    InventoryResult result;
                    player->StoreLootItem(static_cast<uint8>(slot), personal, result);
                    if (result != EQUIP_ERR_OK)
                        break;
                }
                player->SetLootGUID(guid);

                // Consolidate existing gold; its configured distribution happens
                // when the player clicks money in the selected corpse's loot window.
                Loot* sourceMoney = corpse->loot.HasPersonalMoney() ? personal : &corpse->loot;
                Loot* targetMoney = clicked->loot.HasPersonalMoney() ? loot : &clicked->loot;
                if (sourceMoney->IsJournalManaged())
                {
                    uint32 amount = sourceMoney->gold;
                    if (player->HasPlayerFlag(PLAYER_FLAGS_PARTIAL_PLAY_TIME))
                        amount /= 2;
                    if (sourceMoney->gold && player->ClaimJournalGold(sourceMoney, amount))
                    {
                        player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, amount);
                        WorldPacket packet(SMSG_LOOT_MONEY_NOTIFY, 5);
                        packet << amount << uint8(1);
                        player->SendDirectMessage(&packet);
                        sourceMoney->gold = 0;
                        sourceMoney->NotifyMoneyRemoved();
                    }
                }
                else if (corpse->loot.HasPersonalMoney() == clicked->loot.HasPersonalMoney() &&
                    sourceMoney->gold <= std::numeric_limits<uint32>::max() - targetMoney->gold)
                {
                    targetMoney->gold += sourceMoney->gold;
                    sourceMoney->gold = 0;
                    sourceMoney->NotifyMoneyRemoved();
                }

                // A personal leaf being empty never authorizes deleting other leaves.
                if (corpse->loot.isLooted())
                {
                    corpse->AllLootRemovedFromCorpse();
                    corpse->RemoveDynamicFlag(UNIT_DYNFLAG_LOOTABLE);
                    corpse->loot.clear();
                }
            }
        }
    };

    class PersonalLootRecipients : public MiscScript
    {
    public:
        PersonalLootRecipients() : MiscScript("PersonalLootRecipients",
            {MISCHOOK_ON_SELECT_PERSONAL_LOOT_RECIPIENTS}) { }

        void OnSelectPersonalLootRecipients(WorldObject* source, Player* owner,
            LootStore const& store, std::vector<Player*>& recipients) override
        {
            if (!source || !owner)
                return;

            std::lock_guard<std::mutex> guard(settingsMutex);
            if (!settings.enabled || !settings.experimental)
                return;

            Creature* creature = source->ToCreature();
            GameObject* chest = source->ToGameObject();
            bool creatureLoot = creature && &store == &LootTemplates_Creature;
            bool encounterChest = chest && settings.bossChests && &store == &LootTemplates_Gameobject &&
                chest->GetGoType() == GAMEOBJECT_TYPE_CHEST && chest->HasLootRecipient() &&
                !chest->GetAllowedLooters().empty();
            bool worldChest = chest && settings.worldChests && &store == &LootTemplates_Gameobject &&
                chest->GetGoType() == GAMEOBJECT_TYPE_CHEST && !chest->HasLootRecipient() &&
                !source->GetMap()->Instanceable() && !chest->GetGOInfo()->chest.questId;
            if (worldChest)
            {
                // Gathering nodes also use CHEST; preserve their stock profession checks and loot.
                if (auto lock = sLockStore.LookupEntry(chest->GetGOInfo()->chest.lockId))
                    for (uint32 index = 0; index < MAX_LOCK_CASE; ++index)
                        if (lock->Type[index] == LOCK_KEY_SKILL &&
                            (lock->Index[index] == LOCKTYPE_HERBALISM || lock->Index[index] == LOCKTYPE_MINING ||
                                lock->Index[index] == LOCKTYPE_FISHING))
                            worldChest = false;
            }
            bool chestLoot = encounterChest || worldChest;
            if (!creatureLoot && !chestLoot)
                return;

            (creatureLoot ? creature->loot : chest->loot).SetPersonalMoney(settings.personalGold);

            auto eligible = [&](Player* player)
            {
                if (!player || !player->GetSession() || player->IsGameMaster() ||
                    player->GetMap() != source->GetMap() || (creatureLoot && !player->IsAtLootRewardDistance(source)) ||
                    player->IsSpectator() || player->HasPendingBind())
                    return;
                if (worldChest && (!player->IsAtLootRewardDistance(source) || !source->InSamePhase(player)))
                    return;

                if (!source->GetAllowedLooters().empty() && !source->HasAllowedLooter(player->GetGUID()))
                    return;

#ifdef MOD_PLAYERBOTS
                if (!settings.enableForBots && sPlayerbotsMgr.GetPlayerbotAI(player))
                    return;
#endif

                recipients.push_back(player);
            };

            if (encounterChest)
            {
                // Core SetLootRecipient snapshots these GUIDs at encounter reward time.
                // Never use the first opener's current group to add chest recipients.
                for (ObjectGuid guid : chest->GetAllowedLooters())
                    eligible(ObjectAccessor::FindConnectedPlayer(guid));
            }
            else if (Group* group = worldChest ? owner->GetGroup() : creature->GetLootRecipientGroup())
                for (GroupReference* member = group->GetFirstMember(); member; member = member->next())
                    eligible(member->GetSource());
            else
                eligible(worldChest ? owner : creature->GetLootRecipient());
        }
    };
}

void Addmod_personal_lootScripts()
{
    new PersonalLootWorldScript();
    new PersonalLootRecipients();
    new PersonalAreaLoot();
    new PersonalLootRecovery();
}
