/*
 * Copyright (C) 2008-2013 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TRINITY_VEHICLE_H
#define __TRINITY_VEHICLE_H

#include "ObjectDefines.h"
#include "Object.h"
#include "VehicleDefines.h"
#include "Unit.h"
#include <list>

struct VehicleEntry;
class Unit;
class VehicleJoinEvent;

typedef std::set<uint64> GuidSet;

enum EjectTargetDirectionTypes
{
    EJECT_DIR_NONE,
    EJECT_DIR_FRONT,
    EJECT_DIR_BACK,
    EJECT_DIR_RIGHT,
    EJECT_DIR_LEFT,
    EJECT_DIR_FRONT_RIGHT,
    EJECT_DIR_BACK_RIGHT,
    EJECT_DIR_BACK_LEFT,
    EJECT_DIR_FRONT_LEFT,
    EJECT_DIR_RANDOM,
    EJECT_DIR_ENTRY
};

class Vehicle : public TransportBase
{
     protected:
        friend bool Unit::CreateVehicleKit(uint32 id, uint32 creatureEntry);
        Vehicle(Unit* unit, VehicleEntry const* vehInfo, uint32 creatureEntry);

        friend void Unit::RemoveVehicleKit();
        ~Vehicle();

    public:
        void Install();
        void Uninstall();
        void Reset(bool evading = false);
        void InstallAllAccessories(bool evading);
        void ApplyAllImmunities();
        void InstallAccessory(uint32 entry, int8 seatId, bool minion, uint8 type, uint32 summonTime);   //! May be called from scripts

        Unit* GetBase() const { return _me; }
        VehicleEntry const* GetVehicleInfo() const { return _vehicleInfo; }
        uint32 GetCreatureEntry() const { return _creatureEntry; }

        bool HasEmptySeat(int8 seatId) const;
        Unit* GetPassenger(int8 seatId) const;
        SeatMap::const_iterator GetNextEmptySeat(int8 seatId, bool next) const;
        uint8 GetAvailableSeatCount() const;

        bool AddPassenger(Unit* passenger, int8 seatId = -1);
        void EjectPassenger(int8 seatId, EjectTargetDirectionTypes dir = EJECT_DIR_RIGHT, float distance = 5.0f);
        void EjectPassenger(Unit* passenger, EjectTargetDirectionTypes dir = EJECT_DIR_RIGHT, float distance = 5.0f);
        void RemovePassenger(Unit* passenger);
        void RelocatePassengers();
        void RemoveAllPassengers();
        bool IsVehicleInUse() const;

        void SetLastShootPos(Position const& pos) { _lastShootPos.Relocate(pos); }
        Position const& GetLastShootPos() const { return _lastShootPos; }

        SeatMap Seats;                                      ///< The collection of all seats on the vehicle. Including vacant ones.

        VehicleSeatEntry const* GetSeatForPassenger(Unit const* passenger) const;

        void RemovePendingEventsForPassenger(Unit* passenger);
        void RemoveAllPendingEvents();

        Position const* GetExitPosition(Unit* passenger);
        void SetExitPosition(Unit* passenger, Position const& exitPosition);

    protected:
        friend class VehicleJoinEvent;
        uint32 UsableSeatNum;                               ///< Number of seats that match VehicleSeatEntry::UsableByPlayer, used for proper display flags

    private:
        enum Status
        {
            STATUS_NONE,
            STATUS_INSTALLED,
            STATUS_UNINSTALLING,
        };

        SeatMap::iterator GetSeatIteratorForPassenger(Unit* passenger);
        void InitMovementInfoForBase();

        /// This method transforms supplied transport offsets into global coordinates
        void CalculatePassengerPosition(float& x, float& y, float& z, float& o);

        /// This method transforms supplied global coordinates into local offsets
        void CalculatePassengerOffset(float& x, float& y, float& z, float& o);

        void RemovePendingEvent(VehicleJoinEvent* e);
        void RemovePendingEventsForSeat(int8 seatId);

        bool HasPendingEventForSeat(int8 seatId) const;

    private:
        Unit* _me;                                          ///< The underlying unit with the vehicle kit. Can be player or creature.
        VehicleEntry const* _vehicleInfo;                   ///< DBC data for vehicle
        GuidSet vehiclePlayers;

        uint32 _creatureEntry;                              ///< Can be different than the entry of _me in case of players
        Status _status;                                     ///< Internal variable for sanity checks
        Position _lastShootPos;

        typedef std::list<VehicleJoinEvent*> PendingJoinEventContainer;
        PendingJoinEventContainer _pendingJoinEvents;       ///< Collection of delayed join events for prospective passengers
};

class VehicleJoinEvent : public BasicEvent
{
    friend class Vehicle;
    protected:
        VehicleJoinEvent(Vehicle* v, Unit* u) : Target(v), vehicleGuid(v->GetBase()->GetGUID()), Passenger(u), Seat(Target->Seats.end()) {}
        ~VehicleJoinEvent();
        bool Execute(uint64, uint32);
        void Abort(uint64);
        float GetSeatOrientation(uint32 seat_entry, uint32 base_entry, int8 seat_id);
        Vehicle* Target;
        uint64 vehicleGuid;
        Unit* Passenger;
        SeatMap::iterator Seat;
};

#endif
