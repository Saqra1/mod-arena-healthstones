// SPDX-License-Identifier: GPL-2.0-or-later

#include "AllBattlegroundScript.h"
#include "Battleground.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "Player.h"
#include <array>
#include <iterator>
#include <map>

namespace
{
    enum HealthstoneSpells : uint32
    {
        SPELL_IMPROVED_HEALTHSTONE_RANK_1 = 18692,
        SPELL_IMPROVED_HEALTHSTONE_RANK_2 = 18693
    };

    struct HealthstoneRank
    {
        uint32 CreateSpell;
        std::array<uint32, 3> Items;
    };

    // Items follow spell_warl_create_healthstone: normal, Improved Healthstone 1/2.
    constexpr HealthstoneRank HealthstoneRanks[] =
    {
        {  6201, {  5512, 19004, 19005 } }, // Minor
        {  6202, {  5511, 19006, 19007 } }, // Lesser
        {  5699, {  5509, 19008, 19009 } }, // Normal
        { 11729, {  5510, 19010, 19011 } }, // Greater
        { 11730, {  9421, 19012, 19013 } }, // Major
        { 27230, { 22103, 22104, 22105 } }, // Master
        { 47871, { 36889, 36890, 36891 } }, // Demonic
        { 47878, { 36892, 36893, 36894 } }  // Fel
    };

    struct RefreshmentRank
    {
        uint32 ItemId;
        std::array<uint32, 2> CreateSpells;
    };

    // Ordered by restoration strength; spell IDs are the learned spells, not the table's use spells.
    constexpr RefreshmentRank RefreshmentRanks[] =
    {
        { 34062, { 43987,     0 } }, // Mana Biscuit: Ritual of Refreshment 1
        { 43518, { 42955,     0 } }, // Mana Pie: Conjure Refreshment 1
        { 43523, { 42956, 58659 } }  // Mana Strudel: Conjure Refreshment 2 / Ritual of Refreshment 2
    };

    constexpr uint32 RefreshmentStackSize = 20;
    using AvailableRefreshments = std::array<bool, std::size(RefreshmentRanks)>;

    struct HealthstoneChoice
    {
        uint32 ItemId = 0;
        uint32 Strength = 0;
    };

    struct Delivery
    {
        uint32 Strength = 0;
        ObjectGuid ItemGuid;
    };

    struct PlayerSupplies
    {
        Delivery Healthstone;
        uint32 RefreshmentStrength = 0;
    };

    struct TeamSupplies
    {
        HealthstoneChoice Healthstone;
        AvailableRefreshments Refreshments{};
    };

    struct ArenaPreparation
    {
        uint32 CheckTimer = IN_MILLISECONDS;
        std::map<ObjectGuid, PlayerSupplies> Deliveries;
    };

    bool IsPreparing(Battleground const* battleground)
    {
        return battleground->isArena() && battleground->GetStatus() == STATUS_WAIT_JOIN;
    }

    bool IsParticipant(Player const* player, Battleground const* battleground)
    {
        return player && player->IsInWorld() && !player->IsSpectator()
            && player->GetBattlegroundId() == battleground->GetInstanceID()
            && player->GetBgTeamId() < PVP_TEAMS_COUNT;
    }

    HealthstoneChoice GetHealthstone(Player const* warlock)
    {
        uint32 improvement = 0;
        if (warlock->HasAura(SPELL_IMPROVED_HEALTHSTONE_RANK_2))
            improvement = 2;
        else if (warlock->HasAura(SPELL_IMPROVED_HEALTHSTONE_RANK_1))
            improvement = 1;

        for (std::size_t rank = std::size(HealthstoneRanks); rank > 0; --rank)
        {
            HealthstoneRank const& healthstone = HealthstoneRanks[rank - 1];
            if (warlock->HasActiveSpell(healthstone.CreateSpell))
                return { healthstone.Items[improvement], static_cast<uint32>(rank * 3 + improvement) };
        }

        return {};
    }

    void DeliverHealthstone(Player* player, HealthstoneChoice const& healthstone, Delivery& delivery)
    {
        // A successful delivery is not replenished if the player consumes, trades or deletes it.
        if (!healthstone.ItemId || delivery.Strength >= healthstone.Strength)
            return;

        if (player->GetItemByEntry(healthstone.ItemId))
        {
            delivery.Strength = healthstone.Strength;
            delivery.ItemGuid.Clear();
            return;
        }

        Item* item = nullptr;
        ItemPosCountVec destinations;
        if (Item* previous = player->GetItemByGuid(delivery.ItemGuid))
        {
            if (previous->IsInTrade())
                return;

            // Exclude the old stone from the shared carry limit and reuse its inventory slot.
            if (player->CanStoreItem(previous->GetBagSlot(), previous->GetSlot(), destinations,
                healthstone.ItemId, 1, previous) != EQUIP_ERR_OK)
                return;

            item = Item::CreateItem(healthstone.ItemId, 1, player);
            if (!item)
                return;

            player->DestroyItem(previous->GetBagSlot(), previous->GetSlot(), true);
            item = player->StoreItem(destinations, item, true);
            player->ItemAddedQuestCheck(healthstone.ItemId, 1);
        }
        else
        {
            if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, destinations, healthstone.ItemId, 1) != EQUIP_ERR_OK)
                return;

            item = player->StoreNewItem(destinations, healthstone.ItemId, true);
        }

        if (!item)
            return;

        player->SendNewItem(item, 1, true, false);
        delivery.Strength = healthstone.Strength;
        delivery.ItemGuid = item->GetGUID();
    }

    void DeliverRefreshments(Player* player, AvailableRefreshments const& available, uint32& deliveredStrength)
    {
        for (std::size_t rank = std::size(RefreshmentRanks); rank > 0; --rank)
        {
            if (!available[rank - 1])
                continue;

            uint32 const itemId = RefreshmentRanks[rank - 1].ItemId;
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
            if (!itemTemplate || player->CanUseItem(itemTemplate) != EQUIP_ERR_OK)
                continue;

            if (deliveredStrength >= rank)
                return;

            uint32 const existingCount = player->GetItemCount(itemId);
            if (existingCount < RefreshmentStackSize)
            {
                uint32 const count = RefreshmentStackSize - existingCount;
                ItemPosCountVec destinations;
                if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, destinations, itemId, count) != EQUIP_ERR_OK)
                    return;

                Item* item = player->StoreNewItem(destinations, itemId, true);
                if (!item)
                    return;

                player->SendNewItem(item, count, true, false);
            }

            deliveredStrength = static_cast<uint32>(rank);
            return;
        }
    }

    void DistributeSupplies(Battleground* battleground, ArenaPreparation& preparation)
    {
        std::array<TeamSupplies, PVP_TEAMS_COUNT> teams{};
        for (auto const& [guid, player] : battleground->GetPlayers())
        {
            if (!IsParticipant(player, battleground))
                continue;

            TeamSupplies& team = teams[player->GetBgTeamId()];
            if (player->getClass() == CLASS_WARLOCK)
            {
                HealthstoneChoice const healthstone = GetHealthstone(player);
                if (healthstone.Strength > team.Healthstone.Strength)
                    team.Healthstone = healthstone;
            }
            else if (player->getClass() == CLASS_MAGE)
            {
                // Lower learned ranks remain available when a teammate cannot use the highest one.
                for (std::size_t rank = 0; rank < std::size(RefreshmentRanks); ++rank)
                    for (uint32 spellId : RefreshmentRanks[rank].CreateSpells)
                        if (spellId && player->HasSpell(spellId))
                            team.Refreshments[rank] = true;
            }
        }

        for (auto const& [guid, player] : battleground->GetPlayers())
        {
            if (IsParticipant(player, battleground))
            {
                TeamSupplies const& team = teams[player->GetBgTeamId()];
                PlayerSupplies& supplies = preparation.Deliveries[guid];
                DeliverHealthstone(player, team.Healthstone, supplies.Healthstone);
                DeliverRefreshments(player, team.Refreshments, supplies.RefreshmentStrength);
            }
        }
    }
}

class ArenaHealthstones : public AllBattlegroundScript
{
public:
    ArenaHealthstones() : AllBattlegroundScript("ArenaHealthstones", {
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_ADD_PLAYER,
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_UPDATE,
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_REMOVE_PLAYER_AT_LEAVE,
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_DESTROY
    }) { }

    void OnBattlegroundAddPlayer(Battleground* battleground, Player* /*player*/) override
    {
        if (IsPreparing(battleground))
            DistributeSupplies(battleground, _preparations[battleground->GetInstanceID()]);
    }

    void OnBattlegroundUpdate(Battleground* battleground, uint32 diff) override
    {
        if (!IsPreparing(battleground))
        {
            _preparations.erase(battleground->GetInstanceID());
            return;
        }

        ArenaPreparation& preparation = _preparations[battleground->GetInstanceID()];
        if (preparation.CheckTimer > diff)
        {
            preparation.CheckTimer -= diff;
            return;
        }

        preparation.CheckTimer = IN_MILLISECONDS;
        DistributeSupplies(battleground, preparation);
    }

    void OnBattlegroundRemovePlayerAtLeave(Battleground* battleground, Player* player) override
    {
        auto const itr = _preparations.find(battleground->GetInstanceID());
        if (itr != _preparations.end())
            itr->second.Deliveries.erase(player->GetGUID());
    }

    void OnBattlegroundDestroy(Battleground* battleground) override
    {
        _preparations.erase(battleground->GetInstanceID());
    }

private:
    std::map<uint32, ArenaPreparation> _preparations;
};

void AddArenaHealthstonesScripts()
{
    new ArenaHealthstones();
}
