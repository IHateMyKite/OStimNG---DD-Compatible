#include "ActorUtil.h"

#include "CompatibilityTable.h"
#include "ObjectRefUtil.h"
#include "VectorUtil.h"

#include "MCM/MCMTable.h"

namespace ActorUtil {
    void sort(std::vector<GameAPI::GameActor>& actors, std::vector<GameAPI::GameActor>& dominantActors, int playerIndex) {
        enum BondageState : uint32_t
        {
            sNone               = 0x0000,  // Non bondage state
            sHandsBound         = 0x0001,  // actors wears any kind of heavy bondage device
            sHandsBoundNoAnim   = 0x0002,  // actor wears heavy bondage device which hides arms. Because of that it can be to some extend used with normal animations
            sGaggedBlocking     = 0x0004,  // actor wears gag which block mouth
            sChastifiedGenital  = 0x0008,  // actor wears chastity belt which blocks genitals
            sChastifiedAnal     = 0x0010,  // actor wears chastity belt which blocks anal
            sChastifiedBreasts  = 0x0020,  // actor wears chastity bra which blocks breasts
            sBlindfolded        = 0x0040,  // ...
            sMittens            = 0x0080,  // ...
            sBoots              = 0x0100,  // ...
            sTotal              = 0x0200   // Last bit for looping
        };

        typedef BondageState(* GetBondageState)(RE::Actor*);
        static GetBondageState DDNGGetBondageState = nullptr;

        static HINSTANCE dllHandle = LoadLibrary(TEXT("DeviousDevices.dll"));
        if (dllHandle != NULL)
        {
            FARPROC pGetBondageState = GetProcAddress(HMODULE (dllHandle),"GetBondageState");
            DDNGGetBondageState = GetBondageState(pGetBondageState);
        }

        std::stable_sort(actors.begin(), actors.end(), [&dominantActors](GameAPI::GameActor actorA, GameAPI::GameActor actorB) {
            const BondageState loc_stateA = DDNGGetBondageState(actorA.form);
            const BondageState loc_stateB = DDNGGetBondageState(actorB.form);

            if (VectorUtil::contains(dominantActors, actorA) && !(loc_stateA & sHandsBound)) {
                if ((loc_stateB & sHandsBound) || !VectorUtil::contains(dominantActors, actorB)) {
                    return true;
                }
            } else {
                if (VectorUtil::contains(dominantActors, actorB) && !(loc_stateB & sHandsBound)) {
                    return false;
                }
            }
            return (!(loc_stateA & sHandsBound) && (loc_stateB & sHandsBound))  || // bondage check
                    (Compatibility::CompatibilityTable::hasSchlong(actorA)      &&
                    !Compatibility::CompatibilityTable::hasSchlong(actorB));
        });

        GameAPI::GameActor player = GameAPI::GameActor::getPlayer();
        int currentPlayerIndex = VectorUtil::getIndex(actors, player);
        if (currentPlayerIndex < 0) {
            return;
        }

        if (playerIndex >= 0 && playerIndex < actors.size()) {
            if (currentPlayerIndex < playerIndex) {
                while (currentPlayerIndex < playerIndex) {
                    actors[currentPlayerIndex] = actors[currentPlayerIndex + 1];
                    currentPlayerIndex++;
                }
                actors[playerIndex] = player;
            } else if (currentPlayerIndex > playerIndex) {
                while (currentPlayerIndex > playerIndex) {
                    actors[currentPlayerIndex] = actors[currentPlayerIndex - 1];
                    currentPlayerIndex--;
                }
                actors[playerIndex] = player;
            }
        } else {
            if (actors.size() == 2) {
                const BondageState loc_stateA = DDNGGetBondageState(actors[0].form);
                const BondageState loc_stateB = DDNGGetBondageState(actors[1].form);

                if (!(loc_stateA & sHandsBound) && (loc_stateB & sHandsBound)) {        // bondage check
                    actors[0] = actors[0];
                    actors[1] = actors[1];
                }
                else if ((loc_stateA & sHandsBound) && !(loc_stateB & sHandsBound)) {   // bondage check
                    actors[0] = actors[1];
                    actors[1] = actors[0];
                }
                else if (Compatibility::CompatibilityTable::hasSchlong(actors[0]) == Compatibility::CompatibilityTable::hasSchlong(actors[1])) {
                    if (MCM::MCMTable::playerAlwaysDomGay()) {
                        if (actors[1] == player) {
                            actors[1] = actors[0];
                            actors[0] = player;
                        }
                    } else if (MCM::MCMTable::playerAlwaysSubGay()) {
                        if (actors[0] == player) {
                            actors[0] = actors[1];
                            actors[1] = player;
                        }
                    }
                } else {
                    if (MCM::MCMTable::playerAlwaysDomStraight()) {
                        if (actors[1] == player) {
                            actors[1] = actors[0];
                            actors[0] = player;
                        }
                    } else if (MCM::MCMTable::playerAlwaysSubStraight()) {
                        if (actors[0] == player) {
                            actors[0] = actors[1];
                            actors[1] = player;
                        }
                    }
                }
            }
        }
    }

    void equipItem(RE::Actor* actor, RE::TESForm* item, bool preventRemoval, bool silent) {
        EquipItem(nullptr, 0, actor, item, preventRemoval, silent);
    }

    void equipItem(RE::Actor* actor, RE::TESForm* item) {
        EquipItem(nullptr, 0, actor, item, false, true);
    }

    void unequipItem(RE::Actor* actor, RE::TESForm* item, bool preventEquip, bool silent) {
        UnequipItem(nullptr, 0, actor, item, preventEquip, silent);
    }

    void unequipItem(RE::Actor* actor, RE::TESForm* item) {
        UnequipItem(nullptr, 0, actor, item, false, true);
    }

    void equipItemEx(RE::Actor* actor, RE::TESForm* item, int slotId, bool preventUnequip, bool equipSound) {
        //TODO: reimplement SKSE function here
        const auto skyrimVM = RE::SkyrimVM::GetSingleton();
        auto vm = skyrimVM ? skyrimVM->impl : nullptr;
        if (vm) {
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
            auto handle = skyrimVM->handlePolicy.GetHandleForObject(static_cast<RE::VMTypeID>(actor->FORMTYPE), actor);
            auto args = RE::MakeFunctionArguments(std::move(item), std::move(slotId), std::move(preventUnequip), std::move(equipSound));
            vm->DispatchMethodCall2(handle, "Actor", "EquipItemEx", args, callback);
        }
    }

    void equipItemEx(RE::Actor* actor, RE::TESForm* item, int slotId) {
        equipItemEx(actor, item, slotId, false, true);
    }

    void equipItemEx(RE::Actor* actor, RE::TESForm* item) {
        equipItemEx(actor, item, 0, false, true);
    }

    float getHeelOffset(RE::Actor* actor) {
        auto& weightModel = actor->GetBiped(0);
        if (weightModel) {
            std::set<RE::NiAVObject*> touched;

            for (int i = 0; i < 42; ++i) {
                auto& data = weightModel->objects[i];
                if (data.partClone) {
                    RE::TESForm* bipedArmor = data.item;

                    // only check slot 37, as too many objects cause weird crashed here and heel offsets should only be on the shoes anyways
                    if (bipedArmor->formType != RE::TESObjectARMO::FORMTYPE || !data.addon->HasPartOf(RE::BIPED_MODEL::BipedObjectSlot::kFeet)){
                        continue;
                    }

                    RE::NiAVObject* object = data.partClone.get();
                    
                    if (!touched.count(object)) {
                        auto clone = data.partClone.get();

                        RE::BSTriShape* shape = clone->AsTriShape();
                        if (shape) {
                            float offset = getHeelOffset(shape);
                            if (offset != 0) {
                                return offset;
                            }
                        }

                        RE::NiNode* node = clone->AsNode();
                        if (node) {
                            for (auto& child : node->GetChildren()) {
                                float offset = getHeelOffset(child.get());
                                if (offset != 0) {
                                    return offset;
                                }
                            }
                        }

                        touched.emplace(object);
                    }
                }
            }
        }

        return 0;
    }

    float getHeelOffset(RE::NiAVObject* object) {
        if (object->HasExtraData("HH_OFFSET")) {
            auto hh_offset = object->GetExtraData<RE::NiFloatExtraData>("HH_OFFSET");
            if (hh_offset) {
                return hh_offset->value;
            }
        } else if (object->HasExtraData("SDTA")) {
            auto sdta = object->GetExtraData<RE::NiStringExtraData>("SDTA");
            if (sdta) {
                json json = json::parse(sdta->value, nullptr, false);

                if (!json.is_discarded()) {
                    for (auto& element : json) {
                        if (element.contains("name") && element["name"] == "NPC" && element.contains("pos")) {
                            return element["pos"][2];
                        }
                    }
                }
            }
        }
        return 0;
    }
}