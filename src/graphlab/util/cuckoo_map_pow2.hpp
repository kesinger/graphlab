#ifndef GRAPHLAB_UTIL_CUCKOO_MAP_POW2_HPP
#define GRAPHLAB_UTIL_CUCKOO_MAP_POW2_HPP

#include <vector>
#include <iterator>
#include <boost/random.hpp>
#include <boost/unordered_map.hpp>
#include <ctime>
namespace graphlab {



/**
 * A cuckoo hash map which requires the user to
 * provide an "illegal" value thus avoiding the need
 * for a seperate bitmap. More or less similar
 * interface as boost::unordered_map, not necessarily
 * entirely STL compliant.
 */
template <typename Key, typename Value,
          Key IllegalValue,
          size_t CuckooK = 2,
          typename IndexType = size_t,
          typename Hash = boost::hash<Key>,
          typename Pred = std::equal_to<Key> >
class cuckoo_map_pow2 {

public:
  // public typedefs
  typedef Key                                      key_type;
  typedef std::pair<Key const, Value>              value_type;
  typedef Value                                    mapped_type;
  typedef Hash                                     hasher;
  typedef Pred                                     key_equal;
  typedef IndexType                                index_type;

private:
  // internal typedefs
  typedef std::vector<std::pair<Key, Value> > map_container_type;
  typedef boost::unordered_map<Key, Value, Hash, Pred> stash_container_type;


  index_type numel;
  index_type maxstash;
  map_container_type data;
  stash_container_type stash;
  boost::rand48  drng;
  boost::uniform_int<index_type> kranddist;
  hasher hashfun;
  key_equal keyeq;
  index_type mask;
public:


  struct const_iterator {
    cuckoo_map_pow2* cmap;
    bool in_stash;
    typename cuckoo_map_pow2::map_container_type::const_iterator vec_iter;
    typename cuckoo_map_pow2::stash_container_type::const_iterator stash_iter;

    typedef std::forward_iterator_tag iterator_category;
    typedef typename cuckoo_map_pow2::value_type value_type;
    typedef size_t difference_type;
    typedef const value_type* pointer;
    typedef const value_type& reference;

    friend class cuckoo_map_pow2;

    const_iterator(): cmap(NULL), in_stash(false) {}

    const_iterator operator++() {
      if (!in_stash) {
        ++vec_iter;
        // we are in the main vector. try to advance the
        // iterator until I hit another data element
        while(vec_iter != cmap->data.end() &&
              !cmap->key_eq()(vec_iter->first, IllegalValue)) ++vec_iter;
        if (vec_iter == cmap->data.end()) {
          in_stash = true;
          stash_iter = cmap->stash.begin();
        }
      }
      else if (in_stash) {
        if (stash_iter != cmap->stash.end())  ++stash_iter;
      }
    }

    const_iterator operator++(int) {
      const_iterator cur = *this;
      ++(*this);
      return cur;
    }


    reference operator*() {
      if (!in_stash) return *vec_iter;
      else return *stash_iter;
    }

    pointer operator->() {
      if (!in_stash) return &(*vec_iter);
      else return &(*stash_iter);
    }

    bool operator==(const const_iterator iter) const {
     return in_stash == iter.in_stash &&
             (in_stash==false ?
                  vec_iter == iter.vec_iter :
                  stash_iter == iter.stash_iter);
    }

    bool operator!=(const const_iterator iter) const {
      return !((*this) == iter);
    }

    private:
    const_iterator(cuckoo_map_pow2* cmap, typename cuckoo_map_pow2::map_container_type::const_iterator vec_iter):
      cmap(cmap), in_stash(false), vec_iter(vec_iter), stash_iter(cmap->stash.begin()) { }

    const_iterator(cuckoo_map_pow2* cmap, typename cuckoo_map_pow2::stash_container_type::const_iterator stash_iter):
      cmap(cmap), in_stash(true), vec_iter(cmap->data.begin()), stash_iter(stash_iter) { }
  };


  struct iterator {
    cuckoo_map_pow2* cmap;
    bool in_stash;
    typename cuckoo_map_pow2::map_container_type::iterator vec_iter;
    typename cuckoo_map_pow2::stash_container_type::iterator stash_iter;

    typedef std::forward_iterator_tag iterator_category;
    typedef typename cuckoo_map_pow2::value_type value_type;
    typedef size_t difference_type;
    typedef value_type* pointer;
    typedef value_type& reference;

    friend class cuckoo_map_pow2;

    iterator(): cmap(NULL), in_stash(false) {}


    operator const_iterator() {
      const_iterator iter;
      iter.cmap = cmap;
      iter.in_stash = in_stash;
      iter.vec_iter = vec_iter;
      iter.stash_iter = stash_iter;
    }

    iterator operator++() {
      if (!in_stash) {
        ++vec_iter;
        // we are in the main vector. try to advance the
        // iterator until I hit another data element
        while(vec_iter != cmap->data.end() &&
              !cmap->key_eq()(vec_iter->first, IllegalValue)) ++vec_iter;
        if (vec_iter == cmap->data.end()) {
          in_stash = true;
          stash_iter = cmap->stash.begin();
        }
      }
      else if (in_stash) {
        if (stash_iter != cmap->stash.end())  ++stash_iter;
      }
    }

    iterator operator++(int) {
      iterator cur = *this;
      ++(*this);
      return cur;
    }


    reference operator*() {
      if (!in_stash) return *vec_iter;
      else return *stash_iter;
    }

    pointer operator->() {
      if (!in_stash) return reinterpret_cast<pointer>(&(*vec_iter));
      else return &(*stash_iter);
    }

    bool operator==(const iterator iter) const {
      return in_stash == iter.in_stash &&
             (in_stash==false ?
                  vec_iter == iter.vec_iter :
                  stash_iter == iter.stash_iter);
    }

    bool operator!=(const iterator iter) const {
      return !((*this) == iter);
    }


    private:
    iterator(cuckoo_map_pow2* cmap, typename cuckoo_map_pow2::map_container_type::iterator vec_iter):
      cmap(cmap), in_stash(false), vec_iter(vec_iter) { }

    iterator(cuckoo_map_pow2* cmap, typename cuckoo_map_pow2::stash_container_type::iterator stash_iter):
      cmap(cmap), in_stash(true), stash_iter(stash_iter) { }

  };


  cuckoo_map_pow2(index_type stashsize = 8,
            hasher const& h = hasher(),
            key_equal const& k = key_equal()):numel(0),maxstash(stashsize),
            data(128, std::make_pair<Key, Value>(IllegalValue, Value())),
            drng(time(NULL)),
            kranddist(0, CuckooK - 1), hashfun(h), keyeq(k), mask(127) {
    stash.max_load_factor(1.0);
  }

  index_type size() {
    return numel;
  }

  iterator begin() {
    iterator iter;
    iter.cmap = this;
    iter.in_stash = false;
    iter.vec_iter = data.begin();

    while(iter.vec_iter != data.end() &&
          !keyeq(iter.vec_iter->first, IllegalValue)) ++iter.vec_iter;
    return iter;
  }

  iterator end() {
    return iterator(this, stash.end());
  }


  const_iterator begin() const {
    const_iterator iter;
    iter.cmap = this;
    iter.in_stash = false;
    iter.vec_iter = data.begin();

    while(iter.vec_iter != data.end() &&
          !keyeq(iter.vec_iter->first, IllegalValue)) ++iter.vec_iter;
    return iter;
  }

  const_iterator end() const {
    return const_iterator(this, stash.end());

  }

  index_type compute_hash(const Key& k , uint32_t seed) const {
    //static size_t a[5] = {15485867, 217645190, 920419813, 2654435760, 6461333093};
    // from hash_combine
    return (seed ^ ((index_type)hashfun(k) + 0x9e3779b9 + (seed << 6) + (seed >> 2))) & mask;
    //uint32_t h = hashfun(k);
    //return (boost::hash_value(hashfun(k) ^ seed)) % data.size();
  }

  void rehash() {
    stash_container_type stmp;
    stmp.swap(stash);
    // effectively, stmp elements are deleted
    numel -= stmp.size();
    for (size_t i = 0;i < data.size(); ++i) {
      // if there is an element here. erase it and reinsert
      if (!keyeq(data[i].first, IllegalValue)) {
        if (count(data[i].first)) continue;
        value_type v = data[i];
        erase(iterator(this, data.begin() + i));
        insert(v);
      }
    }

    typename stash_container_type::const_iterator iter = stmp.begin();
    while(iter != stmp.end()) {
      insert(*iter);
      ++iter;
    }
  }

  static uint64_t next_powerof2(uint64_t val) {
    --val;
    val = val | (val >> 1);
    val = val | (val >> 2);
    val = val | (val >> 4);
    val = val | (val >> 8);
    val = val | (val >> 16);
    val = val | (val >> 32);
    return val + 1;
  }

  
  void reserve(size_t newlen) {
    newlen = next_powerof2(newlen);
    mask = newlen - 1;
    data.resize(newlen, std::make_pair<Key, Value>(IllegalValue, Value()));
    rehash();
  }

  std::pair<iterator, bool> insert(const value_type& v_) {
    typename map_container_type::value_type v = v_;
    if (stash.size() > maxstash) {
      // resize
      reserve(data.size() * 2);
    }

    index_type insertpos = (index_type)(-1); // tracks where the current
                                     // inserted value went
    ++numel;
    // take a random walk down the tree
    for (int i = 0;i < 1000; ++i) {
      index_type idx = compute_hash(v.first, kranddist(drng));
      // if insertpos is -1, v holds the current value. and we
      //                     are inserting it into idx
      // if insertpos is idx, we are bumping v again. and v will hold the
      //                      current value once more. so revert
      //                      insertpos to -1
      if (insertpos == (index_type)(-1)) insertpos = idx;
      else if (insertpos == idx) insertpos = (index_type)(-1);
      // there is room here
      if (keyeq(data[idx].first, IllegalValue)) {
        data[idx] = v;
        // success!
        return std::make_pair(iterator(this, data.begin() + insertpos), true);
      }
      // failed to insert!
      // try again!
      std::swap(data[idx], v);
    }
    // ok. tried and failed 100 times.
    //stick it in the stash

    typename stash_container_type::iterator stashiter = stash.insert(v).first;
    // if insertpos is -1, current value went into stash
    if (insertpos == (index_type)(-1)) {
      return std::make_pair(iterator(this, stashiter), true);
    }
    else {
      return std::make_pair(iterator(this, data.begin() + insertpos), true);
    }
  }


  iterator find(key_type const& k) {
    for (uint32_t i = 0;i < CuckooK; ++i) {
      index_type idx = compute_hash(k, i);
      if (keyeq(data[idx].first, k)) return iterator(this, data.begin() + idx);
    }
    return iterator(this, stash.find(k));
  }

  const_iterator find(key_type const& k) const {
    for (uint32_t i = 0;i < CuckooK; ++i) {
      index_type idx = compute_hash(k, i);
      if (keyeq(data[idx].first, k)) return iterator(this, data.begin() + idx);
    }
    return iterator(this, stash.find(k));
  }

  size_t count(key_type const& k) const {
    for (uint32_t i = 0;i < CuckooK; ++i) {
      index_type idx = compute_hash(k, i);
      if (keyeq(data[idx].first, k)) return true;
    }
    return stash.count(k);
  }

  void erase(iterator iter) {
    if (iter.in_stash == false) {
      if (!keyeq(iter.vec_iter->first, IllegalValue)) {
        iter.vec_iter->first = IllegalValue;
        iter.vec_iter->second = Value();
        --numel;
      }
    }
    else if (iter.stash_iter != stash.end()) {
      --numel;
      stash.erase(iter.stash_iter);
    }
  }

  void erase(key_type const& k) {
    iterator iter = find(k);
    if (iter != end()) erase(iter);
  }

  void swap(cuckoo_map_pow2& other) {
    std::swap(numel, other.numel);
    std::swap(maxstash, other.maxstash);
    std::swap(data, other.data);
    std::swap(stash, other.stash);
    std::swap(drng, other.drng);
    std::swap(kranddist, other.kranddist);
    std::swap(hashfun, other.hashfun);
    std::swap(keyeq, other.keyeq);
    std::swap(mask, other.mask);
  }

  mapped_type& operator[](const key_type& i) {
    iterator iter = find(i);
    if (iter == end()) iter = insert(std::make_pair(i, mapped_type())).first;
    return iter->second;
  }

  key_equal key_eq() const {
    return keyeq;
  }

  void clear() {
    (*this) = cuckoo_map_pow2();
  }


  float load_factor() const {
    return (float)numel / (data.size() + stash.size());
  }
};

}

#endif