/**
 *  @file
 *  Ethereum token standards ported to EOS
 */

#include <map>
#include <vector>

using namespace eosio;

template <class T> class erc721 : public eosio::contract
{
    public:
    erc721(account_name self)
        : contract(self), _accounts(_self, _self), _allowances(_self, _self), _tokens(_self, _self) {}

    private:

    struct var 
    {            
        uint64_t integer;
        float number;;
        std::string text;
    };

    struct token 
    {
        uint64_t id;
        uint64_t primary_key() const { return id; }

        bool frozen;

        //user associations
        account_name owner;
        account_name issuer;        

        //template class
        T metadata;    

        //custom key-value data store
        std::map<std::string, var> vars;
    };

    eosio::multi_index<N(tokens), token> _tokens;

    struct account
    {
        account_name owner;            
        uint64_t balance;

        std::string thing;            
        
        uint64_t primary_key() const { return owner; }
    };

    eosio::multi_index<N(accounts), account> _accounts;

    struct allowance
    {
        uint64_t token_id;   
        account_name to;                         

        uint64_t primary_key() const { return token_id; }
    };

    eosio::multi_index<N(allowances), allowance> _allowances;

    void transfer_balances(account_name from, account_name to, int64_t amount=1){
        auto fromitr = _accounts.find(from);
        _accounts.modify(fromitr, 0, [&](auto &a){
            a.balance -= amount;
        });

        auto toitr = _accounts.find(to);
        _accounts.modify(toitr, 0, [&](auto &a){
            a.balance += amount;
        });
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

    bool approve(account_name from, account_name to, uint64_t token_id){            
        require_auth(from);

        auto tokitr = _tokens.find(token_id);

        //check to see if approver owns the token
        if(tokitr == _tokens.end() || tokitr->owner != from ){
            return false;
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

        return true;
    }

    void mint(uint64_t owner, bool is_frozen=false){
        require_auth(_self);

        uint64_t token_id = total_supply() + 1;
       
        _tokens.emplace(token_id, [&](auto &a) {
            a.owner = owner;
            a.issuer = _self;
            a.id = token_id;  
            a.frozen = is_frozen;                                  
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


    void transfer_from(account_name sender, account_name from, account_name to, uint64_t token_id){        
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

    void burn_from(account_name burner, account_name from, uint64_t token_id){
        require_auth(burner);

        //try to find allowance and token
        auto allowanceitr = _allowances.find(token_id);
        auto tokenitr = _tokens.find(token_id);

        //make sure the token/allowances for token exist and the users match
        if (tokenitr != _tokens.end() && tokenitr->owner == from &&
            allowanceitr != _allowances.end() && allowanceitr->to == burner)
        {
            _tokens.erase(tokenitr);    
            transfer_balances(from, 0);     
            //_allowances.erase(allowanceitr);
            _tokens.modify(tokenitr, 0, [&](auto &a){
                a.owner = 0;
            }); 
        }
    }
   
};

EOSIO_ABI(erc721, (transfer)(mint)(approve)(transfer_from)(burn)(burn_from) )

class erc20 : public eosio::contract
{
    public:
    erc20(account_name self)
        : contract(self), _accounts(_self, _self), _allowances(_self, _self) {}

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

    private:

    struct account
    {
        account_name owner;
        uint64_t balance;

        uint64_t primary_key() const { return owner; }
    };

    eosio::multi_index<N(accounts), account> _accounts;

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

    public:
    uint64_t balance_of(account_name account){
        auto accountitr = _accounts.find(account);

        if(accountitr == _accounts.end()) return 0;

        return accountitr->balance;
    }

    private:
    struct allowance
    {
        account_name from;
        
        std::map<uint64_t, uint64_t> amounts;
        uint64_t primary_key() const { return from; }
    };

    eosio::multi_index<N(allowances), allowance> _allowances;

    public:

    uint64_t allowance_of(account_name from, account_name to){
        auto allowanceitr = _allowances.find(from);

        if(allowanceitr == _allowances.end() ) return 0;
        if( allowanceitr->amounts.find(to) == allowanceitr->amounts.end() ) return 0;

        return allowanceitr->amounts.at((uint64_t)(to));
    }

    bool transfer_from(account_name sender, account_name from, account_name to, uint64_t amount){
        require_auth(sender);

        auto allowance = allowance_of(from, sender);

        if(allowance < amount) return false;
        if(balance_of(from) < amount) return false;

        add_balance(sender, to, amount);
        add_balance(sender, from, -amount); 

        set_allowance(from, sender, allowance - amount);

        return true;
    }

    bool burn_from(account_name sender, account_name from, uint64_t amount){
        require_auth(sender);

        auto allowance = allowance_of(from, sender);
        
        if(allowance < amount) return false;
        if(balance_of(from) < amount) return false;

        add_balance(sender, from, -amount); 
        set_allowance(from, sender, allowance - amount);

        return true;
    }

    bool burn(account_name burner, uint64_t amount){
        require_auth(burner);
        const auto &fromacnt = _accounts.get(burner);
        if(fromacnt.balance < amount) return false;
        _accounts.modify(fromacnt, burner, [&](auto &a) { a.balance -= amount; });
        return true;
    }

    private:
    void set_allowance(account_name from, account_name to, uint64_t amount){
        auto allowanceitr = _allowances.find(from);
        if (allowanceitr == _allowances.end())
        {
            _allowances.emplace(from, [&](auto &a) {
                a.from = from;                
                a.amounts = std::map<uint64_t, uint64_t>();
                a.amounts[to] = amount;
            });
        }
        else
        {
            _allowances.modify(allowanceitr, 0, [&](auto &a) {                
                a.amounts[to] = amount;
            });
        }
    }


    
};

EOSIO_ABI(erc20, (transfer)(mint)(approve)(transfer_from)(burn)(burn_from) )
