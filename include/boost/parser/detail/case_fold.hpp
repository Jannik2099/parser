#ifndef BOOST_PARSER_DETAIL_CASE_FOLD_HPP
#define BOOST_PARSER_DETAIL_CASE_FOLD_HPP

#include <boost/parser/config.hpp>
#include <boost/parser/detail/case_fold_data_generated.hpp>

#include <algorithm>


namespace boost::parser::detail {

    inline std::optional<char32_t *> do_short_mapping(
        short_mapping_range const * first,
        short_mapping_range const * last,
        char32_t cp,
        char32_t * out)
    {
        auto it = std::lower_bound(
            first,
            last,
            cp,
            [](short_mapping_range const & range, char32_t cp) {
                return range.cp_first_ < cp;
            });
        if (it != first) {
            auto const prev = it - 1;
            if (prev->cp_first_ <= cp && cp < prev->cp_last_)
                it = prev;
        }
        if (it != last && it->cp_first_ <= cp && cp < it->cp_last_) {
            auto const offset = cp - it->cp_first_;
            if (offset % it->stride_ == 0) {
                *out++ =
                    single_mapping_cps[it->first_idx_ + offset / it->stride_];
                return out;
            }
        }

        return std::nullopt;
    }

    inline char32_t * case_fold(char32_t cp, char32_t * out)
    {
        // One-byte fast path.
        if (cp < 0x100) {
            // ASCII letter fast path.
            if (0x41 <= cp && cp < 0x5a) {
                *out++ = cp + 0x20;
                return out;
            } else if (cp == 0x00DF) {
                // The lone multi-mapping below 0x100.
                *out++ = 0x0073;
                *out++ = 0x0073;
                return out;
            } else {
                // Skip [0x41, 0x5a), handled above.
                auto const first = std::begin(mapping_ranges) + 1;
                // 7th entry starts with 0x100.
                auto const last = std::begin(mapping_ranges) + 7;
                if (auto out_opt = do_short_mapping(first, last, cp, out))
                    return *out_opt;
            }
            *out++ = cp;
            return out;
        }

        // Single-cp-mapping path (next most common case).
        {
            auto const first = std::begin(mapping_ranges);
            auto const last = std::end(mapping_ranges);
            if (auto out_opt = do_short_mapping(first, last, cp, out))
                return *out_opt;
        }

        // Multi-cp mapping path.
        {
            auto const last = std::end(long_mappings);
            auto const it = std::lower_bound(
                std::begin(long_mappings),
                last,
                cp,
                [](long_mapping const & mapping, char32_t cp) {
                    return mapping.cp_ < cp;
                });
            if (it != last && it->cp_ == cp) {
#if BOOST_PARSER_USE_CONCEPTS
                return std::ranges::copy(it->mapping_, null_sentinel, out);
#else
                return std::copy(
                    it->mapping_,
                    std::find(
                        std::begin(it->mapping_), std::end(it->mapping_), 0),
                    out);
#endif
            }
        }

        *out++ = cp;
        return out;
    }

}

#endif
