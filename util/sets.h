#ifndef BASIL_SETS_H
#define BASIL_SETS_H

#include "util/defs.h"
#include "util/hash.h"

// Dynamically-sized bitset data structure.
struct bitset {
    u32* data;
    u32 size;
    u32 local;

private:
    void grow(u32 n);

public:
    bitset();
    ~bitset();
    bitset(const bitset& other);
    bitset& operator=(const bitset& other);

    bool contains(u32 n) const;
    bool insert(u32 n);
    bool erase(u32 n);
    void clear();

    struct const_iterator {
        const bitset& b;
        u32 i;

        const_iterator(const bitset& b_in, u32 i_in);
        u32 operator*() const;
        bool operator==(const const_iterator& other) const;
        bool operator!=(const const_iterator& other) const;
        const_iterator& operator--();
        const_iterator& operator++();
    };

    const_iterator begin() const;
    const_iterator end() const;
};

// Linear-searching, equality-only set for cases where hashing is undesirable and/or
// we expect only a small number of elements.
template<typename T>
struct smallset {
    vector<T> data;
    bool (*equals)(const T&, const T&);

    smallset(bool (*equals_in)(const T&, const T&) = ::equals):
        equals(equals_in) {}

    using const_iterator = const T*;
    using iterator = T*;

    const_iterator begin() const {
        return data.begin();
    }

    const_iterator end() const {
        return data.end();
    }

    iterator begin() {
        return data.begin();
    }

    iterator end() {
        return data.end();
    }

    bool contains(const T& t) const {
        for (const T& datum : data) if (equals(datum, t)) return true;
        return false;
    }

    void insert(const T& t) {
        if (!contains(t)) data.push(t);
    }

    const_iterator find(const T& t) const {
        for (const T& datum : data) if (equals(datum, t)) return &datum;
        return end();
    }

    iterator find(const T& t) {
        for (const T& datum : data) if (equals(datum, t)) return &datum;
        return end();
    }

    void erase(const T& t) {
        auto it = find(t);
        if (it != end()) {
            *it = data.back();
            data.pop();
        }
    }

    void clear() {
        data.clear();
    }

    u32 size() const {
        return data.size();
    }

    u32 capacity() const {
        return data.capacity();
    }
};

// Map equivalent for small sets.
template<typename K, typename V>
class smallmap : public smallset<pair<K, V>> {
public:
    smallmap(): smallset<pair<K, V>>(::key_equals<pair<K, V>>) {
        //
    }

    void put(const K& key, const V& value) {
        smallset<pair<K, V>>::insert({ key, value });
    }

    void erase(const K& key) {
        smallset<pair<K, V>>::erase({ key, V() });
    }

    V& operator[](const K& key) {
        pair<K, V> dummy = { key, V() };
        auto it = smallset<pair<K, V>>::find(dummy);
        if (it == smallset<pair<K, V>>::end()) {
            set<pair<K, V>>::insert(dummy);
        }
        return set<pair<K, V>>::find(dummy)->second;
    }

    const V& operator[](const K& key) const {
        pair<K, V> dummy = { key, V() };
        auto it = smallset<pair<K, V>>::find(dummy);
        if (it == smallset<pair<K, V>>::end()) {
            panic("Tried to access nonexistent map key!");
        }
        return smallset<pair<K, V>>::find(dummy)->second;
    }

    typename smallset<pair<K, V>>::const_iterator find(const K& key) const {
        return smallset<pair<K, V>>::find({ key, V() });
    }

    typename smallset<pair<K, V>>::iterator find(const K& key) {
        return smallset<pair<K, V>>::find({ key, V() });
    }

    bool contains(const K& key) const {
        return smallset<pair<K, V>>::contains({ key, V() });
    }
};

#endif