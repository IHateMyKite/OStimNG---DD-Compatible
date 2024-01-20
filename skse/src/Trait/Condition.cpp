#include "Condition.h"

#include "TraitTable.h"

#include "Core/ThreadActor.h"
#include "Core/ThreadManager.h"
#include "Graph/Requirement.h"
#include "MCM/MCMTable.h"
#include "Util/CompatibilityTable.h"

namespace Trait {
    ActorCondition ActorCondition::create(GameAPI::GameActor actor) {
        return create(actor, OStim::ThreadManager::GetSingleton()->findActor(actor));
    }

    ActorCondition ActorCondition::create(OStim::ThreadActor* actor) {
        return create(actor->getActor(), actor);
    }

    ActorCondition ActorCondition::create(GameAPI::GameActor actor, OStim::ThreadActor* threadActor) {
        ActorCondition condition;

        condition.type = TraitTable::getActorType(actor);

        // nullptr needs to meet all conditions, it's important for some add-ons
        if (!actor) {
            condition.requirements = ~0;
            return condition;
        }

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

        BondageState loc_state = DDNGGetBondageState(actor.form);

        GameAPI::GameSex sex = actor.getSex();
        switch (sex) {
            case GameAPI::GameSex::FEMALE:
                if (MCM::MCMTable::futaUseMaleRole() && Compatibility::CompatibilityTable::hasSchlong(actor)) {
                    condition.sex = GameAPI::GameSex::AGENDER; // despite the name this means both, not none
                } else {
                    condition.sex = GameAPI::GameSex::FEMALE;
                }
                break;
            default:
                condition.sex = sex;
            break;
        }

        if (!(loc_state & sChastifiedAnal)) condition.requirements |= Graph::Requirement::ANUS;

        if (actor.isSex(GameAPI::GameSex::FEMALE)) {
            if (!(loc_state & sChastifiedBreasts)) condition.requirements |= Graph::Requirement::BREAST;
            if (MCM::MCMTable::unequipStrapOnIfNotNeeded() || MCM::MCMTable::unequipStrapOnIfInWay() || !threadActor || !threadActor->isObjectEquipped("strapon")) {
                if (!(loc_state & sChastifiedGenital)) condition.requirements |= Graph::Requirement::VAGINA;
            }
        }

        condition.requirements |= Graph::Requirement::FOOT;
        if (!(loc_state & sHandsBoundNoAnim) && !(loc_state & sMittens))    condition.requirements |= Graph::Requirement::HAND;
        if (!(loc_state & sGaggedBlocking))                                 condition.requirements |= Graph::Requirement::MOUTH;
        if (!(loc_state & sChastifiedBreasts))                              condition.requirements |= Graph::Requirement::NIPPLE;

        bool hasSchlong = Compatibility::CompatibilityTable::hasSchlong(actor);
        if (hasSchlong) {
            if (!(loc_state & sChastifiedGenital)) condition.requirements |= Graph::Requirement::PENIS;
            if (!(loc_state & sChastifiedGenital)) condition.requirements |= Graph::Requirement::TESTICLES;
        } else {
            if (MCM::MCMTable::equipStrapOnIfNeeded() || threadActor && threadActor->isObjectEquipped("strapon")) {
                if (!(loc_state & sChastifiedGenital)) condition.requirements |= Graph::Requirement::PENIS;
            }
        }

        if (actor.isVampire()) {
            condition.requirements |= Graph::Requirement::VAMPIRE;
        }

        return condition;
    }

    std::vector<ActorCondition> ActorCondition::create(std::vector<GameAPI::GameActor> actors) {
        std::vector<ActorCondition> ret;
        for (auto& actor : actors) {
            ret.push_back(create(actor));
        }
        return ret;
    }

    bool ActorCondition::fulfills(ActorCondition other) {
        if (type != other.type) {
            return false;
        }

        if (MCM::MCMTable::unrestrictedNavigation()) {
            return true;
        }

        if (MCM::MCMTable::intendedSexOnly() && sex != GameAPI::GameSex::AGENDER && other.sex != GameAPI::GameSex::AGENDER && sex != other.sex) {
            return false;
        }

        if ((requirements & other.requirements) != other.requirements) {
            return false;
        }
        
        return true;
    }
}