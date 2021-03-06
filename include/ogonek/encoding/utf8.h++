// Ogonek
//
// Written in 2012-2013 by Martinho Fernandes <martinho.fernandes@gmail.com>
//
// To the extent possible under law, the author(s) have dedicated all copyright and related
// and neighboring rights to this software to the public domain worldwide. This software is
// distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along with this software.
// If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.

// UTF-8 encoding form

#ifndef OGONEK_ENCODING_UTF8_HPP
#define OGONEK_ENCODING_UTF8_HPP

#include <ogonek/encoding/iterator.h++>
#include <ogonek/types.h++>
#include <ogonek/error.h++>
#include <ogonek/error/unicode_error.h++>
#include <ogonek/detail/ranges.h++>
#include <ogonek/detail/constants.h++>
#include <ogonek/detail/container/partial_array.h++>
#include <ogonek/detail/container/encoded_character.h++>

#include <taussig/primitives.h++>

#include <boost/range/iterator_range.hpp>
#include <boost/range/sub_range.hpp>
#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <boost/range/empty.hpp>

#include <array>
#include <utility>

namespace ogonek {
    struct utf8 {
    private:
        static constexpr auto last_1byte_value = 0x7Fu;
        static constexpr auto last_2byte_value = 0x7FFu;
        static constexpr auto last_3byte_value = 0xFFFFu;

        static constexpr auto start_2byte_mask = 0x80u;
        static constexpr auto start_3byte_mask = 0xE0u;
        static constexpr auto start_4byte_mask = 0xF0u;

        static constexpr auto continuation_mask = 0xC0u;
        static constexpr auto continuation_signature = 0x80u;

        static constexpr int sequence_length(byte b) {
            return (b & start_2byte_mask) == 0? 1
                 : (b & start_3byte_mask) != start_3byte_mask? 2
                 : (b & start_4byte_mask) != start_4byte_mask? 3
                 : 4;
        }

        static constexpr code_point decode(byte b0, byte b1) {
            return ((b0 & 0x1F) << 6) | (b1 & 0x3F);
        }
        static constexpr code_point decode(byte b0, byte b1, byte b2) {
            return ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
        }
        static constexpr code_point decode(byte b0, byte b1, byte b2, byte b3) {
            return ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
        }

    public:
        using code_unit = char;
        static constexpr bool is_fixed_width = false;
        static constexpr std::size_t max_width = 4;
        static constexpr bool is_self_synchronizing = true;
        struct state {};

        template <typename ErrorHandler>
        static detail::encoded_character<utf8> encode_one(code_point u, state&, ErrorHandler) {
            if(u <= last_1byte_value) {
                return { static_cast<code_unit>(u) };
            } else if(u <= last_2byte_value) {
                return {
                    static_cast<code_unit>(0xC0 | ((u & 0x7C0) >> 6)),
                    static_cast<code_unit>(0x80 | (u & 0x3F)),
                };
            } else if(u <= last_3byte_value) {
                return {
                    static_cast<code_unit>(0xE0 | ((u & 0xF000) >> 12)),
                    static_cast<code_unit>(0x80 | ((u & 0xFC0) >> 6)),
                    static_cast<code_unit>(0x80 | (u & 0x3F)),
                };
            }
            return {
                static_cast<code_unit>(0xF0 | ((u & 0x1C0000) >> 18)),
                static_cast<code_unit>(0x80 | ((u & 0x3F000) >> 12)),
                static_cast<code_unit>(0x80 | ((u & 0xFC0) >> 6)),
                static_cast<code_unit>(0x80 | (u & 0x3F)),
            };
        }
        template <typename Range>
        static boost::sub_range<Range> decode_one(Range const& r, code_point& out, state&, assume_valid_t) {
            auto first = boost::begin(r);
            byte b0 = *first++;
            auto length = sequence_length(b0);
            if(length == 1) {
                out = b0;
                return { first, boost::end(r) };
            }
            byte b1 = *first++;
            if(length == 2) {
                out = decode(b0, b1);
                return { first, boost::end(r) };
            }
            byte b2 = *first++;
            if(length == 3) {
                out = decode(b0, b1, b2);
                return { first, boost::end(r) };
            }
            byte b3 = *first++;
            out = decode(b0, b1, b2, b3);
            return { first, boost::end(r) };
        }
        template <typename Range, typename ErrorHandler>
        static boost::sub_range<Range> decode_one(Range const& r, code_point& out, state& s, ErrorHandler) {
            auto first = boost::begin(r);
            byte b0 = *first++;
            auto length = sequence_length(b0);

            if(length == 1) {
                out = b0;
                return { first, boost::end(r) };
            }

            auto is_invalid = [](byte b) { return b == 0xC0 || b == 0xC1 || b > 0xF4; };
            auto is_continuation = [](byte b) {
                return (b & continuation_mask) == continuation_signature;
            };

            if(is_invalid(b0) || is_continuation(b0)) {
                return ErrorHandler::template apply_decode<utf8>(r, s, out);
            }

            std::array<byte, 4> b = {{ b0, }};
            for(int i = 1; i < length; ++i) {
                b[i] = *first++;
                if(!is_continuation(b[i])) {
                    return ErrorHandler::template apply_decode<utf8>(r, s, out);
                }
            }

            if(length == 2) {
                out = decode(b[0], b[1]);
            } else if(length == 3) {
                out = decode(b[0], b[1], b[2]);
            } else {
                out = decode(b[0], b[1], b[2], b[3]);
            }

            auto is_overlong = [](code_point u, int bytes) {
                return u <= last_1byte_value
                    || (u <= last_2byte_value && bytes > 2)
                    || (u <= last_3byte_value && bytes > 3);
            };
            if(is_overlong(out, length)) {
                return ErrorHandler::template apply_decode<utf8>(r, s, out);
            }
            if(detail::is_surrogate(out) || out > detail::last_code_point) {
                return ErrorHandler::template apply_decode<utf8>(r, s, out);
            }
            return { first, boost::end(r) };
        }
        template <typename Sequence>
        static std::pair<Sequence, code_point> decode_one_ex(Sequence s, state&, assume_valid_t) {
            byte b0 = seq::front(s);
            seq::pop_front(s);
            auto length = sequence_length(b0);

            if(length == 1) {
                return { s, b0 };
            }

            std::array<byte, 4> b = {{ b0, }};
            for(int i = 1; i < length; ++i) {
                b[i] = seq::front(s);
                seq::pop_front(s);
            }

            code_point decoded;
            if(length == 2) {
                decoded = decode(b[0], b[1]);
            } else if(length == 3) {
                decoded = decode(b[0], b[1], b[2]);
            } else {
                decoded = decode(b[0], b[1], b[2], b[3]);
            }

            return { s, decoded };
        }
        template <typename Sequence, typename ErrorHandler>
        static std::pair<Sequence, code_point> decode_one_ex(Sequence s, state& state, ErrorHandler handler) {
            byte b0 = seq::front(s);
            seq::pop_front(s);
            auto length = sequence_length(b0);

            if(length == 1) {
                return { s, b0 };
            }

            auto is_invalid = [](byte b) { return b == 0xC0 || b == 0xC1 || b > 0xF4; };
            auto is_continuation = [](byte b) {
                return (b & continuation_mask) == continuation_signature;
            };

            if(is_invalid(b0) || is_continuation(b0)) {
                decode_error<Sequence, utf8> error { s, state };
                wheels::optional<code_point> u;
                std::tie(s, state, u) = handler.handle(error);
                return { s, *u };
            }

            std::array<byte, 4> b = {{ b0, }};
            for(int i = 1; i < length; ++i) {
                b[i] = seq::front(s);
                if(!is_continuation(b[i])) {
                    decode_error<Sequence, utf8> error { s, state };
                    wheels::optional<code_point> u;
                    std::tie(s, state, u) = handler.handle(error);
                    return { s, *u };
                }
                seq::pop_front(s);
            }

            code_point decoded;
            if(length == 2) {
                decoded = decode(b[0], b[1]);
            } else if(length == 3) {
                decoded = decode(b[0], b[1], b[2]);
            } else {
                decoded = decode(b[0], b[1], b[2], b[3]);
            }

            auto is_overlong = [](code_point u, int bytes) {
                return u <= last_1byte_value
                    || (u <= last_2byte_value && bytes > 2)
                    || (u <= last_3byte_value && bytes > 3);
            };
            if(is_overlong(decoded, length)) {
                decode_error<Sequence, utf8> error { s, state };
                wheels::optional<code_point> u;
                std::tie(s, state, u) = handler.handle(error);
                return { s, *u };
            }
            if(detail::is_surrogate(decoded) || decoded > detail::last_code_point) {
                decode_error<Sequence, utf8> error { s, state };
                wheels::optional<code_point> u;
                std::tie(s, state, u) = handler.handle(error);
                return { s, *u };
            }
            return { s, decoded };
        }
    };
} // namespace ogonek

#endif // OGONEK_ENCODING_UTF8_HPP

