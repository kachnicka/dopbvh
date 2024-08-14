#pragma once

#include <cassert>
#include <queue>
#include <unordered_map>

#include "uid.h"

template<typename T>
class uidGenerator {
    uint32_t freeId = 0;
    std::queue<uint32_t> freeIds;

public:
    uid<T> get()
    {
        uint32_t id;
        if (!freeIds.empty()) {
            id = freeIds.front();
            freeIds.pop();
        } else {
            id = freeId++;
        }

        return uid<T>(id);
    }

    void putBack(uid<T> id)
    {
        assert(id.get() <= freeId);
        freeIds.emplace(id);
    }

    void reset()
    {
        freeId = 0;
        std::queue<uint32_t> empty;
        std::swap(freeIds, empty);
    }
};

template<typename T, typename OldIDType = uint32_t>
class uidRemapper {
    uidGenerator<T> generator;
    std::unordered_map<OldIDType, uid<T>> oldToNew;
    std::vector<OldIDType> newToOld;

public:
    bool existsOldID(OldIDType const& oldID) const
    {
        return oldToNew.count(oldID) != 0;
    }

    uid<T> getNewID(OldIDType const& oldID)
    {
        if (!existsOldID(oldID)) {
            auto const id = generator.get();
            oldToNew[oldID] = id;
            newToOld.resize(std::max(newToOld.size(), static_cast<size_t>(id.get()) + 1));
            newToOld[id.get()] = oldID;
        }
        return oldToNew[oldID];
    }

    uid<T> getNewID(OldIDType const& oldID) const
    {
        assert(existsOldID(oldID));

        return oldToNew.at(oldID);
    }

    OldIDType getOldID(uid<T> newID) const
    {
        assert(newToOld.size() > newID.get());
        assert(existsOldID(newToOld[newID.get()]));

        return newToOld[newID.get()];
    }

    void removeOldID(OldIDType const& oldID)
    {
        assert(existsOldID(oldID));
        generator.putBack(oldToNew[oldID]);
        oldToNew.erase(oldID);
    }

    void reset()
    {
        generator.reset();
        oldToNew.clear();
        newToOld.clear();
    }

    [[nodiscard]] size_t size() const
    {
        return oldToNew.size();
    }
};

template<typename T>
class uidVector {
    uidGenerator<T> generator;
    std::vector<T> data;

public:
    T& operator[](uid<T> id)
    {
        assert(data.size() > id.get());
        return data[id.get()];
    }

    T const& operator[](uid<T> id) const
    {
        assert(data.size() > id.get());
        return data[id.get()];
    }

    //    typename std::vector<T>::const_iterator cbegin() const
    //    {
    //        return data.cbegin();
    //    }
    //
    //    typename std::vector<T>::const_iterator cend() const
    //    {
    //        return data.cend();
    //    }

    typename std::vector<T>::const_iterator begin() const
    {
        return data.begin();
    }

    typename std::vector<T>::const_iterator end() const
    {
        return data.end();
    }

    [[nodiscard]] size_t size() const
    {
        return data.size();
    }

    uid<T> add()
    {
        auto const result { generator.get() };
        data.resize(std::max(data.size(), static_cast<size_t>(result.get()) + 1));
        return result;
    }

    uid<T> add(T item)
    {
        auto const result { add() };
        data[result.get()] = std::move(item);
        return result;
    }

    void remove(uid<T> id)
    {
        data[id.get()].reset();
        generator.putBack(id);
    }

    void reset()
    {
        data.clear();
        generator.reset();
    }
};

// class uint32_tIDRemapper {
//     class DefaultIDType;
//     uidGenerator<DefaultIDType> generator;
//     std::unordered_map<uint32_t, uint32_t> oldToNew;
//     std::vector<uint32_t> newToOld;
//
// public:
//     bool existsOldID(uint32_t oldID) const
//     {
//         return oldToNew.count(oldID) != 0;
//     }
//     uint32_t getNewID(uint32_t oldID)
//     {
//         if (oldToNew.count(oldID) == 0) {
//             auto const id = generator.get();
//             oldToNew[oldID] = static_cast<uint32_t>(id);
//             newToOld.resize(std::max(newToOld.size(), static_cast<size_t>(static_cast<uint32_t>(id)) + 1));
//             newToOld[static_cast<uint32_t>(id)] = oldID;
//         }
//         return oldToNew[oldID];
//     }
//
//     uint32_t getNewID(uint32_t oldID) const
//     {
//         assert(oldToNew.count(oldID) != 0);
//
//         return oldToNew.at(oldID);
//     }
//
//     uint32_t getOldID(uint32_t newID) const
//     {
//         assert(newToOld.size() > newID);
//         assert(oldToNew.count(newToOld[newID]) != 0);
//
//         return newToOld[newID];
//     }
//
//     void removeOldID(uint32_t oldID)
//     {
//         assert(oldToNew.count(oldID) != 0);
//         generator.putBack(uid<DefaultIDType>(oldToNew[oldID]));
//         oldToNew.erase(oldID);
//     }
//
//     void reset()
//     {
//         generator.reset();
//         oldToNew.clear();
//         newToOld.clear();
//     }
//
//     size_t size() const
//     {
//         return oldToNew.size();
//     }
// };
