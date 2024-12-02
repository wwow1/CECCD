#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <bitset>
#include <assert.h>
#include <cmath>

#include "countingBloomFilter.hpp"

namespace elastic_rose
{
    class Rosetta
    {
    public:

        std::vector<u64> allocateSpace(double total_size, double beta, int n) {
            std::vector<u64> layers(n);
            
            // 如果 beta 是 1，则每层空间平均分配
            if (beta == 1.0) {
                double each_layer_size = total_size / n;
                std::fill(layers.begin(), layers.end(), each_layer_size);
            } else {
                // 计算第一层的空间大小 s1
                double s1 = total_size * (1 - beta) / (1 - std::pow(beta, n));
                
                // 计算每一层的空间
                for (int i = levels_ - 1; i >= 0; --i) {
                    u64 l = levels_ - i - 1;
                    layers[i] = std::max(min_size_, u64(s1 * std::pow(beta, l)));
                }
            }
            return layers;
        }

        Rosetta(){};
        // 默认alpha能被64整除, beta < 1, p是预期的假阳性率
        Rosetta(u32 total_size, u32 alpha, double beta, double false_positive)
         : alpha_(alpha), beta_(beta), expected_false_positive_(false_positive)
        {
            
            levels_ = 64 / alpha;
            // std::cout << "levels:" << levels_ << std::endl;
            bfs = std::vector<CountingBloomFilter *>(levels_);
            auto alloc = allocateSpace(total_size, beta, levels_);
            // double pre_time1, pre_time2, pre_time = 0, build_time = 0;
            for (int i = levels_ - 1; i >= 0; --i)
            {
                std::cout << "total_size " << alloc[i] << " expected_false_positive_ " << expected_false_positive_ << std::endl;
                bfs[i] = new CountingBloomFilter(alloc[i], expected_false_positive_, i);
            }

            // std::cout << "pre_time:" << pre_time << std::endl;
            // std::cout << "bloom_build_time:" << build_time << std::endl;
        }
        ~Rosetta()
        {
            for (auto bf : bfs)
                delete bf;
        }

        bool lookupKey(const u64 &key)
        {
            return bfs[levels_ - 1]->KeyMayMatch(key);
        }

        // bool lookupKey(const std::string &key);

        void insertKey(u64 key)
        {
            u64 base = pow(2, alpha_) - 1;
            u64 last = 0;
            for (u32 i = 0; i < levels_; ++i)
            {
                u64 mask = last + (base << (alpha_ * (levels_ - i - 1)));
                u64 ik = key & mask;
                bfs[i]->PutKey(ik);
                last = mask;
            }
        }
        // void insertKey(std::string key);

        void DeleteKey(u64 key)
        {
            u64 base = pow(2, alpha_) - 1;
            u64 last = 0;
            for (u32 i = 0; i < levels_; ++i)
            {
                u64 mask = last + (base << (alpha_ * (levels_ - i - 1)));
                u64 ik = key & mask;
                bfs[i]->DeleteKey(ik);
                last = mask;
            }
        }

        bool range_query(u64 low, u64 high);
        bool range_query(u64 low, u64 high, u64 p, u64 l);
        // bool range_query(const std::string &low, const std::string &high);
        // bool range_query(const std::string &low, const std::string &high, std::string &p, u64 l, std::string &min_accept);



        // u64 seek(const u64 &key);
        // std::string seek(const std::string &key);
        u32 getLevels() const { return levels_; }

    private:
        std::vector<CountingBloomFilter *> bfs;
        u32 levels_;
        u32 alpha_;   // 相邻层之间的位差
        double beta_; // 相邻层之间的空间差异
        u64 min_size_ = 1024;
        double expected_false_positive_;
        u64 R_;

        bool doubt(u64 cur, u64 next, u64 l);
        bool doubt(std::string &p, u64 l, std::string &min_accept);

        std::string str2BitArray(const std::string &str)
        {
            std::string ret = "";
            for (auto c : str)
                for (int i = 7; i >= 0; --i)
                    ret += (((c >> i) & 1) ? '1' : '0');

            // format str size
            while (ret.size() < levels_)
                ret += '0';

            return ret;
        }

        std::string bitArray2Str(std::string &str)
        {
            std::string ret = "";
            size_t size = str.size();
            for (size_t i = 0; i < size; i = i + 8)
            {
                bitset<8> bit(str.substr(i, 8));
                int tmp = bit.to_ullong();
                if (tmp != 0)
                    ret += static_cast<char>(tmp);
            }
            return ret;
        }

        std::string maxBitArray()
        {
            std::string ret = "";
            while (ret.size() < levels_)
                ret += '1';

            return ret;
        }
    };
    // void Rosetta::insertKey(std::string key)
    // {
    //     key = str2BitArray(key);
    //     for (u32 i = 0; i < levels_; ++i)
    //         bfs[i]->add(BloomHash(key.substr(0, i + 1)));
    // }

    // bool Rosetta::lookupKey(const std::string &key)
    // {
    //     // std::cout << str2BitArray(key) << std::endl;
    //     // std::string bit_key = str2BitArray(key);
    //     return bfs[levels_ - 1]->KeyMayMatch(key);
    // }

    // bool Rosetta::range_query(const std::string &low, const std::string &high)
    // {
    //     std::string p(levels_, '0');
    //     std::string tmp;
    //     // return range_query(str2BitArray(low), str2BitArray(high), p, 1, tmp);
    //     return range_query(low, high, p, 1, tmp);
    // }

    inline bool Rosetta::range_query(u64 low, u64 high)
    {
        return range_query(low, high, 0, 0);
    }

    inline bool Rosetta::range_query(u64 low, u64 high, u64 p, u64 l)
    {
        // printf("level = %lx\n", l);
        if (l == levels_ - 1) {
            l = levels_ - 1;
        }
        u64 base = 0;
        u64 move = (levels_ - l - 1) * alpha_;
        u64 end = pow(2, alpha_) - 1;
        for (int i = 0; i <= end; ++i, ++base) {
            u64 next = (l == 0 && i == end) ? UINT64_MAX : (((base + 1) << move) + p - 1);
            u64 cur = (base << move) + p;
            // printf("l = %lx, range_query: cur = %lx, next = %lx, low = %lx, high = %lx\n", l, cur, next, low, high);
            if (low > next) continue;
            if (cur > high) break;
            if (low <= cur && next <= high ) {
                if (doubt(cur, next, l))    return true;
                continue;
            }
            if (range_query(low, high, cur, l + 1)) {
                return true;
            }
        }
        return false;
    }

    inline bool Rosetta::doubt(u64 low, u64 high, u64 l)
    {
        // std::cout << "doubt:" << p << ' ' << l << std::endl;
        if (!bfs[l]->KeyMayMatch(low))
            return false;
        if (l == levels_ - 1) return true;
        u64 base = 0;
        u64 move = (levels_ - l - 2) * alpha_;
        u64 end = pow(2, alpha_) - 1;
        for (u32 i = 0; i <= end; i++, ++base) {
            u64 cur = low + (base << move);
            u64 next = ((base + 1) << move) + low - 1;
            if (doubt(cur, next, l + 1))
                return true;
        }
        return false;
    }

    // bool Rosetta::range_query(const std::string &low, const std::string &high, std::string &p, u64 l, std::string &min_accept)
    // {
    //     // std::cout << p << ' ' << l << std::endl;
    //     std::string pow_1;
    //     for (u32 i = 0; i < levels_ - l + 1; ++i)
    //         pow_1 += '1';

    //     std::string upper_bound = p.substr(0, levels_ - pow_1.size()) + pow_1;

    //     if ((p > high) || (low > upper_bound))
    //     {
    //         return false;
    //     }

    //     if ((p >= low) && (high >= upper_bound))
    //     {
    //         return doubt(p, l, min_accept);
    //     }

    //     if (range_query(low, high, p, l + 1, min_accept))
    //     {
    //         return true;
    //     }

    //     p[l - 1] = '1';
    //     return range_query(low, high, p, l + 1, min_accept);
    // }

    // bool Rosetta::doubt(std::string &p, u64 l, std::string &min_accept)
    // {
    //     // std::cout << "doubt:" << p << ' ' << l << std::endl;
    //     if (!bfs[l - 2]->KeyMayMatch(p.substr(0, l - 1)))
    //     {
    //         return false;
    //     }

    //     if (l > levels_)
    //     {
    //         min_accept = bitArray2Str(p);
    //         return true;
    //     }

    //     if (doubt(p, l + 1, min_accept))
    //     {
    //         return true;
    //     }
    //     p[l - 1] = '1';
    //     return doubt(p, l + 1, min_accept);
    // }

    // u64 Rosetta::seek(const u64 &key)
    // {
    //     u64 tmp = 0;
    //     return range_query(key, UINT64_MAX, tmp, 0, 1) ? tmp : 0;
    // }

    // std::string Rosetta::seek(const std::string &key)
    // {
    //     std::string p(levels_, '0');
    //     std::string tmp;

    //     // return range_query(str2BitArray(key), maxBitArray(), p, 1, tmp) ? tmp : "";
    //     return range_query(key, maxBitArray(), p, 1, tmp) ? tmp : "";
    // }
} // namespace elastic_rose
