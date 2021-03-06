#pragma once

#include <iostream>
#include <algorithm>
#include <vector>
#include "multi_idx/perm.hpp"
#include "sdsl/io.hpp"
#include "sdsl/int_vector.hpp"
#include "sdsl/sd_vector.hpp"
#include "sdsl/bit_vectors.hpp"

namespace multi_index {
  
template<uint8_t t_b=7,
         uint8_t t_k=3,
         size_t  t_id=0,
         typename perm_b_k=perm<t_b,t_b-t_k>
         >
class _simple_buckets_binsearch {
public:
    typedef uint64_t          size_type;
    typedef uint64_t          entry_type;
    typedef perm_b_k perm;
    enum {id = t_id};


private:        
    static constexpr uint8_t init_splitter_bits(size_t i=0){
        return i < perm_b_k::match_len ? perm_b_k::mi_permute_block_widths[t_id][t_b-1-i] + init_splitter_bits(i+1) : 0;
    }
    uint64_t              m_n;      // number of items
    sdsl::int_vector<64>  m_entries; 

public:
    static constexpr uint8_t splitter_bits = init_splitter_bits();

    _simple_buckets_binsearch() = default;

    _simple_buckets_binsearch(const std::vector<entry_type> &input_entries) {
        std::cout << "Splitter bits " << (uint16_t) splitter_bits << std::endl; 

        m_n = input_entries.size();
        sdsl::int_vector<64>(input_entries.size(), 0).swap(m_entries);
        
        if(splitter_bits <= 28) build_small_universe(input_entries);
        else build_large_universe(input_entries);
    }

    inline std::pair<std::vector<uint64_t>, uint64_t> match(const entry_type q, uint8_t errors=t_k, const bool find_only_candidates=false) {
        uint64_t bucket = get_bucket_id(q);

        auto begin = std::lower_bound(m_entries.begin(), 
                                      m_entries.end(), 
                                      bucket,
                                      [&](const entry_type &a, const entry_type &bucket) {
                                            return get_bucket_id(a) < bucket;}
                     );
        auto end = std::upper_bound(begin, 
                                    m_entries.end(), 
                                    bucket,
                                    [&](const entry_type &bucket, const entry_type &b) {
                                            return bucket < get_bucket_id(b);}
                    );

        uint64_t candidates = std::distance(begin,end);
        std::vector<entry_type> res;
        
        if (find_only_candidates) return {res, candidates};
        if (errors >= 6) res.reserve(128);

        for (auto it = begin; it != end; ++it) {
          if (sdsl::bits::cnt(q^*it) <= errors) {
            res.push_back(*it);
          }
        }
        return {res, candidates};
    }

    _simple_buckets_binsearch& operator=(const _simple_buckets_binsearch& idx) {
        if ( this != &idx ) {
            m_n       = std::move(idx.m_n);
            m_entries   = std::move(idx.m_entries);
        }
        return *this;
    }

    _simple_buckets_binsearch& operator=(_simple_buckets_binsearch&& idx) {
        if ( this != &idx ) {
            m_n       = std::move(idx.m_n);
            m_entries   = std::move(idx.m_entries);
        }
        return *this;
    }

    _simple_buckets_binsearch(const _simple_buckets_binsearch& idx) {
        *this = idx;
    }

    _simple_buckets_binsearch(_simple_buckets_binsearch&& idx){
        *this = std::move(idx);
    }

    //! Serializes the data structure into the given ostream
    size_type serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr, std::string name="")const {
        using namespace sdsl;
        structure_tree_node* child = structure_tree::add_child(v, name, util::class_name(*this));
        uint64_t written_bytes = 0;
        written_bytes += write_member(m_n, out, child, "n");
        written_bytes += m_entries.serialize(out, child, "entries");      
        structure_tree::add_size(child, written_bytes);
        return written_bytes;
    }

    //! Loads the data structure from the given istream.
    void load(std::istream& in) {
        using namespace sdsl;
        read_member(m_n, in);
        m_entries.load(in);
    }

    size_type size() const{
        return m_n;
    }

private:

  inline uint64_t get_bucket_id(const uint64_t x) const {
      return perm_b_k::mi_permute[t_id](x) >> (64-splitter_bits);
  }


void build_large_universe(const std::vector<entry_type> &input_entries) {
    std::copy(input_entries.begin(), input_entries.end(), m_entries.begin());   
    std::cout << "Start sorting\n";
    std::sort(m_entries.begin(), m_entries.end(), 
        [&](const entry_type &a, const entry_type &b) {
            return get_bucket_id(a) < get_bucket_id(b);
    });
    std::cout << "End sorting\n";   
}

void build_small_universe(const std::vector<entry_type> &input_entries) {
    // countingSort-like strategy to order entries accordingly to bucket_id
    uint64_t splitter_universe = ((uint64_t) 1) << (splitter_bits);
    std::vector<uint64_t> prefix_sums(splitter_universe + 1, 0); // includes a sentinel
    for (auto x : input_entries) {
        prefix_sums[get_bucket_id(x)]++;
    }

    uint64_t sum = prefix_sums[0];
    prefix_sums[0] = 0;
    for(uint64_t i = 1; i < prefix_sums.size(); ++i) {
        uint64_t curr = prefix_sums[i];
        prefix_sums[i] = sum; 
        sum += curr;
    }
    for (auto x : input_entries) {
        uint64_t bucket = get_bucket_id(x);
        m_entries[prefix_sums[bucket]] = x; 
        prefix_sums[bucket]++;
    }
}

};

struct simple_buckets_binsearch {
    template<uint8_t t_b, uint8_t t_k, uint8_t t_id, typename t_perm>
    using type = _simple_buckets_binsearch<t_b, t_k, t_id, t_perm>;
};

}
