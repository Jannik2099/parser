/**
 *   Copyright (C) 2017 Zach Laine
 *
 *   Distributed under the Boost Software License, Version 1.0. (See accompanying
 *   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#ifndef YAML_PARSER_BASIC_STRUCTURES_DEF_HPP
#define YAML_PARSER_BASIC_STRUCTURES_DEF_HPP

#include <yaml/parser/basic_structures.hpp>

#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/phoenix/object/construct.hpp>


namespace yaml { namespace parser {

    namespace detail {

        struct check_yaml_version
        {
            template <typename, typename>
            struct result { using type = void; };

            template <typename Pass>
            void operator() (
                unsigned int major,
                unsigned int minor,
                error_handler_t const & error_handler,
                Pass & pass
            ) const {
                if (major != 1) {
                    std::stringstream oss;
                    oss << "The current document has a %YAML "
                        << major << '.' << minor
                        << " directive.  This parser recognizes "
                           "YAML 1.2, and so cannot continue.\n";
                    error_handler.report_error(oss.str());
                    pass = false;
                } else if (minor != 2) {
                    std::stringstream oss;
                    oss << "The current document has a %YAML "
                        << major << '.' << minor
                        << " directive.  This parser recognizes "
                           "YAML 1.2, and so might not work.  "
                           "Trying anyway...\n";
                    error_handler.report_warning(oss.str());
                }
            }
        };

    }

    template <typename Iterator>
    basic_structures<Iterator>::basic_structures (boost::phoenix::function<error_handler_t> const & error_handler)
        : error_handler_ (error_handler)
    {
        qi::attr_type attr;
        qi::uint_type uint_;
        qi::char_type char_;
        qi::_val_type _val;
        qi::_1_type _1;
        qi::_r1_type _r1;
        qi::_r2_type _r2;
        qi::_a_type _a;
        qi::_b_type _b;
        qi::lit_type lit;
        qi::blank_type blank;
        qi::alnum_type alnum;
        qi::eol_type eol;
        qi::eps_type eps;
        qi::repeat_type repeat;
        qi::_pass_type _pass;

        namespace phx = boost::phoenix;
        using qi::copy;
        using phx::function;
        using phx::construct;
        using phx::cref;

        auto & nb_char = characters_.nb_char;
        auto & ns_char = characters_.ns_char;
        auto & tag_char = characters_.tag_char;
        auto & uri_char = characters_.uri_char;

        function<detail::check_yaml_version> check_yaml_version;

        // 6.1. Indentation Spaces

        // [63]
        indent =
            repeat(_r1)[lit(' ')]
            ;

        // [64]
        indent_lt =
            repeat(0, _r1 - 1)[lit(' ')]
            ;

        // [65]
        indent_le =
            repeat(0, _r1)[lit(' ')]
            ;

        // 6.2. Separation Spaces

        // [66]
        separate_in_line =
            *blank             // Should properly be +blank|start-of-line
            ;

        // 6.3. Line Prefixes

        // [67]
        line_prefix =
                indent(_r1)
            >>  eps(_r2 == context_t::flow_in || _r2 == context_t::flow_out) >> -separate(_r1, _r2)
            ;

        // 6.4 Empty Lines

        // [70]
        l_empty =
                (line_prefix(_r1, _r2) | indent_lt(_r1))
            >>  eol
            ;

        // 6.5 Line Folding

        // [73]
        b_l_folded = (
                eol >> +l_empty(_r1, _r2)   // b-l-trimmed [71]
            |   eol
            )
            >>  attr('\n')
            ;

        // [74]
        flow_folded =
                -separate_in_line
            >>  b_l_folded(_r1, context_t::flow_in)
            >>  line_prefix(_r1, context_t::flow_in)
            ;

        // 6.6 Comments

        // [75]
        comment_text =
            '#' >> +nb_char
            ;

        // [77]
        s_b_comment =
                -(separate_in_line >> -comment_text)
            >>  eol                                   // b-comment [77]
            ;

        // [78]
        l_comment =
            separate_in_line >> -comment_text >> eol
            ;

        // [79]
        s_l_comments =
                -s_b_comment  // Should properly be s_b_comment|start-of-line
            >>  *l_comment
            ;

        // 6.7 Separation Lines

        // [80]
        separate =
                eps(_r2 == context_t::block_key || _r2 == context_t::flow_key) >> separate_in_line
            |   separate_lines(_r1)
            ;

        // [81]
        separate_lines =
                s_l_comments
            >>  indent(_r1) >> -separate_in_line   // flow-line-prefix [69]
            |   separate_in_line
            ;

        // 6.8 Directives

        // [82]
        directive =
                '%'
            >>  (yaml_directive | tag_directive | reserved_directive)
            >>  s_l_comments
            ;

        // [83]
        reserved_directive =
                +ns_char
            >>  *(+blank >> +ns_char)
            ;

        // [86]
        yaml_directive =
                "YAML"
            >   +blank
            >   uint_[_a = _1]
            >   '.'
            >   uint_[check_yaml_version(_a, _1, phx::cref(error_handler_.get().f), _pass)]
            ;

        // [88]
        tag_directive =
            "TAG" > +blank > tag_handle > +blank > tag_prefix
            // TODO [add tag to tag symbol table]
            ;

        // [89]
        tag_handle =
                // "alnum..." below is  word_char [38]
                '!' >> +(alnum | char_("-")) >> '!' // named_tag_handle [92] (must match existing TAG-defined prefix)
            |   "!!"                                // secondary_tag_handle [91]
            |   '!'                                 // primary_tag_handle [90]
            ; // TODO: Check that nonempty handle matches existing TAG prefix (or better yet, use a symbol table)

        // [93]
        tag_prefix =
                '!' >> *uri_char
            |   tag_char >> *uri_char
            ;

        // 6.9 Node Properties

        // [96]
        properties = (
                tag_property[_a = _1] >> -(separate(_r1, _r2) >> anchor_property[_b = *_1])
            |   anchor_property[_b = _1] >> -(separate(_r1, _r2) >> tag_property[_a = *_1])
            )
            [_val = construct<ast::properties_t>(_a, _b)]
            ;

        // [97]
        tag_property %=
                lit('!') >> "<" > +uri_char > ">"   // verbatim_tag [98]
            |   tag_handle >> +tag_char             // shorthand_tag [99]
            |   '!'                                 // non_specific_tag [100]
            ;

        // [22]
        auto indicator = char_("-?:,[]{}#&*!|>'\"%@`");

        // [101]
        // TODO: The spec says flow-indicator (",[]{}") here, not indicator.
        // Document this.  Reference test 2SXE.yml:
        // Before:
//&a : key: &a value
// foo:
//  *a:
        // After:
//&a: key: &a value
//foo:
//  *a:
        anchor_property =
            '&' >> +(ns_char - indicator)
            ;

        // [102]
        // auto anchor_char = ns_char - char_(",[]{}");

        // [103]
        anchor_name =
            +(ns_char - char_(",[]{}"))
            ;

        BOOST_SPIRIT_DEBUG_NODES(
#if 0
            (indent)
            (indent_lt)
            (indent_le)
            (separate_in_line)
            (line_prefix)
            (l_empty)
#endif
            (b_l_folded)
            (flow_folded)
#if 0
            (comment_text)
            (s_b_comment)
            (l_comment)
            (s_l_comments)
            (separate)
            (separate_lines)
#endif
            (directive)
            (reserved_directive)
            (yaml_directive)
            (tag_directive)
            (tag_handle)
            (tag_prefix)
            (properties)
            (tag_property)
            (anchor_property)
            (anchor_name)
        );
    }

} }

#endif