// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

// Modified by Huanchen, 2018

#pragma once

#include <stdint.h>
#include <string.h>
#include <cmath>

#include <vector>
#include <string>
#include <assert.h>
#include <iostream>

#include "murmurHash3.h"

using namespace std;

namespace elastic_rose
{
    using u8 = unsigned char;
    using u16 = unsigned short;
    using u32 = unsigned int;
    using u64 = unsigned long;
    static void LeveldbBloomHash(const u64 key, u32 *out, u32 hash_code)
    {
        MurmurHash3_x86_128((const char *)(&key), sizeof(u64), hash_code, out);
    }

    static void LeveldbBloomHash(const string &key, u32 *out, u32 hash_code)
    {
        MurmurHash3_x86_128(key.c_str(), key.size(), hash_code, out);
    }

    inline uint32_t BloomHashId(const std::string &key, uint32_t id)
    {
        u32 ret = 0;
        switch (id)
        {
        case 0:
            LeveldbBloomHash(key, &ret, 0xbc9f1d34);
            return ret;
        case 1:
            LeveldbBloomHash(key, &ret, 0x34f1d34b);
            return ret;
        case 2:
            LeveldbBloomHash(key, &ret, 0x251d34bc);
            return ret;
        case 3:
            LeveldbBloomHash(key, &ret, 0x01d34bc9);
            return ret;
        case 4:
            LeveldbBloomHash(key, &ret, 0x1934bc9f);
            return ret;
        case 5:
            LeveldbBloomHash(key, &ret, 0x934bc9f1);
            return ret;
        case 6:
            LeveldbBloomHash(key, &ret, 0x4bc9f193);
            return ret;
        case 7:
            LeveldbBloomHash(key, &ret, 0x51c2578a);
            return ret;
        case 8:
            LeveldbBloomHash(key, &ret, 0xda23562f);
            return ret;
        case 9:
            LeveldbBloomHash(key, &ret, 0x135254f2);
            return ret;
        case 10:
            LeveldbBloomHash(key, &ret, 0xea1e4a48);
            return ret;
        case 11:
            LeveldbBloomHash(key, &ret, 0x567925f1);
            return ret;
        default:
            fprintf(stderr, "BloomHash id error\n");
            exit(1);
        }
    }

    // 基于空间大小m,和预期的假阳性率p,计算出最佳容纳的元素数n
    inline double calculate_n(u64 slot, double p) {
        // 使用公式 n = (m * ln(2)) / -ln(p)
        return (slot * log(2)) / -log(p);
    }

    class CountingBloomFilter
    {
    private:
        size_t bits_per_key_;
        size_t k_;
        size_t id_;
        size_t counter_size_ = 8; // 目前的实现都默认一个counter对应一个字节
        size_t max_counter_value_ = 255;
        size_t expect_num_;
        size_t insert_num_;
        std::vector<u8> filter_data_;
    public:
        CountingBloomFilter() = default;
        CountingBloomFilter(size_t id) : id_(id){};
        CountingBloomFilter(u64 total_size, double false_positive, u32 id)
            : id_(id), insert_num_(0)
        {
            filter_data_.resize(total_size, 0);
            expect_num_ = calculate_n(total_size * 8 / counter_size_, false_positive);
            bits_per_key_ = (total_size * 8 / counter_size_ / expect_num_);
            // We intentionally round down to reduce probing cost a little bit
            k_ = static_cast<size_t>(bits_per_key_ * 0.69); // 0.69 =~ ln(2)
            if (k_ < 1)
                k_ = 1;
            if (k_ > 30)
                k_ = 30;
        }

        size_t GetExpectNum()
        {
            return expect_num_;
        }

        size_t GetInsertNum()
        {
            return insert_num_;
        }

        size_t GetMaxCounterValue()
        {
            return max_counter_value_;
        }

        u64 getMemoryUsage()
        {
            return filter_data_.size();
        }

        // 返回false代表实际插入的键已远大于预期键的数量,或是存在计数器溢出，需要重构
        template<class T>
        bool PutKey(const T &key)
        {
            const size_t len = filter_data_.size();

            u8 *array = &(filter_data_)[0];
            const size_t bits = len * 8;
            // Use double-hashing to generate a sequence of hash values.
            // See analysis in [Kirsch,Mitzenmacher 2006].
            //u32 h = LeveldbBloomHash(keys[i]);
            u32 hbase[4];
            LeveldbBloomHash(key, hbase, id_);
            u32 h = hbase[0];
            const u32 delta = hbase[1];
            for (size_t j = 0; j < k_; j++)
            {
                const u32 bitpos = h % (bits / counter_size_);
                if (array[bitpos / (8 / counter_size_)] < max_counter_value_) {
                    (array[bitpos / (8 / counter_size_)])++;
                } else {
                    return false;
                }
                h += delta;
            }
            insert_num_++;
            if (insert_num_ > (expect_num_ * 2))    return false;
            return true;
        }

        template<class T>
        bool DeleteKey(const T &key)
        {
            const size_t len = filter_data_.size();

            u8 *array = &(filter_data_)[0];
            const size_t bits = len * 8;
            // Use double-hashing to generate a sequence of hash values.
            // See analysis in [Kirsch,Mitzenmacher 2006].
            //u32 h = LeveldbBloomHash(keys[i]);
            u32 hbase[4];
            LeveldbBloomHash(key, hbase, id_);
            u32 h = hbase[0];
            const u32 delta = hbase[1];
            for (size_t j = 0; j < k_; j++)
            {
                const u32 bitpos = h % (bits / counter_size_);
                (array[bitpos / (8 / counter_size_)])--;
                if (array[bitpos / (8 / counter_size_)] < 0) {
                    std::cout << "when delete key " << key << "counter < 0 !!" << std::endl;
                    assert(false);
                }
                h += delta;
            }
            insert_num_--;
            return true;
        }

        template<class T>
        bool KeyMayMatch(const T &key) const
        {
            const size_t len = filter_data_.size();
            if (len < 2)
                return false;

            const u8 *array = &filter_data_[0];
            const size_t bits = len * 8;

            u32 hbase[4];
            LeveldbBloomHash(key, hbase, id_);
            u32 h = hbase[0];
            const u32 delta = hbase[1];
            for (size_t j = 0; j < k_; j++)
            {
                const u32 bitpos = h % (bits / counter_size_);
                if ((array[bitpos / (8 / counter_size_)]) == 0)
                    return false;
                h += delta;
            }
            return true;
        }
    };

} // namespace elastic_rose
