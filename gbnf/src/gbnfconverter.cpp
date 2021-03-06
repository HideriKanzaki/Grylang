#include "gbnfconverter.hpp"

namespace gbnf{

class ConverterToBNF{
private:
    GbnfData& data;
    std::vector< GrammarRule > newRules;
    const bool preferRightRecursion = true; // For LR Pars0rz
    int verbosity = 0;

public:
    ConverterToBNF( GbnfData& _data, bool _preferRightRec = true, int _verbosity = 0 ) 
        : data( _data ), preferRightRecursion( _preferRightRec ), verbosity( _verbosity ) 
    {}

    void convert();

    short createNewRuleAndGetTag( GrammarToken&& rootToken, int recLevel = 0 );
    void fixNonBNFTokensInRule( const GrammarRule& rule, int recLevel = 0 );
};


/*! Converts a container-type token to a new grammar rule.
 * - Puts new rule to the newRules vector
 *  @param token - a group-type grammar token containing tokens to be parsed.
 *  @param recLevel - level of recursion. Default Zero.
 *  @return an ID of the NonTerminal which the newly created rule defines.
 */ 
short ConverterToBNF::createNewRuleAndGetTag( GrammarToken&& token, int recLevel ){
    // Backup current token's type
    auto tokType = token.type; 

    short nruleID = data.insertTag( "__tmp_bnfmode_"+
                        std::to_string( data.getLastTagID() + 1 ) );

    // Create a new rule to be added to newRules.
    // Assign the being-fixed token's children as new rule's Option no.1 .
    GrammarRule nrule( nruleID );
    nrule.options.push_back( 
        GrammarToken( GrammarToken::ROOT_TOKEN, 0, "", 
            std::move( token.children ) 
        )
    );

    // Binary Recursion !
    // Fix our new rule's options with "fixNonBNFTokensInRule".
    fixNonBNFTokensInRule( nrule, recLevel + 1 );

    // If original token, which we are currently fixing, is of repeatable type,
    // Add current rule's tag as an option, to support recursive repeat.
    if( tokType == GrammarToken::GROUP_REPEAT_NONE ||
        tokType == GrammarToken::GROUP_REPEAT_ONE )
    {
        // Check if only one option is left in the rule. If not, make another rule.
        if( nrule.options.size() > 1 ){
            short newTagID = data.insertTag( "__tmp_bnfmode_"+
                    std::to_string( data.getLastTagID() + 1 ) );

            // Make a new rule containing all option of the current rule.
            // The new rule defines a tag with ID of "newTagID".
            this->newRules.push_back( GrammarRule(
                newTagID,
                std::move( nrule.options )
            ) );
            
            // Now clear current rule's options, and add a single option - reference to
            // a new rule.
            nrule.options.clear();
            nrule.options.push_back( GrammarToken( GrammarToken::ROOT_TOKEN, 0, "",
                { GrammarToken( GrammarToken::TAG_ID, newTagID, std::string(), {} ) }
            ) );
        }

        // Create a repetition option (copy tokens in first option)
        GrammarToken reptok ( GrammarToken::ROOT_TOKEN, 0, std::string(), {} );
        reptok.children = nrule.options[ 0 ].children;

        // Insert a nonTerminal option with Current Rule's ID at end or beginning,
        // depending on parser type, to initiate recursive repetition. 
        if( preferRightRecursion ){ // If right recursion, add in the end.
            reptok.children.insert( reptok.children.end(), 
                GrammarToken( GrammarToken::TAG_ID, nrule.getID(), std::string(), {} )
            );

            //nrule.options.insert( nrule.options.end(), std::move( reptok ) );
        }
        else{ // If left recursion, add in the beginning.
            reptok.children.insert( reptok.children.begin(),
                GrammarToken( GrammarToken::TAG_ID, nrule.getID(), std::string(), {} )
            );

            //nrule.options.insert( nrule.options.begin(), std::move( reptok ) );
        }

        // Add repetition option to the rule.
        nrule.options.insert( nrule.options.end(), std::move( reptok ) );
    }

    // Push (move) this rule to newRules vector.
    this->newRules.push_back( std::move( nrule ) );

    // Return ID of this rule.
    return nrule.getID();
}

/*! Converts all Non-BNF tokens in a Rule to BNF tokens, creating new Rules if needed.
 *  - Uses Binary Recursion with createNewRuleAndGetTag to complete the feat.
 *  - Non-BNF tokens are Groups, which are of 4 types.
 *
 *    - GROUP_OPTIONAL and GROUP_REPEAT_NONE, which indicate optional tokens, are 
 *      getting a separate rule, and after that, are also being handled on this function, 
 *      where additional option is added to the Rule.
 *
 *    - GROUP_ONE and GROUP_REPEAT_ONE, which are non-optional, are just getting a
 *      separate rule, and the token in current Rule is being replaced by a tag of
 *      a new rule.
 *
 *  @param rule - a const reference to a GrammarRule being fixed.
 *      NOTE: reference is "const" because the ID of the rule must not be modified.
 *            However, we can modify the options of the rule (hence "fixing").
 *            The "options" vector is declared "mutable", to allow modification 
 *            even if the parent object is "const".
 *
 *            We use this technique as a little hack to allow Rules to be
 *            stored in a std::set.
 *
 *  @param recLevel - recursion level.
 */ 
void ConverterToBNF::fixNonBNFTokensInRule( const GrammarRule& rule, int recLevel ){
    // Loop through all the options of the rule, and 
    // check every token of every option, if it hasn't got more layers.
    const size_t optSize = rule.options.size();

    for( size_t oi = 0; oi < optSize; oi++ ){
        // Lvalue reference, because we don't want to move it. 
        auto& option = rule.options[ oi ]; 

        // Check first-level tokens of the current option.
        for( size_t i = 0; i < option.children.size(); i++ ){
            auto&& token = option.children[ i ];
            auto type = token.type;

            // Check if it's illegal ( A group ). If so, it needs replacement.
            if( type != GrammarToken::TAG_ID && 
                type != GrammarToken::REGEX_STRING )
            {
                // If only one child, and it's a leaf, just use it as repl. However, if
                // there are more than one, or if it's a tree, create a separate rule. 
                // TODO: Maybe dont't check anything.
                auto&& replacement = ( 
                    ( token.children.size() == 1 && 
                      token.children[ 0 ].children.empty() && (
                        type == GrammarToken::GROUP_ONE ||
                        type == GrammarToken::GROUP_OPTIONAL
                      )    
                    ) ?
                    // If only one child and it's a non-group type:
                    token.children[0] :
                    // If many childs, and/or it/they have group types:
                    GrammarToken( 
                        GrammarToken::TAG_ID, 
                        createNewRuleAndGetTag( std::move( token ), recLevel ),
                        "", {} 
                    )   
                );

                // Now create new options according to the type.
                // Replace current group-type element with generated tag-type 
                // element, referring to the newly created rule or element.
                option.children[ i ] = std::move( replacement );

                // If optional/repeatable, create a new option without current element.
                if( type == GrammarToken::GROUP_OPTIONAL || 
                    type == GrammarToken::GROUP_REPEAT_NONE )
                {
                    // Copy current option, but without a current element.
                    GrammarToken newOption( option );
                    newOption.children.erase( newOption.children.begin() + i );

                    // Push this new option to our rule.
                    rule.options.push_back( std::move( newOption ) );
                }
            }
        }
    }
}

void ConverterToBNF::convert(){
    // Search for rules which contain eBNF tokens.
    for( auto&& rule : data.grammarTableConst() ){
        // For each rule, pass it to this function, which modifies the rule, 
        // fixing Non-BNF tokens.
        fixNonBNFTokensInRule( rule );
    }

    // At the end, append newly constructed rules to grammar table.
    for( auto&& rl : newRules ){
        data.insertRule( std::move( rl ) );
    }

    // Sort rules by ID.
    data.sort();
}

//============= PUBLIC SECTION =============//

void convertToBNF( GbnfData& data, bool preferRightRecursion, int verbosity ){
    ConverterToBNF cbnf( data, preferRightRecursion, verbosity );
    cbnf.convert();
}

void fixRecursion( GbnfData& data, int recursionFixMode, int verbosity ){

}


/*void removeLeftRecursion( GbnfData& data ){

}

void fullyFixGBNF( GbnfData& data ){

} */

}

