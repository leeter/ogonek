// Ogonek
//
// Written in 2012 by Martinho Fernandes <martinho.fernandes@gmail.com>
//
// To the extent possible under law, the author(s) have dedicated all copyright and related
// and neighboring rights to this software to the public domain worldwide. This software is
// distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along with this software.
// If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.

// Tests for <ogonek/segmentation.h++>

#include <ogonek/segmentation.h++>
#include <ogonek/types.h++>

#include <catch.h++>

#include <string>
#include <vector>

struct grapheme_cluster_test {
    std::u32string input;
    std::vector<int> breaks;
};

grapheme_cluster_test grapheme_cluster_test_data[] = {
    #include "grapheme_cluster_test.g.inl"
};

TEST_CASE("grapheme_clusters", "Extended grapheme cluster ranges") {
    for(auto&& test : grapheme_cluster_test_data) {
        auto clusters = ogonek::grapheme_clusters(test.input);
        auto it = clusters.begin();
        auto last_break = 0;
        for(auto&& this_break : test.breaks) {
            CHECK(it->begin() == test.input.begin() + last_break);
            CHECK(it->end()   == test.input.begin() + this_break);
            last_break = this_break;
            ++it;
        }
        CHECK(it == clusters.end());
    }
}
