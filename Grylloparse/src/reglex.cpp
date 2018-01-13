#include "reglex.hpp"
#include <iostream>
#include <map>

namespace gparse{

/*! Recursively collects the string segments into one Regex,
 *  traversing the grammar tokens.
 *  @param str - string to which to append all the collected stuff
 *  @param rule - starting GrammarRule,
 *  @param recLevel - level of recursion.
 *  @return true, if successfully collected a regex.
 */ 
static bool collectRegexStringFromGTokens_priv( const gbnf::GbnfData& data,
    std::string& str, const gbnf::GrammarRule& rule, std::set<int>& idStack,
    bool parentMultiOption = false, size_t recLevel = 0 )
{
    // First, check if we haven't came into a recursive loop.
    // Check if we have already processed a rule with an ID of "rule.getID()".
    if( idStack.find( (int)(rule.getID()) ) != idStack.end() ) 
        return false;
    else // Insert current ID as the one being processed.
        idStack.insert( (int)(rule.getID()) );

    // Check if there is only one option, with only one token, of type REGEX.
    // Don't use groups if so. Only Non-Recursive one-level tokens can be made so.
    if( rule.options.size() == 1 && rule.options[0].children.size() == 1 &&
        rule.options[0].children[0].type == gbnf::GrammarToken::REGEX_STRING && 
        !parentMultiOption )
    {
        str += rule.options[0].children[0].data; 
        return true;
    }

    // Regex group start - use Non-Capturing Groups.
    str += "(?:";

    // Loop all options of the rule and construct a regex.
    bool first = true;
    for( auto&& opt : rule.options ){
        // Add OR if not first option
        if(!first)
            str += "|";
        else
            first = false;

        // Loop all tokens of this option.
        for( auto&& token : opt.children ){
            // Now check the types of tokens, and concatenate regex.
            if( token.type == gbnf::GrammarToken::REGEX_STRING )
                str += token.data;

            else if( token.type == gbnf::GrammarToken::TAG_ID ){
                // Find rule which defines this tag, and launch a collector on that rule.
                auto&& iter = data.getRule( token.id );
                if( iter != data.grammarTableConst().end() ){
                    collectRegexStringFromGTokens_priv( data, str, *iter, idStack,
                           rule.options.size() > 1, recLevel+1 );
                }
            }
        }
    }

    // Regex group end.
    str += ")";

    // At the end, pop current rule's ID from the stack.
    idStack.erase( (int)(rule.getID()) );

    return true;
}

static inline bool setSpecialRegexRules( RegLexData& rl, 
    const SpecialTags& tags, int gRuleID, const std::string& regexString )
{
    // Find the regex whitespace rule. Assign special rule, and remove from table.
    if( regexIgnoreTag != (size_t)(-1) ){
        auto&& regWsIter = rl.rules.find( RegLexRule(regexIgnoreTag) );
        if( regWsIter != rl.rules.end() )
            rl.regexWhitespaces = *regWsIter; // Assign rule from the one on table.
        else
            throw std::runtime_error("[RegLexData(GbnfData)]: <delim_regex> rule is not present."); 
        //rl.rules.erase( RegLexRule(regexIgnoreTag) );
        rl.rules.erase( regWsIter );

        rl.useCustomWhitespaces = true; 
    } 
}

/*! Function checks if assigned GBNF-type grammar is supported.
 *  @throws an exception if grammar contain wrong rules/tokens.
 */ 
static inline void checkAndAssignLexicProperties( RegLexData& rl, 
        const gbnf::GbnfData& gdata, bool useStringRepresentations )
{
    // Find the specification declarations.
    std::map< std::string, size_t > specTags;
    
    // Tags to be ignored in the regex collection.
    std::set<int> ignoredTags;

    for( auto&& nt : gdata.tagTableConst() ){
        // Check if special tag. If yes, add to the specials map for later processing.
        for( auto&& it : RegLexData::SpecialTags ){
            if( it->second == nt.data )
                specTags.insert( std::pair<std::string, size_t>( it->first, nt.getID() ) );
        }
    }

    // If both regex and simple whitespaces are defined . . .  
    if( (regexIgnoreTag != (size_t)(-1) && ignoreTag != (size_t)(-1)) ){
        //throw std::runtime_error("[RegLexData(GbnfData)]: " \
        //    "<ignore> and <regex_ignore> tags are both defined.");
    }

    // --- Set Non-Regex (Simple) special rules --- //

    // If using delimiters (simple character array), just assign data string.
    // Only STRING type token can define non-regex delimiters.
    /*if( sDelimTag != (size_t)(-1) ){
        auto&& sDelimRule = gdata.getRule( sDelimTag );

        if( sDelimRule != gdata.grammarTableConst().end() &&
            sDelimRule->options.size() == 1 )
        {
            rl.nonRegexDelimiters = sDelimRule->options[0].children[0].data;
            ignoredTags.insert( sDelimTag );
            std::cout<<"Simple Delim Rule: \""<< rl.nonRegexDelimiters <<"\"\n"; 
        }
        else
            throw std::runtime_error("[RegLexData(GbnfData)]: <delim> rule is not present."); 
        rl.tokenized = true;
    }*/

    // If ignorable tag was found, assign ignoreables string.
    // Ignorables can't be regex by nature, it's a list of whitespace chars.
    /*if( ignoreTag != (size_t)(-1) ){
        auto&& ignoreRule = gdata.getRule( ignoreTag );

        if( ignoreRule != gdata.grammarTableConst().end() &&
            ignoreRule->options.size() == 1 )
        {
            rl.whitespaces = ignoreRule->options[0].children[0].data;
            ignoredTags.insert( ignoreTag );
            std::cout<<"Ignoreable Rule: \""<< rl.nonRegexDelimiters <<"\"\n"; 
        }
        else
            throw std::runtime_error("[RegLexData(GbnfData)]: <ignore> rule is not present."); 
        rl.useCustomWhitespaces = true;
    }*/

    // Collect the regexes for each rule.
    // Construct a final regex, which will be used in lexer-tokenizing the language.
    for( auto&& rule : gdata.grammarTableConst() ){
        std::string regstr;
        if( collectRegexStringFromGTokens_priv( gdata, regstr, rule, ignoredTags ) ){
            // Check if ID is special.
            if( 

            if( !useStringRepresentations )
                rl.rules.insert( RegLexRule(rule.getID(), std::regex( std::move(regstr) )) );
            else
                rl.rules.insert( RegLexRule( rule.getID(), std::regex( regstr ),  \
                                             std::move(regstr) ) );
        }
    }

    
}

/*! RegLexData constructor from GBNF grammar.
 */ 
RegLexData::RegLexData( const gbnf::GbnfData& data, bool useStringReprs ){
    checkAndAssignLexicProperties( *this, data, useStringReprs ); 
}

void RegLexData::print( std::ostream& os ) const {
    os << "RegLexData:\n";
    if(!nonRegexDelimiters.empty())
        os << " nonRegexDelimiters: "<< nonRegexDelimiters <<"\n";
    if(!whitespaces.empty())
        os << " whitespaces: "<< whitespaces <<"\n";
    if(useRegexWhitespaces || !regexWhitespaces.regexStringRepr.empty())
        os << " regexWhitespaces: "<< regexWhitespaces.regexStringRepr <<"\n"; 
     
    if(!rules.empty()){
        os<<" Rules:\n  ";
        for( auto&& a : rules ){
            os << a.getID() <<" -> "<< a.regexStringRepr <<"\n  ";
        }
    }
}

}

