/**
 *  @file
 *  Ethereum token standards ported to EOS
 */

#include <map>
#include <vector>

using namespace eosio;

class token_eos721 : public eosio::contract
{
    public:
    token_eos721(account_name self)
        : contract(self), _accounts(_self, _self), _allowances(_self, _self), _tokens(_self, _self) {}

    private:

    struct var 
    {            
        std::string key;       
        std::string value;        
    };

    //@abi table tokens i64 
    struct token 
    {
        uint64_t id;
        
        bool frozen;

        //user associations
        account_name owner;
        account_name issuer;        

        //data        
        std::vector<std::string> keys;
        std::vector<std::string> values;

        std::vector<var> vars;

        uint64_t primary_key() const { return id; }
        
        EOSLIB_SERIALIZE( token, (id)(frozen)(owner)(issuer)(keys)(values)(vars) )
    };

    eosio::multi_index<N(tokens), token> _tokens;

    //@abi table accounts i64 
    struct account
    {
        account_name owner;            
        uint64_t balance;

        uint64_t primary_key() const { return owner; }

        EOSLIB_SERIALIZE( account, (owner)(balance))
    };

    eosio::multi_index<N(accounts), account> _accounts;

    //@abi table allowances i64 
    struct allowance
    {
        uint64_t token_id;   
        account_name to;                         

        uint64_t primary_key() const { return token_id; }

        EOSLIB_SERIALIZE( allowance, (token_id)(to))
    };

    eosio::multi_index<N(allowances), allowance> _allowances;

    void transfer_balances(account_name from, account_name to, int64_t amount=1){
        if(from != 0){
            auto fromitr = _accounts.find(from);
            _accounts.modify(fromitr, 0, [&](auto &a){
                a.balance -= amount;
            });
        }

        if(to != 0){
            auto toitr = _accounts.find(to);

            if(toitr != _accounts.end()){
                _accounts.modify(toitr, 0, [&](auto &a){
                    a.balance += amount;
                });
            }else{
                _accounts.emplace(from, [&](auto &a) {
                    a.owner = to;
                    a.balance = amount;
                });
            }

            
        }
    }

    bool _owns(account_name claimant, uint64_t token_id){
        return owner_of(token_id) == claimant;
    }

    public:

    // Required methods
    uint64_t total_supply(){
        auto tokitr = _tokens.begin();
        uint64_t token_id = 0;
        while(tokitr != _tokens.end()){
            token_id++;
            tokitr++;
        }

        return token_id;
    }

    //returns balance of
    uint64_t balance_of(account_name owner) {
        auto account = _accounts.find(owner);
        return account->balance;
    }

    //returns who owns a token
    account_name owner_of(uint64_t token_id){
        auto token = _tokens.find(token_id);
        return token->owner;
    }

    void approve(account_name from, account_name to, uint64_t token_id){            
        require_auth(from);

        auto tokitr = _tokens.find(token_id);

        //check to see if approver owns the token
        if(tokitr == _tokens.end() || tokitr->owner != from ){            
            eosio_assert(false, "token does not exist");
        }

        auto allowanceitr = _allowances.find(token_id);
        if (allowanceitr == _allowances.end())
        {            
            _allowances.emplace(token_id, [&](auto &a) {                
                a.to = to;
                a.token_id = token_id;                 
            });
        }
        else
        {
            _allowances.modify(allowanceitr, 0, [&](auto &a) {
                a.to = to;                
            });
        }
    }

    void mint(account_name owner, vector<string> keys = {}, vector<string> values = {}, bool is_frozen=false){
        require_auth(_self);

        print("minting token to", name{owner});

        uint64_t token_id = total_supply() + 1;

        print("new token id", token_id);

        auto accountitr = _accounts.find(owner);

        if(accountitr == _accounts.end()){
            _accounts.emplace(_self, [&](auto &a){
                a.owner = owner;
                a.balance = 1;
            });
        }

        vector<var> vars;
        for(int i = 0; i<keys.size(); i++){
            vars.push_back(var {keys[i], values[i]} );
        }

        _tokens.emplace(_self, [&](auto &a) {
            a.owner = owner;
            a.issuer = _self;
            a.id = token_id;  
            a.frozen = is_frozen; 
            a.keys = keys;
            a.values = values;   
            a.vars = vars;                              
        });
    }

    void transfer(account_name sender, account_name to, uint64_t token_id){
        require_auth(sender);

        //find token
        auto tokenitr = _tokens.find(token_id);

        //make sure token exists and sender owns it
        if(tokenitr != _tokens.end() && tokenitr->owner == sender){
            //update token's owner
            _tokens.modify(tokenitr, 0, [&](auto &a){
                a.owner = to;
            });

            //increment/decrement balances 
            transfer_balances(sender, to);  
        } 
    }


    void transferfrom(account_name sender, account_name from, account_name to, uint64_t token_id){        
        require_auth(sender);

        //try to find allowance and token
        auto allowanceitr = _allowances.find(token_id);
        auto tokenitr = _tokens.find(token_id);

        //make sure the token/allowances for token exist and the users match
        if (tokenitr != _tokens.end() && tokenitr->owner == from &&
            allowanceitr != _allowances.end() && allowanceitr->to == sender)
        {
            _tokens.modify(tokenitr, 0, [&](auto &a){
                a.owner = to;
            });     
            transfer_balances(from, to);     
            _allowances.erase(allowanceitr);
        } 
    }     

    void burn(account_name burner, uint64_t token_id){
        require_auth(burner);
        auto tokenitr = _tokens.find(token_id);
        
        if(tokenitr != _tokens.end() && tokenitr->owner == burner){
            transfer_balances(burner, 0);     
            //_tokens.erase(tokenitr);
            _tokens.modify(tokenitr, 0, [&](auto &a){
                a.owner = 0;
            }); 
        }
    }

    void burnfrom(account_name burner, account_name from, uint64_t token_id){
        require_auth(burner);

        //try to find allowance and token
        auto allowanceitr = _allowances.find(token_id);
        auto tokenitr = _tokens.find(token_id);

        //make sure the token/allowances for token exist and the users match
        if (tokenitr != _tokens.end() && tokenitr->owner == from &&
            allowanceitr != _allowances.end() && allowanceitr->to == burner)
        {
            //_tokens.erase(tokenitr);    
            transfer_balances(from, 0);     
            //_allowances.erase(allowanceitr);
            _tokens.modify(tokenitr, 0, [&](auto &a){
                a.owner = 0;
            }); 
        }
    }
};

//typedef erc721<instrument_data> instrument;

EOSIO_ABI(token_eos721, (transfer)(mint)(approve)(transferfrom)(burn)(burnfrom) )


    
    
class token_eos20 : public eosio::contract
{
    public:
    token_eos20(account_name self)
        : contract(self), _accounts(_self, _self) {}

    private:

    //@abi table accounts i64 
    struct account
    {
        account_name owner;
        uint64_t balance;

        uint64_t primary_key() const { return owner; }

        EOSLIB_SERIALIZE( account, (owner)(balance) )
    };

    eosio::multi_index<N(accounts), account> _accounts;

    //@abi table allowances i64 
    struct allowance
    {        
        account_name to;
        uint64_t amount;
        
        uint64_t primary_key() const { return to; }

        EOSLIB_SERIALIZE( allowance, (to)(amount) )
    };

    typedef eosio::multi_index<N(allowances), allowance> _allowances;

    void add_balance(account_name payer, account_name to, uint64_t q)
    {
        auto toitr = _accounts.find(to);
        if (toitr == _accounts.end())
        {
            _accounts.emplace(payer, [&](auto &a) {
                a.owner = to;
                a.balance = q;
            });
        }
        else
        {
            _accounts.modify(toitr, 0, [&](auto &a) {
                a.balance += q;
                //eosio_assert(a.balance >= q, "overflow detected");
            });
        }
    }

    void set_allowance(account_name from, account_name to, uint64_t amount){
        account_name key = to;
        
        _allowances allowances(_self, from);

        auto allowanceitr = allowances.find(key);

        if (allowanceitr == allowances.end())
        {
            allowances.emplace(from, [&](auto &a) {                
                a.to = to;                
                a.amount = amount;
            });
        }
        else
        {            
            allowances.modify(allowanceitr, 0, [&](auto &a) {                                
                a.amount = amount;
            });
        }
    }

    public:
    uint64_t balance_of(account_name account){
        auto accountitr = _accounts.find(account);

        if(accountitr == _accounts.end()) return 0;

        return accountitr->balance;
    }

    uint64_t allowance_of(account_name from, account_name to){
        _allowances allowances(_self, from);

        auto allowanceitr = allowances.find(to);

        if(allowanceitr == allowances.end() ) return 0;

        return allowanceitr->amount;
    }

    void transfer(account_name from, account_name to, uint64_t quantity)
    {
        require_auth(from);

        const auto &fromacnt = _accounts.get(from);
        eosio_assert(fromacnt.balance >= quantity, "overdrawn balance");
        _accounts.modify(fromacnt, from, [&](auto &a) { a.balance -= quantity; });

        add_balance(from, to, quantity);
    }

    void mint(account_name to, uint64_t quantity)
    {
        require_auth(_self);

        add_balance(_self, to, quantity);
    }

    void approve(account_name from, account_name to, uint64_t amount)
    {
        require_auth(from);

        set_allowance(from, to, amount);
    }

    void transferfrom(account_name sender, account_name from, account_name to, uint64_t amount){
        require_auth(sender);

        auto allowance = allowance_of(from, sender);

        if(allowance < amount) eosio_assert(false, "overdrawn allowance");
        if(balance_of(from) < amount) eosio_assert(false, "overdrawn balance");;

        add_balance(sender, to, amount);
        add_balance(sender, from, -amount); 

        set_allowance(from, sender, allowance - amount);
    }

    void burnfrom(account_name sender, account_name from, uint64_t amount){
        require_auth(sender);

        auto allowance = allowance_of(from, sender);
        
        if(allowance < amount) eosio_assert(false, "overdrawn allowance");
        if(balance_of(from) < amount) eosio_assert(false, "overdrawn balance");

        add_balance(sender, from, -amount); 
        set_allowance(from, sender, allowance - amount);
    }

    void burn(account_name burner, uint64_t amount){
        require_auth(burner);
        const auto &fromacnt = _accounts.get(burner);
        if(fromacnt.balance < amount) eosio_assert(false, "overdrawn balance");
        _accounts.modify(fromacnt, burner, [&](auto &a) { a.balance -= amount; });        
    }


    
};

EOSIO_ABI(token_eos20, (transfer)(mint)(approve)(transferfrom)(burn)(burnfrom) )
